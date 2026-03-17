#include "ns3/antenna-module.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/geocentric-constant-position-mobility-model.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-a3-rsrp-handover-algorithm.h"
#include "ns3/nr-gnb-rrc.h"
#include "ns3/nr-module.h"
#include "ns3/nr-ue-rrc.h"
#include "beam-link-budget.h"
#include "leo-ntn-handover-config.h"
#include "leo-orbit-calculator.h"
#include "leo-ntn-handover-reporting.h"
#include "leo-ntn-handover-runtime.h"
#include "leo-ntn-handover-update.h"
#include "leo-ntn-handover-utils.h"
#include "wgs84-hex-grid.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

/*
 * 文件说明：
 * `leo-ntn-handover-baseline.cc` 是当前毕设基础组的主仿真入口。
 *
 * 当前场景目标：
 * - 使用 5G-LENA/NR 搭建一个可解释的 LEO 多卫星、多 UE 切换基线；
 * - 用较简化但可控的轨道、波束和地面网格模型构造物理情境；
 * - 重点观察卫星/小区切换过程、切换成功率和业务连续性。
 *
 * 阅读建议：
 * 1. 先看“全局状态与配置镜像”；
 * 2. 再看“事件回调与切换辅助函数”；
 * 3. 最后看 `main()` 中的场景搭建、运行和汇总输出。
 */

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LeoDynamicHandoverDemo");

// ============================================================================
// 全局状态与配置镜像
// ============================================================================

// 当前仿真中的卫星运行时对象。
static std::vector<SatelliteRuntime> g_satellites;
// 当前仿真中的 UE 运行时对象。
static std::vector<UeRuntime> g_ues;
// 小区 ID 到卫星索引的映射，用于从 RRC 服务小区反查服务卫星。
static std::map<uint16_t, uint32_t> g_cellToSatellite;
// IMSI 到 UE 序号的映射，仅用于日志和统计。
static std::map<uint64_t, uint32_t> g_imsiToUe;

// 参考 UE 地面点，用于轨道自动对齐和默认几何计算。
static LeoOrbitCalculator::GroundPoint g_ueGroundPoint;
// 默认小区锚点地面位置；未启用六边形网格时，波束会指向该位置。
static LeoOrbitCalculator::GroundPoint g_cellGroundPoint;
// 仿真纪元的格林威治恒星时。
static double g_gmstAtEpochRad = 0.0;
// 最小可见仰角门限。
static double g_minElevationRad = LeoOrbitCalculator::DegToRad(10.0);
// 当前载波频率，供轨道几何和链路预算共用。
static double g_carrierFrequencyHz = 2e9;
// 简化波束与链路预算模型配置。
static BeamModelConfig g_beamModelConfig;
// 是否启用 WGS84 六边形网格作为地面锚点。
static bool g_useWgs84HexGrid = true;
// 是否将六边形网格中心锁定到 UE 所在区域。
static bool g_lockGridCenterToUe = true;
static double g_gridCenterLatitudeDeg = 45.6;
static double g_gridCenterLongitudeDeg = 84.9;
static double g_gridWidthKm = 400.0;
static double g_gridHeightKm = 400.0;
static double g_hexCellRadiusKm = 20.0;
// 每颗卫星用于锚点选择的最近网格候选数。
static uint32_t g_gridNearestK = 3;
static std::string g_outputDir = "scratch/results";
static bool g_printGridCatalog = true;
static std::string g_gridCatalogPath = JoinOutputPath(g_outputDir, "hex_grid_cells.csv");
// 预生成的六边形网格目录。
static std::vector<Wgs84HexGridCell> g_hexGridCells;

// 以下开关控制控制台输出详略程度。
static bool g_compactReport = true;
static bool g_printGridAnchorEvents = false;
static bool g_printKpiReports = false;
static bool g_printNrtEvents = false;
static bool g_printOrbitCheck = false;
static bool g_printRrcStateTransitions = false;
static double g_kpiIntervalSeconds = 1.0;
static bool g_printSimulationProgress = true;
static double g_progressReportIntervalSeconds = 2.0;
static double g_progressStopTimeSeconds = 0.0;
static double g_offeredPacketRatePerUe = 1000.0;
static double g_maxSupportedUesPerSatellite = 3.0;
static double g_loadCongestionThreshold = 0.8;

// 严格邻区守卫：只有满足可见、波束锁定和门限条件的邻区才会被激活。
static bool g_strictNrtGuard = true;
static double g_strictNrtMarginDb = 0.2;

// 下面三个状态量主要用于切换存在性检测和 A3 门控状态管理。
static bool g_seenAnyHandoverStart = false;
static bool g_seenAnyHandoverEndOk = false;
static bool g_handoverFrozen = false;
static std::vector<Ptr<NrGnbRrc>> g_gnbRrcs;
static std::vector<Ptr<NrA3RsrpHandoverAlgorithm>> g_hoAlgos;
static uint32_t g_a3GateServingSat = std::numeric_limits<uint32_t>::max();

// 多 UE 基线默认使用自定义 A3 执行器，而不是直接放开底层算法自行切换。
static bool g_useCustomA3Executor = true;
static double g_manualHoTttSeconds = 1.2;

// 逐时刻卫星链路预算导出文件。
static std::ofstream g_satBeamTrace;

// ============================================================================
// 运行时复位与事件回调
// ============================================================================

/**
 * 在新一轮仿真开始前，统一重置全局运行时状态。
 */
static void
ResetRuntimeState(uint32_t gNbNum)
{
    g_seenAnyHandoverStart = false;
    g_seenAnyHandoverEndOk = false;
    g_handoverFrozen = false;
    g_a3GateServingSat = std::numeric_limits<uint32_t>::max();
    for (auto& ue : g_ues)
    {
        ResetUeRuntime(ue, gNbNum);
    }
}

/**
 * 根据卫星当前位置更新其六边形网格锚点。
 *
 * 这个函数只在启用 WGS84 六边形网格时生效。
 */
static void
UpdateSatelliteAnchorFromGrid(uint32_t satIndex, const Vector& satEcef, double nowSeconds, bool forceLog)
{
    if (!g_useWgs84HexGrid || g_hexGridCells.empty() || satIndex >= g_satellites.size())
    {
        return;
    }

    auto& sat = g_satellites[satIndex];
    const auto anchor = ComputeGridAnchorSelection(g_hexGridCells, satEcef, g_gridNearestK);
    if (!anchor.found)
    {
        return;
    }

    sat.nearestGridIds = anchor.nearestGridIds;
    sat.cellAnchorEcef = anchor.anchorEcef;

    if (forceLog || (g_printGridAnchorEvents && sat.currentAnchorGridId != anchor.anchorGridId))
    {
        std::cout << "[GRID-ANCHOR] t=" << std::fixed << std::setprecision(3) << nowSeconds << "s"
                  << " sat" << satIndex
                  << " cell=" << sat.dev->GetCellId()
                  << " anchorGrid=" << anchor.anchorGridId
                  << " nearestK=" << FormatUintList(sat.nearestGridIds) << std::endl;
    }

    sat.currentAnchorGridId = anchor.anchorGridId;
}

static void
ReportStateTransition(std::string,
                      uint64_t imsi,
                      uint16_t cellId,
                      uint16_t rnti,
                      NrUeRrc::State oldState,
                      NrUeRrc::State newState)
{
    // 默认关闭 UE RRC 状态细节，只在深度排障时打开。
    if (!g_printRrcStateTransitions)
    {
        return;
    }

    if (oldState == newState)
    {
        return;
    }

    std::ostringstream prefix;
    if (const auto ueIdx = ResolveUeIndexFromImsi(g_imsiToUe, imsi))
    {
        prefix << " ue=" << *ueIdx;
    }

    std::cout << "[UE-RRC] t=" << std::fixed << std::setprecision(3)
              << Simulator::Now().GetSeconds() << "s"
              << prefix.str()
              << " cell=" << cellId
              << " state=" << static_cast<int>(oldState) << "->" << static_cast<int>(newState)
              << std::endl;
}

/**
 * gNB 侧切换开始回调。
 *
 * 这里除了打印日志，也会把“待闭合的切换”记入对应 UE 运行时对象，
 * 便于后续在 `HO-END-OK` 时计算执行时延。
 */
static void
ReportHandoverStart(std::string, uint64_t imsi, uint16_t cellId, uint16_t rnti, uint16_t targetCellId)
{
    g_seenAnyHandoverStart = true;
    std::ostringstream prefix;
    if (const auto ueIdx = ResolveUeIndexFromImsi(g_imsiToUe, imsi))
    {
        auto& ue = g_ues[*ueIdx];
        ue.hasPendingHoStart = true;
        ue.lastHoStartSourceCell = cellId;
        ue.lastHoStartTargetCell = targetCellId;
        ue.lastHoStartTimeSeconds = Simulator::Now().GetSeconds();
        ue.handoverStartCount++;
        prefix << " ue=" << *ueIdx;
    }

    std::cout << "[HO-START] t=" << std::fixed << std::setprecision(3)
              << Simulator::Now().GetSeconds() << "s"
              << prefix.str()
              << " sourceCell=" << cellId
              << " targetCell=" << targetCellId << std::endl;
}

/**
 * gNB 侧切换成功回调。
 *
 * 主要工作：
 * - 统计成功次数；
 * - 计算切换执行时延；
 * - 判断预测切换是否真实发生。
 */
static void
ReportHandoverEndOk(std::string, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
    g_seenAnyHandoverEndOk = true;
    std::ostringstream prefix;
    double handoverDelayMs = -1.0;
    if (const auto ueIdx = ResolveUeIndexFromImsi(g_imsiToUe, imsi))
    {
        auto& ue = g_ues[*ueIdx];
        ue.handoverEndOkCount++;
        if (ue.lastHoStartTimeSeconds >= 0.0)
        {
            const double delaySeconds = Simulator::Now().GetSeconds() - ue.lastHoStartTimeSeconds;
            ue.totalHandoverExecutionDelaySeconds += delaySeconds;
            handoverDelayMs = delaySeconds * 1000.0;
        }
        if (ue.hasPredictedHandover && g_satellites.size() > ue.expectedSourceIndex &&
            g_satellites.size() > ue.expectedTargetIndex)
        {
            const uint16_t expectedSourceCell = g_satellites[ue.expectedSourceIndex].dev->GetCellId();
            const uint16_t expectedTargetCell = g_satellites[ue.expectedTargetIndex].dev->GetCellId();
            if (ue.hasPendingHoStart && ue.lastHoStartSourceCell == expectedSourceCell &&
                ue.lastHoStartTargetCell == expectedTargetCell && cellId == expectedTargetCell)
            {
                ue.seenExpectedHandover = true;
                if (!g_useCustomA3Executor && !g_handoverFrozen)
                {
                    for (const auto& rrc : g_gnbRrcs)
                    {
                        rrc->SetNrHandoverManagementSapProvider(nullptr);
                    }
                    g_handoverFrozen = true;
                    g_a3GateServingSat = std::numeric_limits<uint32_t>::max();
                    std::cout << "[HO] freeze automatic handover after predicted success" << std::endl;
                }
            }
        }
        ue.hasPendingHoStart = false;
        ue.lastHoStartTimeSeconds = -1.0;
        prefix << " ue=" << *ueIdx;
    }

    std::cout << "[HO-END-OK] t=" << std::fixed << std::setprecision(3)
              << Simulator::Now().GetSeconds() << "s"
              << prefix.str()
              << " targetCell=" << cellId;
    if (handoverDelayMs >= 0.0)
    {
        std::cout << " delay=" << std::fixed << std::setprecision(3) << handoverDelayMs << "ms";
    }
    std::cout << std::endl;
}

// ============================================================================
// 预测与辅助逻辑
// ============================================================================

/**
 * 使用“离线 A3 代理”预测第一次切换时刻。
 *
 * 这个预测只用于初始化阶段的参考输出，不直接驱动真实仿真中的切换行为。
 */
static std::optional<PredictedHandover>
PredictFirstHandoverA3Proxy(uint32_t initialAttachIdx,
                            double simTimeSeconds,
                            double stepSeconds,
                            double hysteresisDb,
                            double tttSeconds,
                            const LeoOrbitCalculator::GroundPoint& ueGroundPoint)
{
    if (g_satellites.empty() || initialAttachIdx >= g_satellites.size() || simTimeSeconds <= 0.0)
    {
        return std::nullopt;
    }

    const double dt = std::max(0.01, stepSeconds);
    uint32_t servingIdx = initialAttachIdx;
    uint32_t candidateIdx = std::numeric_limits<uint32_t>::max();
    double candidateSince = 0.0;

    for (double t = 0.0; t <= simTimeSeconds + 1e-9; t += dt)
    {
        std::vector<LeoOrbitCalculator::OrbitState> states(g_satellites.size());
        std::vector<BeamLinkBudget> budgets(g_satellites.size());

        for (uint32_t i = 0; i < g_satellites.size(); ++i)
        {
            states[i] = LeoOrbitCalculator::Calculate(t,
                                                      g_satellites[i].orbit,
                                                      g_gmstAtEpochRad,
                                                      ueGroundPoint,
                                                      g_carrierFrequencyHz,
                                                      g_minElevationRad);

            const Vector anchorEcef = ResolvePredictiveAnchorEcef(states[i].ecef,
                                                                   g_satellites[i].cellAnchorEcef,
                                                                   g_useWgs84HexGrid,
                                                                   g_hexGridCells);

            budgets[i] = CalculateEarthFixedBeamBudget(states[i].ecef,
                                                       ueGroundPoint.ecef,
                                                       anchorEcef,
                                                       g_beamModelConfig);
        }

        if (servingIdx >= g_satellites.size())
        {
            return std::nullopt;
        }

        const bool servingUsable = states[servingIdx].visible && budgets[servingIdx].beamLocked &&
                                   std::isfinite(budgets[servingIdx].rsrpDbm);
        double bestNeighbourRsrp = -std::numeric_limits<double>::infinity();
        const uint32_t bestNeighbourIdx = FindBestRsrpSatellite(
            &states,
            budgets,
            servingIdx,
            true,
            true,
            &bestNeighbourRsrp);

        bool a3Triggered = false;
        if (bestNeighbourIdx != std::numeric_limits<uint32_t>::max())
        {
            if (!servingUsable)
            {
                a3Triggered = true;
            }
            else
            {
                a3Triggered = (bestNeighbourRsrp > budgets[servingIdx].rsrpDbm + hysteresisDb);
            }
        }

        if (a3Triggered)
        {
            if (candidateIdx != bestNeighbourIdx)
            {
                // 目标星第一次持续优于服务星时，开始累计 TTT 计时。
                candidateIdx = bestNeighbourIdx;
                candidateSince = t;
            }

            if (t - candidateSince + 1e-9 >= tttSeconds)
            {
                PredictedHandover out;
                out.sourceIdx = initialAttachIdx;
                out.targetIdx = bestNeighbourIdx;
                out.triggerTimeSeconds = t;
                return out;
            }
        }
        else
        {
            candidateIdx = std::numeric_limits<uint32_t>::max();
            candidateSince = 0.0;
        }
    }

    return std::nullopt;
}

/**
 * 周期性打印总下行吞吐。
 *
 * 这是一个可选调试输出，主要用于观察业务连续性。
 */
static void
PrintDlThroughput(uint32_t packetSizeBytes, Time interval)
{
    const Time now = Simulator::Now();
    uint64_t totalRxPackets = 0;
    uint64_t deltaPackets = 0;
    for (auto& ue : g_ues)
    {
        if (!ue.server)
        {
            continue;
        }
        totalRxPackets += ue.server->GetReceived();
        const uint64_t currentRxPackets = ue.server->GetReceived();
        deltaPackets += currentRxPackets - ue.lastRxPackets;
        ue.lastRxPackets = currentRxPackets;
        ue.lastThroughputTime = now;
    }
    const double dt = interval.GetSeconds();
    const double throughputMbps = (dt > 0.0) ? (deltaPackets * packetSizeBytes * 8.0) / dt / 1e6 : 0.0;

    std::cout << std::fixed << std::setprecision(3)
              << "[DL] t=" << now.GetSeconds() << "s"
              << " throughput(total)=" << throughputMbps << " Mbps"
              << " rxPackets=" << totalRxPackets << std::endl;

    Simulator::Schedule(interval, &PrintDlThroughput, packetSizeBytes, interval);
}

/**
 * 周期性输出仿真当前推进到的模拟时间。
 */
static void
PrintSimulationProgress(Time interval)
{
    if (!g_printSimulationProgress || g_progressStopTimeSeconds <= 0.0)
    {
        return;
    }

    const double nowSeconds = Simulator::Now().GetSeconds();
    const double progressPercent =
        std::min(100.0, nowSeconds * 100.0 / std::max(1e-9, g_progressStopTimeSeconds));

    std::cout << "[Progress] t=" << std::fixed << std::setprecision(3) << nowSeconds << "s / "
              << g_progressStopTimeSeconds << "s"
              << " (" << std::setprecision(1) << progressPercent << "%)" << std::endl;

    if (Simulator::Now() + interval <= Seconds(g_progressStopTimeSeconds) + NanoSeconds(1))
    {
        Simulator::Schedule(interval, &PrintSimulationProgress, interval);
    }
}

/**
 * 控制底层 A3 provider 是否对指定服务卫星开放。
 *
 * 目前多 UE 场景主要走自定义 A3 执行器，这个函数更多用于保留单 UE 兼容路径。
 */
static void
ApplyA3Gate(uint32_t servingSatIdx, bool allowHandover)
{
    if (g_handoverFrozen || g_hoAlgos.size() != g_satellites.size())
    {
        return;
    }

    const uint32_t nextGateSat =
        allowHandover ? servingSatIdx : std::numeric_limits<uint32_t>::max();
    if (nextGateSat == g_a3GateServingSat)
    {
        return;
    }

    for (uint32_t i = 0; i < g_satellites.size(); ++i)
    {
        auto* provider = (nextGateSat == i && g_hoAlgos[i])
                             ? g_hoAlgos[i]->GetNrHandoverManagementSapProvider()
                             : nullptr;
        g_satellites[i].rrc->SetNrHandoverManagementSapProvider(provider);
    }
    g_a3GateServingSat = nextGateSat;

    if (!g_compactReport || g_printNrtEvents)
    {
        std::cout << "[A3-GATE] t=" << std::fixed << std::setprecision(3) << Simulator::Now().GetSeconds()
                  << "s mode=" << (nextGateSat == std::numeric_limits<uint32_t>::max() ? "CLOSED" : "OPEN");
        if (nextGateSat != std::numeric_limits<uint32_t>::max())
        {
            std::cout << " servingSat=sat" << nextGateSat
                      << " cell=" << g_satellites[nextGateSat].dev->GetCellId();
        }
        std::cout << std::endl;
    }
}

/**
 * 自定义 A3 风格切换执行器。
 *
 * 当目标邻星持续优于当前服务星达到 TTT 后，
 * 由这里主动向 serving gNB 发起切换请求。
 */
static void
ExecuteCustomA3Handover(UeRuntime& ue,
                        uint32_t ueIdx,
                        uint32_t servingSatIdx,
                        uint32_t bestNeighbourIdx,
                        bool neighbourQualified,
                        double nowSeconds)
{
    if (!g_useCustomA3Executor || servingSatIdx >= g_satellites.size())
    {
        return;
    }

    if (ue.manualHoServingIdx != servingSatIdx)
    {
        ue.manualHoServingIdx = servingSatIdx;
        ue.manualHoCandidateIdx = std::numeric_limits<uint32_t>::max();
        ue.manualHoCandidateSince = -1.0;
    }

    if (!neighbourQualified || bestNeighbourIdx == std::numeric_limits<uint32_t>::max())
    {
        ue.manualHoCandidateIdx = std::numeric_limits<uint32_t>::max();
        ue.manualHoCandidateSince = -1.0;
        return;
    }

    if (ue.hasPendingHoStart || (ue.manualHoLastTriggerTime >= 0.0 &&
                                 nowSeconds - ue.manualHoLastTriggerTime < 0.2))
    {
        return;
    }

    if (ue.manualHoCandidateIdx != bestNeighbourIdx)
    {
        // 候选目标发生变化时，重新开始计时，避免把不同目标的优势时间混算在一起。
        ue.manualHoCandidateIdx = bestNeighbourIdx;
        ue.manualHoCandidateSince = nowSeconds;
        return;
    }

    if (ue.manualHoCandidateSince < 0.0 ||
        nowSeconds - ue.manualHoCandidateSince + 1e-9 < g_manualHoTttSeconds)
    {
        return;
    }

    if (!ue.dev || !ue.dev->GetRrc())
    {
        return;
    }

    const uint16_t rnti = ue.dev->GetRrc()->GetRnti();
    if (rnti == 0)
    {
        return;
    }

    const uint16_t targetCellId = g_satellites[bestNeighbourIdx].dev->GetCellId();
    ue.hasPendingHoStart = true;
    ue.lastHoStartSourceCell = g_satellites[servingSatIdx].dev->GetCellId();
    ue.lastHoStartTargetCell = targetCellId;
    // 真正触发切换的动作仍交给 gNB RRC 完成。
    g_satellites[servingSatIdx].rrc->GetNrHandoverManagementSapUser()->TriggerHandover(rnti, targetCellId);
    ue.manualHoLastTriggerTime = nowSeconds;
    ue.manualHoCandidateIdx = std::numeric_limits<uint32_t>::max();
    ue.manualHoCandidateSince = -1.0;

    if (!g_compactReport || g_printNrtEvents)
    {
        std::cout << "[HO-MANUAL-TRIGGER] t=" << std::fixed << std::setprecision(3) << nowSeconds
                  << "s ue=" << ueIdx
                  << " sourceCell=" << ue.lastHoStartSourceCell
                  << " targetCell=" << targetCellId
                  << " ttt=" << g_manualHoTttSeconds << "s" << std::endl;
    }
}

// ============================================================================
// 周期更新主循环
// ============================================================================

/**
 * 周期更新整张星座和所有 UE 的观测状态。
 *
 * 这是整个场景的“时间推进核心”：
 * - 更新卫星位置；
 * - 计算每个 UE 到所有卫星的链路预算；
 * - 根据严格邻区守卫和自定义 A3 规则决定是否切换；
 * - 动态维护邻区集合。
 */
static void
UpdateConstellation(Time interval, Time stopTime)
{
    const double nowSeconds = Simulator::Now().GetSeconds();
    for (auto& sat : g_satellites)
    {
        sat.attachedUeCount = 0;
        sat.offeredPacketRate = 0.0;
        sat.loadScore = 0.0;
        sat.admissionAllowed = true;
    }

    std::vector<LeoOrbitCalculator::OrbitState> referenceStates(g_satellites.size());
    std::vector<BeamTraceRow> pendingBeamTraceRows;
    pendingBeamTraceRows.reserve(g_ues.size() * g_satellites.size());
    for (uint32_t i = 0; i < g_satellites.size(); ++i)
    {
        referenceStates[i] = LeoOrbitCalculator::Calculate(nowSeconds,
                                                            g_satellites[i].orbit,
                                                            g_gmstAtEpochRad,
                                                            g_ueGroundPoint,
                                                            g_carrierFrequencyHz,
                                                            g_minElevationRad);
        // 卫星节点位置直接用几何计算结果覆盖，形成时变轨道运动。
        g_satellites[i].node->GetObject<MobilityModel>()->SetPosition(referenceStates[i].ecef);
        UpdateSatelliteAnchorFromGrid(i, referenceStates[i].ecef, nowSeconds, false);
    }

    std::map<uint32_t, std::set<uint16_t>> desiredActiveNeighbours;
    for (uint32_t ueIdx = 0; ueIdx < g_ues.size(); ++ueIdx)
    {
        auto& ue = g_ues[ueIdx];
        UeObservationSnapshot observation = BuildUeObservationSnapshot(ue,
                                                                       ueIdx,
                                                                       nowSeconds,
                                                                       g_gmstAtEpochRad,
                                                                       g_carrierFrequencyHz,
                                                                       g_minElevationRad,
                                                                       g_beamModelConfig,
                                                                       g_satellites,
                                                                       g_cellToSatellite,
                                                                       g_satBeamTrace.is_open()
                                                                           ? &pendingBeamTraceRows
                                                                           : nullptr);
        if (observation.servingSatIdx != std::numeric_limits<uint32_t>::max())
        {
            g_satellites[observation.servingSatIdx].attachedUeCount++;
        }

        if (!g_compactReport)
        {
            PrintMobilitySnapshot(ue, ueIdx, nowSeconds, g_satellites, observation, g_beamModelConfig);
        }

        MaybePrintServingChangeAndKpi(
            ue, ueIdx, nowSeconds, g_kpiIntervalSeconds, g_printKpiReports, g_satellites, observation);

        if (observation.servingSatIdx != std::numeric_limits<uint32_t>::max())
        {
            observation.neighbourQualified =
                (observation.bestNeighbourIdx != std::numeric_limits<uint32_t>::max()) &&
                ((!observation.servingUsable) ||
                 (observation.bestNeighbourRsrp >
                  observation.beamBudgets[observation.servingSatIdx].rsrpDbm + g_strictNrtMarginDb));
            if (g_strictNrtGuard)
            {
                // 多 UE 路径优先使用自定义执行器；单 UE 时保留 A3 gate 兼容逻辑。
                if (g_useCustomA3Executor)
                {
                    ExecuteCustomA3Handover(
                        ue,
                        ueIdx,
                        observation.servingSatIdx,
                        observation.bestNeighbourIdx,
                        observation.neighbourQualified,
                        nowSeconds);
                }
                else if (g_ues.size() == 1)
                {
                    ApplyA3Gate(observation.servingSatIdx, observation.neighbourQualified);
                }
            }

            for (uint32_t j = 0; j < g_satellites.size(); ++j)
            {
                if (j == observation.servingSatIdx)
                {
                    continue;
                }

                bool shouldBeActive =
                    observation.states[j].visible && observation.beamBudgets[j].beamLocked;
                if (g_strictNrtGuard)
                {
                    shouldBeActive = shouldBeActive && observation.neighbourQualified &&
                                     (j == observation.bestNeighbourIdx);
                }
                if (shouldBeActive)
                {
                    desiredActiveNeighbours[observation.servingSatIdx].insert(
                        g_satellites[j].dev->GetCellId());
                }
            }
        }
        else if (g_strictNrtGuard)
        {
            if (!g_useCustomA3Executor && g_ues.size() == 1)
            {
                ApplyA3Gate(std::numeric_limits<uint32_t>::max(), false);
            }
            ue.manualHoServingIdx = std::numeric_limits<uint32_t>::max();
            ue.manualHoCandidateIdx = std::numeric_limits<uint32_t>::max();
            ue.manualHoCandidateSince = -1.0;
        }
    }

    UpdateSatelliteLoadStats(
        g_satellites, g_offeredPacketRatePerUe, g_maxSupportedUesPerSatellite, g_loadCongestionThreshold);
    FlushBeamTraceRows(g_satBeamTrace, pendingBeamTraceRows, g_satellites);
    ApplyDesiredActiveNeighbours(
        g_satellites, desiredActiveNeighbours, nowSeconds, g_compactReport, g_printNrtEvents);

    if (Simulator::Now() + interval <= stopTime)
    {
        Simulator::Schedule(interval, &UpdateConstellation, interval, stopTime);
    }
}

/**
 * 将解析后的配置同步到主脚本使用的全局镜像变量。
 *
 * 这些变量主要服务于周期更新、日志输出和离线预测逻辑。
 */
static void
ApplyGlobalMirrorConfig(const BaselineSimulationConfig& config)
{
    g_useWgs84HexGrid = config.useWgs84HexGrid;
    g_lockGridCenterToUe = config.lockGridCenterToUe;
    g_gridCenterLatitudeDeg = config.gridCenterLatitudeDeg;
    g_gridCenterLongitudeDeg = config.gridCenterLongitudeDeg;
    g_gridWidthKm = config.gridWidthKm;
    g_gridHeightKm = config.gridHeightKm;
    g_hexCellRadiusKm = config.hexCellRadiusKm;
    g_gridNearestK = config.gridNearestK;
    g_outputDir = config.outputDir;
    g_printGridCatalog = config.printGridCatalog;
    g_gridCatalogPath = config.gridCatalogPath;
    g_compactReport = config.compactReport;
    g_printGridAnchorEvents = config.printGridAnchorEvents;
    g_printKpiReports = config.printKpiReports;
    g_printNrtEvents = config.printNrtEvents;
    g_printOrbitCheck = config.printOrbitCheck;
    g_printRrcStateTransitions = config.printRrcStateTransitions;
    g_kpiIntervalSeconds = config.kpiIntervalSeconds;
    g_printSimulationProgress = config.printSimulationProgress;
    g_progressReportIntervalSeconds = config.progressReportIntervalSeconds;
    g_progressStopTimeSeconds = config.simTime;
    g_offeredPacketRatePerUe = config.lambda;
    g_maxSupportedUesPerSatellite = config.maxSupportedUesPerSatellite;
    g_loadCongestionThreshold = config.loadCongestionThreshold;
    g_strictNrtGuard = config.strictNrtGuard;
    g_strictNrtMarginDb = config.strictNrtMarginDb;
    g_manualHoTttSeconds = static_cast<double>(config.hoTttMs) / 1000.0;
}

// ============================================================================
// 主流程：参数、场景搭建、运行与结果汇总
// ============================================================================

int
main(int argc, char* argv[])
{
    BaselineSimulationConfig config;

    // 当前默认研究场景：8 颗卫星、2 个轨道面、25 个热点增加 + 边界增强的 UE。
    // 当前默认参数优先放大负载不均衡和边界竞争，用于后续负载平衡与 ping-pong 研究。
    CommandLine cmd(__FILE__);
    RegisterBaselineCommandLineOptions(cmd, config);
    cmd.Parse(argc, argv);

    const BaselineOutputPaths outputPaths = ResolveBaselineOutputPaths(config);
    ValidateBaselineSimulationConfig(config, g_useCustomA3Executor);

    auto& simTime = config.simTime;
    auto& appStartTime = config.appStartTime;
    auto& gNbNum = config.gNbNum;
    auto& ueNum = config.ueNum;
    auto& ueLayoutType = config.ueLayoutType;
    auto& ueSpacingMeters = config.ueSpacingMeters;
    auto& ueHotspotSpacingMeters = config.ueHotspotSpacingMeters;
    auto& ueBoundarySpacingMeters = config.ueBoundarySpacingMeters;
    auto& ueBoundaryOffsetMeters = config.ueBoundaryOffsetMeters;
    auto& ueBackgroundRadiusXMeters = config.ueBackgroundRadiusXMeters;
    auto& ueBackgroundRadiusYMeters = config.ueBackgroundRadiusYMeters;
    auto& ueHotspotCenterOffsetXMeters = config.ueHotspotCenterOffsetXMeters;
    auto& ueHotspotCenterOffsetYMeters = config.ueHotspotCenterOffsetYMeters;
    auto& satAltitudeMeters = config.satAltitudeMeters;
    auto& orbitEccentricity = config.orbitEccentricity;
    auto& orbitInclinationDeg = config.orbitInclinationDeg;
    auto& orbitRaanDeg = config.orbitRaanDeg;
    auto& orbitArgPerigeeDeg = config.orbitArgPerigeeDeg;
    auto& orbitPlaneCount = config.orbitPlaneCount;
    auto& interPlaneRaanSpacingDeg = config.interPlaneRaanSpacingDeg;
    auto& interPlaneTimeOffsetSeconds = config.interPlaneTimeOffsetSeconds;
    auto& baseTrueAnomalyDeg = config.baseTrueAnomalyDeg;
    auto& gmstAtEpochDeg = config.gmstAtEpochDeg;
    auto& autoAlignToUe = config.autoAlignToUe;
    auto& descendingPass = config.descendingPass;
    auto& alignmentReferenceTimeSeconds = config.alignmentReferenceTimeSeconds;
    auto& overpassGapSeconds = config.overpassGapSeconds;
    auto& overpassTimeOffsetSeconds = config.overpassTimeOffsetSeconds;
    auto& updateIntervalMs = config.updateIntervalMs;
    auto& minElevationDeg = config.minElevationDeg;
    auto& ueLatitudeDeg = config.ueLatitudeDeg;
    auto& ueLongitudeDeg = config.ueLongitudeDeg;
    auto& ueAltitudeMeters = config.ueAltitudeMeters;
    auto& lockCellAnchorToUe = config.lockCellAnchorToUe;
    auto& cellLatitudeDeg = config.cellLatitudeDeg;
    auto& cellLongitudeDeg = config.cellLongitudeDeg;
    auto& cellAltitudeMeters = config.cellAltitudeMeters;
    auto& lockGridCenterToUe = config.lockGridCenterToUe;
    auto& gridCenterLatitudeDeg = config.gridCenterLatitudeDeg;
    auto& gridCenterLongitudeDeg = config.gridCenterLongitudeDeg;
    auto& centralFrequency = config.centralFrequency;
    auto& bandwidth = config.bandwidth;
    auto& lambda = config.lambda;
    auto& udpPacketSize = config.udpPacketSize;
    auto& gnbTxPower = config.gnbTxPower;
    auto& ueTxPower = config.ueTxPower;
    auto& beamMaxGainDbi = config.beamMaxGainDbi;
    auto& scanMaxDeg = config.scanMaxDeg;
    auto& theta3dBDeg = config.theta3dBDeg;
    auto& sideLobeAttenuationDb = config.sideLobeAttenuationDb;
    auto& ueRxGainDbi = config.ueRxGainDbi;
    auto& atmLossDb = config.atmLossDb;
    auto& beamDropPenaltyDb = config.beamDropPenaltyDb;
    auto& hoHysteresisDb = config.hoHysteresisDb;
    auto& hoTttMs = config.hoTttMs;
    auto& startupVerbose = config.startupVerbose;
    auto& runAttenuationScript = config.runAttenuationScript;
    auto& attenuationScriptPath = config.attenuationScriptPath;
    auto& attenuationInputPath = config.attenuationInputPath;
    auto& attenuationPerTimePath = config.attenuationPerTimePath;
    auto& throughputReportIntervalSeconds = config.throughputReportIntervalSeconds;
    auto& enableSrsInFSlots = config.enableSrsInFSlots;
    auto& enableSrsInUlSlots = config.enableSrsInUlSlots;
    auto& srsSymbols = config.srsSymbols;
    const std::string& attenuationSummaryOutputPath = outputPaths.attenuationSummaryOutputPath;
    const std::string& attenuationTimeMatrixOutputPath =
        outputPaths.attenuationTimeMatrixOutputPath;

    // ------------------------------
    // 3. 几何场景与链路预算参数初始化
    // ------------------------------
    g_ueGroundPoint = LeoOrbitCalculator::CreateGroundPoint(ueLatitudeDeg, ueLongitudeDeg, ueAltitudeMeters);
    if (lockCellAnchorToUe)
    {
        cellLatitudeDeg = ueLatitudeDeg;
        cellLongitudeDeg = ueLongitudeDeg;
        cellAltitudeMeters = ueAltitudeMeters;
    }
    g_cellGroundPoint =
        LeoOrbitCalculator::CreateGroundPoint(cellLatitudeDeg, cellLongitudeDeg, cellAltitudeMeters);
    g_gmstAtEpochRad = LeoOrbitCalculator::DegToRad(gmstAtEpochDeg);
    g_minElevationRad = LeoOrbitCalculator::DegToRad(minElevationDeg);
    g_carrierFrequencyHz = centralFrequency;
    g_beamModelConfig.carrierFrequencyHz = centralFrequency;
    g_beamModelConfig.txPowerDbm = gnbTxPower;
    g_beamModelConfig.gMax0Dbi = beamMaxGainDbi;
    g_beamModelConfig.alphaMaxRad = LeoOrbitCalculator::DegToRad(scanMaxDeg);
    g_beamModelConfig.theta3dBRad = LeoOrbitCalculator::DegToRad(theta3dBDeg);
    g_beamModelConfig.slaVDb = sideLobeAttenuationDb;
    g_beamModelConfig.rxGainDbi = ueRxGainDbi;
    g_beamModelConfig.atmLossDb = atmLossDb;
    g_beamModelConfig.beamDropPenaltyDb = beamDropPenaltyDb;
    if (lockGridCenterToUe)
    {
        gridCenterLatitudeDeg = ueLatitudeDeg;
        gridCenterLongitudeDeg = ueLongitudeDeg;
    }
    ApplyGlobalMirrorConfig(config);

    const bool outputDirsReady = EnsureParentDirectoryForFile(g_gridCatalogPath) &&
                                 EnsureParentDirectoryForFile(attenuationInputPath) &&
                                 EnsureParentDirectoryForFile(attenuationPerTimePath) &&
                                 EnsureParentDirectoryForFile(attenuationSummaryOutputPath) &&
                                 EnsureParentDirectoryForFile(attenuationTimeMatrixOutputPath);
    NS_ABORT_MSG_IF(!outputDirsReady, "Failed to create simulation output directories");

    // 生成用于波束锚定和后续可视化的六边形网格。
    if (g_useWgs84HexGrid)
    {
        g_hexGridCells = BuildWgs84HexGrid(g_gridCenterLatitudeDeg,
                                           g_gridCenterLongitudeDeg,
                                           g_gridWidthKm * 1000.0,
                                           g_gridHeightKm * 1000.0,
                                           g_hexCellRadiusKm * 1000.0);
        NS_ABORT_MSG_IF(g_hexGridCells.empty(), "hex-grid generation produced 0 cells");

        if (startupVerbose)
        {
            std::cout << std::fixed << std::setprecision(3)
                      << "[Grid] WGS84 hex-grid enabled center=(" << g_gridCenterLatitudeDeg << "deg, "
                      << g_gridCenterLongitudeDeg << "deg)"
                      << " size=" << g_gridWidthKm << "x" << g_gridHeightKm << "km"
                      << " hexRadius=" << g_hexCellRadiusKm << "km"
                      << " cells=" << g_hexGridCells.size()
                      << " K=" << g_gridNearestK
                      << std::endl;
        }
        else
        {
            std::cout << "[Grid] enabled cells=" << g_hexGridCells.size()
                      << " K=" << g_gridNearestK << std::endl;
        }

        if (g_printGridCatalog)
        {
            DumpHexGridCatalog(g_gridCatalogPath, g_hexGridCells);
            if (startupVerbose)
            {
                std::cout << "[Grid] catalog exported: " << g_gridCatalogPath << std::endl;
            }
        }
    }
    else
    {
        g_hexGridCells.clear();
    }

    // 输出当前基础组的关键物理和切换配置。
    std::cout << std::fixed << std::setprecision(3)
              << "[BeamModel] mode=" << (g_useWgs84HexGrid ? "HEX_GRID" : "FIXED_ANCHOR")
              << " alphaMax=" << scanMaxDeg << "deg"
              << " theta3dB=" << theta3dBDeg << "deg" << std::endl;
    std::cout << "[Constellation] satellites=" << gNbNum
              << " planes=" << orbitPlaneCount
              << " ue=" << ueNum
              << " raanSpacing=" << interPlaneRaanSpacingDeg << "deg"
              << " planeTimeOffset=" << interPlaneTimeOffsetSeconds << "s" << std::endl;
    std::cout << "[UE-Layout] type=" << ueLayoutType;
    if (ueLayoutType == "hotspot-boundary")
    {
        std::cout << " groups=hotspot(9)+boundary(10)+background(6)"
                  << " hotspotSpacing=" << ueHotspotSpacingMeters / 1000.0 << "km"
                  << " boundarySpacing=" << ueBoundarySpacingMeters / 1000.0 << "km"
                  << " boundaryOffset=" << ueBoundaryOffsetMeters / 1000.0 << "km"
                  << " hotspotCenter=(" << ueHotspotCenterOffsetXMeters / 1000.0 << "kmE,"
                  << ueHotspotCenterOffsetYMeters / 1000.0 << "kmN)"
                  << " backgroundRadius=(" << ueBackgroundRadiusXMeters / 1000.0 << "km,"
                  << ueBackgroundRadiusYMeters / 1000.0 << "km)";
    }
    else
    {
        std::cout << " spacing=" << ueSpacingMeters / 1000.0 << "km";
    }
    std::cout << std::endl;
    std::cout << "[NRT] strict=" << (g_strictNrtGuard ? "ON" : "OFF")
              << " margin=" << g_strictNrtMarginDb << "dB";
    if (g_strictNrtGuard && g_useCustomA3Executor)
    {
        std::cout << " a3=custom(ttt=" << g_manualHoTttSeconds << "s)";
    }
    std::cout << std::endl;
    std::cout << "[Output] dir=" << g_outputDir << std::endl;

    const double semiMajorAxisMeters = LeoOrbitCalculator::kWgs84SemiMajorAxisMeters + satAltitudeMeters;
    const double meanMotionRadPerSec =
        std::sqrt(LeoOrbitCalculator::kEarthGravitationalMu / std::pow(semiMajorAxisMeters, 3.0));

    const double inclinationRad = LeoOrbitCalculator::DegToRad(orbitInclinationDeg);
    double raanRad = LeoOrbitCalculator::DegToRad(orbitRaanDeg);
    const double argPerigeeRad = LeoOrbitCalculator::DegToRad(orbitArgPerigeeDeg);
    double baseTrueAnomalyRad = LeoOrbitCalculator::DegToRad(baseTrueAnomalyDeg);

    if (autoAlignToUe)
    {
        const auto aligned = AutoAlignOrbitToUe(alignmentReferenceTimeSeconds,
                                                descendingPass,
                                                semiMajorAxisMeters,
                                                orbitEccentricity,
                                                inclinationRad,
                                                argPerigeeRad,
                                                raanRad,
                                                baseTrueAnomalyRad,
                                                meanMotionRadPerSec,
                                                g_gmstAtEpochRad,
                                                g_ueGroundPoint,
                                                centralFrequency,
                                                g_minElevationRad);
        raanRad = aligned.raanRad;
        baseTrueAnomalyRad = aligned.baseTrueAnomalyRad;
        orbitRaanDeg = LeoOrbitCalculator::RadToDeg(raanRad);
        baseTrueAnomalyDeg = LeoOrbitCalculator::RadToDeg(baseTrueAnomalyRad);

        std::cout << "[Setup] autoAlign=ON branch=" << (descendingPass ? "descending" : "ascending")
                  << " peakEl=" << LeoOrbitCalculator::RadToDeg(aligned.peakElevationRad) << "deg" << std::endl;
    }
    if (orbitPlaneCount > 1)
    {
        std::cout << "[Setup] multi-plane mode: plane0 keeps the aligned reference orbit, "
                  << "additional planes use RAAN/time offsets" << std::endl;
    }

    // ------------------------------
    // 4. NR/EPC 与信道配置
    // ------------------------------
    Config::SetDefault("ns3::ThreeGppChannelModel::UpdatePeriod", TimeValue(MilliSeconds(10)));
    Config::SetDefault("ns3::NrUePhy::UeMeasurementsFilterPeriod", TimeValue(MilliSeconds(50)));
    Config::SetDefault("ns3::NrAnr::Threshold", UintegerValue(0));

    if (g_printOrbitCheck)
    {
        std::cout << "=== Orbit math check: Kepler/ECI/ECEF/ENU (0~100s, step 10s) ===" << std::endl;
    }
    LeoOrbitCalculator::KeplerElements checkOrbit;
    checkOrbit.semiMajorAxisMeters = semiMajorAxisMeters;
    checkOrbit.eccentricity = orbitEccentricity;
    checkOrbit.inclinationRad = inclinationRad;
    checkOrbit.raanRad = raanRad;
    checkOrbit.argPerigeeRad = argPerigeeRad;
    checkOrbit.trueAnomalyAtEpochRad = baseTrueAnomalyRad;

    for (uint32_t t = 0; t <= 100; t += 10)
    {
        const auto state = LeoOrbitCalculator::Calculate(static_cast<double>(t),
                                                         checkOrbit,
                                                         g_gmstAtEpochRad,
                                                         g_ueGroundPoint,
                                                         centralFrequency,
                                                         g_minElevationRad);
        if (g_printOrbitCheck)
        {
            std::cout << std::fixed << std::setprecision(3)
                      << "[OrbitCheck] t=" << t << "s"
                      << " ECEF=(" << state.ecef.x << ", " << state.ecef.y << ", " << state.ecef.z
                      << ") m"
                      << " el=" << LeoOrbitCalculator::RadToDeg(state.elevationRad) << "deg"
                      << " fd=" << state.dopplerHz / 1e3 << "kHz"
                      << std::endl;
        }
    }

    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    Ptr<NrPointToPointEpcHelper> nrEpcHelper = CreateObject<NrPointToPointEpcHelper>();
    nrEpcHelper->SetAttribute("S1uLinkDelay", TimeValue(MilliSeconds(0)));
    nrHelper->SetEpcHelper(nrEpcHelper);

    // 恢复 A3 切换算法配置
    nrHelper->SetHandoverAlgorithmType("ns3::NrA3RsrpHandoverAlgorithm");
    nrHelper->SetHandoverAlgorithmAttribute("Hysteresis", DoubleValue(hoHysteresisDb));
    nrHelper->SetHandoverAlgorithmAttribute("TimeToTrigger", TimeValue(MilliSeconds(hoTttMs)));

    Ptr<NrChannelHelper> channelHelper = CreateObject<NrChannelHelper>();
    channelHelper->ConfigureFactories("NTN-Rural", "LOS", "ThreeGpp");
    channelHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(true));

    CcBwpCreator ccBwpCreator;
    CcBwpCreator::SimpleOperationBandConf bandConf(centralFrequency, bandwidth, 1);
    OperationBandInfo band = ccBwpCreator.CreateOperationBandContiguousCc(bandConf);
    channelHelper->AssignChannelsToBands({band});
    BandwidthPartInfoPtrVector allBwps = CcBwpCreator::GetAllBwps({band});

    nrHelper->SetGnbPhyAttribute("TxPower", DoubleValue(gnbTxPower));
    nrHelper->SetUePhyAttribute("TxPower", DoubleValue(ueTxPower));

    nrHelper->SetSchedulerTypeId(TypeId::LookupByName("ns3::NrMacSchedulerTdmaRR"));
    nrHelper->SetSchedulerAttribute("EnableSrsInFSlots", BooleanValue(enableSrsInFSlots));
    nrHelper->SetSchedulerAttribute("EnableSrsInUlSlots", BooleanValue(enableSrsInUlSlots));
    nrHelper->SetSchedulerAttribute("SrsSymbols", UintegerValue(srsSymbols));
    nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(1));
    nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(2));
    nrHelper->SetUeAntennaAttribute("AntennaElement", PointerValue(CreateObject<IsotropicAntennaModel>()));
    nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(8));
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(8));
    nrHelper->SetGnbAntennaAttribute("AntennaElement", PointerValue(CreateObject<IsotropicAntennaModel>()));

    Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject<IdealBeamformingHelper>();
    idealBeamformingHelper->SetAttribute("BeamformingMethod", TypeIdValue(DirectPathBeamforming::GetTypeId()));
    nrHelper->SetBeamformingHelper(idealBeamformingHelper);

    // ------------------------------
    // 5. 节点创建与初始位置部署
    // ------------------------------
    NodeContainer gNbNodes;
    gNbNodes.Create(gNbNum);
    NodeContainer ueNodes;
    ueNodes.Create(ueNum);

    MobilityHelper gnbMobility;
    gnbMobility.SetMobilityModel("ns3::GeocentricConstantPositionMobilityModel");
    gnbMobility.Install(gNbNodes);

    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::GeocentricConstantPositionMobilityModel");
    ueMobility.Install(ueNodes);

    UeLayoutConfig ueLayout;
    ueLayout.layoutType = ueLayoutType;
    ueLayout.lineSpacingMeters = ueSpacingMeters;
    ueLayout.hotspotSpacingMeters = ueHotspotSpacingMeters;
    ueLayout.boundarySpacingMeters = ueBoundarySpacingMeters;
    ueLayout.boundaryOffsetMeters = ueBoundaryOffsetMeters;
    ueLayout.backgroundRadiusXMeters = ueBackgroundRadiusXMeters;
    ueLayout.backgroundRadiusYMeters = ueBackgroundRadiusYMeters;
    ueLayout.hotspotCenterOffsetXMeters = ueHotspotCenterOffsetXMeters;
    ueLayout.hotspotCenterOffsetYMeters = ueHotspotCenterOffsetYMeters;
    const auto uePlacements =
        BuildUePlacements(ueLatitudeDeg, ueLongitudeDeg, ueAltitudeMeters, ueNum, ueLayout);
    NS_ABORT_MSG_IF(uePlacements.size() != ueNum, "UE placement count does not match ueNum");
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        ueNodes.Get(i)->GetObject<MobilityModel>()->SetPosition(uePlacements[i].groundPoint.ecef);
    }

    NetDeviceContainer gNbNetDev = nrHelper->InstallGnbDevice(gNbNodes, allBwps);
    NetDeviceContainer ueNetDev = nrHelper->InstallUeDevice(ueNodes, allBwps);

    int64_t randomStream = 1;
    randomStream += nrHelper->AssignStreams(gNbNetDev, randomStream);
    randomStream += nrHelper->AssignStreams(ueNetDev, randomStream);

    // 先一次性建立完整 X2 网格；后续由动态 NRT（可见+可锁定）决定邻区是否处于激活态。
    for (uint32_t i = 0; i < gNbNodes.GetN(); ++i)
    {
        for (uint32_t j = i + 1; j < gNbNodes.GetN(); ++j)
        {
            nrEpcHelper->AddX2Interface(gNbNodes.Get(i), gNbNodes.Get(j));
        }
    }

    g_satellites.clear();
    g_cellToSatellite.clear();
    g_satellites.reserve(gNbNum);
    std::vector<uint32_t> satellitesPerPlane(orbitPlaneCount, gNbNum / orbitPlaneCount);
    for (uint32_t planeIdx = 0; planeIdx < (gNbNum % orbitPlaneCount); ++planeIdx)
    {
        satellitesPerPlane[planeIdx]++;
    }

    // 为每颗卫星绑定轨道、初始位置和初始网格锚点。
    uint32_t globalSatIdx = 0;
    for (uint32_t planeIdx = 0; planeIdx < orbitPlaneCount; ++planeIdx)
    {
        const uint32_t satsInPlane = satellitesPerPlane[planeIdx];
        const double planeCenterIndex = (static_cast<double>(satsInPlane) - 1.0) / 2.0;
        const double planeRaanRad =
            LeoOrbitCalculator::NormalizeAngle(raanRad +
                                               LeoOrbitCalculator::DegToRad(interPlaneRaanSpacingDeg) * planeIdx);
        const double planeRaanDeg = LeoOrbitCalculator::RadToDeg(planeRaanRad);
        const double planeTimeOffset = interPlaneTimeOffsetSeconds * planeIdx;

        for (uint32_t slotIdx = 0; slotIdx < satsInPlane; ++slotIdx, ++globalSatIdx)
        {
            auto dev = DynamicCast<NrGnbNetDevice>(gNbNetDev.Get(globalSatIdx));
            auto rrc = dev->GetRrc();

            const double overpassTime =
                alignmentReferenceTimeSeconds +
                (static_cast<double>(slotIdx) - planeCenterIndex) * overpassGapSeconds +
                overpassTimeOffsetSeconds + planeTimeOffset;
            LeoOrbitCalculator::KeplerElements orbit;
            orbit.semiMajorAxisMeters = semiMajorAxisMeters;
            orbit.eccentricity = orbitEccentricity;
            orbit.inclinationRad = inclinationRad;
            orbit.raanRad = planeRaanRad;
            orbit.argPerigeeRad = argPerigeeRad;
            // 对近圆 LEO 而言，平移真近点角可近似实现按时间错开的过顶相位。
            orbit.trueAnomalyAtEpochRad = baseTrueAnomalyRad - meanMotionRadPerSec * overpassTime;

            const auto initState = LeoOrbitCalculator::Calculate(0.0,
                                                                 orbit,
                                                                 g_gmstAtEpochRad,
                                                                 g_ueGroundPoint,
                                                                 centralFrequency,
                                                                 g_minElevationRad);
            gNbNodes.Get(globalSatIdx)->GetObject<MobilityModel>()->SetPosition(initState.ecef);

            SatelliteRuntime sat;
            sat.node = gNbNodes.Get(globalSatIdx);
            sat.dev = dev;
            sat.rrc = rrc;
            sat.orbit = orbit;
            sat.orbitPlaneIndex = planeIdx;
            sat.orbitSlotIndex = slotIdx;
            sat.cellAnchorEcef = g_cellGroundPoint.ecef;

            if (g_useWgs84HexGrid && !g_hexGridCells.empty())
            {
                const auto anchor = ComputeGridAnchorSelection(g_hexGridCells, initState.ecef, g_gridNearestK);
                if (anchor.found)
                {
                    sat.currentAnchorGridId = anchor.anchorGridId;
                    sat.cellAnchorEcef = anchor.anchorEcef;
                    sat.nearestGridIds = anchor.nearestGridIds;
                }
            }
            g_cellToSatellite[dev->GetCellId()] = globalSatIdx;
            g_satellites.push_back(sat);

            if (startupVerbose && g_useWgs84HexGrid && !g_hexGridCells.empty())
            {
                UpdateSatelliteAnchorFromGrid(globalSatIdx, initState.ecef, 0.0, true);
            }

            std::cout << "[Setup] sat" << globalSatIdx << " plane=" << planeIdx << " slot=" << slotIdx
                      << " cell=" << dev->GetCellId();
            if (startupVerbose)
            {
                std::cout << " nu0=" << LeoOrbitCalculator::RadToDeg(orbit.trueAnomalyAtEpochRad) << "deg"
                          << " i=" << orbitInclinationDeg << "deg"
                          << " RAAN=" << planeRaanDeg << "deg";
                if (g_useWgs84HexGrid && g_satellites[globalSatIdx].currentAnchorGridId != 0)
                {
                    std::cout << " anchorGrid=" << g_satellites[globalSatIdx].currentAnchorGridId;
                }
            }
            std::cout << std::endl;
        }
    }

    // ------------------------------
    // 6. 切换算法与核心网初始化
    // ------------------------------
    // 手动将 A3 算法挂接到每个 gNB RRC
    g_hoAlgos.clear();
    g_hoAlgos.reserve(gNbNetDev.GetN());
    for (uint32_t i = 0; i < gNbNetDev.GetN(); ++i)
    {
        auto gnbDev = DynamicCast<NrGnbNetDevice>(gNbNetDev.Get(i));
        auto rrc = gnbDev->GetRrc();

        Ptr<NrA3RsrpHandoverAlgorithm> ho = CreateObject<NrA3RsrpHandoverAlgorithm>();
        ho->SetAttribute("Hysteresis", DoubleValue(hoHysteresisDb));
        ho->SetAttribute("TimeToTrigger", TimeValue(MilliSeconds(hoTttMs)));

        rrc->SetNrHandoverManagementSapProvider(ho->GetNrHandoverManagementSapProvider());
        ho->SetNrHandoverManagementSapUser(rrc->GetNrHandoverManagementSapUser());
        ho->Initialize();
        g_hoAlgos.push_back(ho);
    }

    auto remoteHostWithAddr = nrEpcHelper->SetupRemoteHost("100Gb/s", 2500, Seconds(0.0));
    Ptr<Node> remoteHost = remoteHostWithAddr.first;

    InternetStackHelper internet;
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIpIface = nrEpcHelper->AssignUeIpv4Address(NetDeviceContainer(ueNetDev));
    Ipv4StaticRoutingHelper routingHelper;

    g_ues.clear();
    g_ues.reserve(ueNum);
    g_imsiToUe.clear();

    // ------------------------------
    // 7. UE 初始接入、业务安装与预测切换
    // ------------------------------
    // 为每个 UE 单独选择初始服务星、业务端口和预测切换目标。
    for (uint32_t ueIdx = 0; ueIdx < ueNum; ++ueIdx)
    {
        UeRuntime ue;
        ue.node = ueNodes.Get(ueIdx);
        ue.dev = DynamicCast<NrUeNetDevice>(ueNetDev.Get(ueIdx));
        ue.groundPoint = uePlacements[ueIdx].groundPoint;
        ue.placementRole = uePlacements[ueIdx].role;
        ue.eastOffsetMeters = uePlacements[ueIdx].eastOffsetMeters;
        ue.northOffsetMeters = uePlacements[ueIdx].northOffsetMeters;

        uint32_t initialAttachIdx = 0;
        uint32_t bestVisibleIdx = 0;
        uint32_t bestAnyIdx = 0;
        uint32_t bestEligibleIdx = 0;
        double bestEligibleRsrp = -std::numeric_limits<double>::infinity();
        double bestVisibleElevation = -std::numeric_limits<double>::infinity();
        double bestAnyElevation = -std::numeric_limits<double>::infinity();
        for (uint32_t satIdx = 0; satIdx < g_satellites.size(); ++satIdx)
        {
            const auto state = LeoOrbitCalculator::Calculate(0.0,
                                                             g_satellites[satIdx].orbit,
                                                             g_gmstAtEpochRad,
                                                             ue.groundPoint,
                                                             centralFrequency,
                                                             g_minElevationRad);
            const auto budget = CalculateEarthFixedBeamBudget(state.ecef,
                                                              ue.groundPoint.ecef,
                                                              g_satellites[satIdx].cellAnchorEcef,
                                                              g_beamModelConfig);

            if (state.elevationRad > bestAnyElevation)
            {
                bestAnyElevation = state.elevationRad;
                bestAnyIdx = satIdx;
            }
            if (state.visible && state.elevationRad > bestVisibleElevation)
            {
                bestVisibleElevation = state.elevationRad;
                bestVisibleIdx = satIdx;
            }
            if (state.visible && budget.beamLocked && budget.rsrpDbm > bestEligibleRsrp)
            {
                bestEligibleRsrp = budget.rsrpDbm;
                bestEligibleIdx = satIdx;
            }
        }

        if (std::isfinite(bestEligibleRsrp))
        {
            initialAttachIdx = bestEligibleIdx;
        }
        else if (std::isfinite(bestVisibleElevation))
        {
            initialAttachIdx = bestVisibleIdx;
            std::cout << "[Setup] warning: ue" << ueIdx
                      << " visible satellites exist but none satisfy alpha<alphaMax at t=0, "
                      << "fallback to highest-elevation visible sat" << initialAttachIdx << std::endl;
        }
        else
        {
            initialAttachIdx = bestAnyIdx;
            std::cout << "[Setup] warning: ue" << ueIdx << " no visible satellite at t=0, attaching to "
                      << "highest-elevation sat" << initialAttachIdx
                      << " (el=" << LeoOrbitCalculator::RadToDeg(bestAnyElevation) << "deg)" << std::endl;
        }

        ue.initialAttachIdx = initialAttachIdx;
        ue.expectedSourceIndex = initialAttachIdx;
        ue.expectedTargetIndex = initialAttachIdx;
        const double predictionStepSeconds =
            std::max(0.01, std::min(0.1, static_cast<double>(updateIntervalMs) / 1000.0));
        // 用离线代理提前估计第一次切换时刻，便于与真实仿真结果对照。
        const auto predicted = PredictFirstHandoverA3Proxy(initialAttachIdx,
                                                           simTime,
                                                           predictionStepSeconds,
                                                           hoHysteresisDb,
                                                           static_cast<double>(hoTttMs) / 1000.0,
                                                           ue.groundPoint);
        if (predicted.has_value())
        {
            ue.hasPredictedHandover = true;
            ue.expectedSourceIndex = predicted->sourceIdx;
            ue.expectedTargetIndex = predicted->targetIdx;
            ue.predictedHandoverTimeSeconds = predicted->triggerTimeSeconds;
        }

        nrHelper->AttachToGnb(ueNetDev.Get(ueIdx), gNbNetDev.Get(initialAttachIdx));

        Ptr<Ipv4StaticRouting> ueStaticRouting =
            routingHelper.GetStaticRouting(ueNodes.Get(ueIdx)->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(nrEpcHelper->GetUeDefaultGatewayAddress(), 1);

        const uint16_t dlPort = static_cast<uint16_t>(1234 + ueIdx);
        UdpServerHelper ueUdpServer(dlPort);
        ApplicationContainer serverApps = ueUdpServer.Install(ueNodes.Get(ueIdx));

        UdpClientHelper dlClient(ueIpIface.GetAddress(ueIdx), dlPort);
        dlClient.SetAttribute("PacketSize", UintegerValue(udpPacketSize));
        dlClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
        dlClient.SetAttribute("Interval", TimeValue(Seconds(1.0 / lambda)));
        ApplicationContainer clientApps = dlClient.Install(remoteHost);

        // 为每个 UE 建立独立的专用承载，确保业务流和切换统计可区分。
        NrEpsBearer bearer(NrEpsBearer::GBR_CONV_VOICE);
        Ptr<NrEpcTft> tft = Create<NrEpcTft>();
        NrEpcTft::PacketFilter dlpf;
        dlpf.localPortStart = dlPort;
        dlpf.localPortEnd = dlPort;
        tft->Add(dlpf);
        nrHelper->ActivateDedicatedEpsBearer(ueNetDev.Get(ueIdx), bearer, tft);

        serverApps.Start(Seconds(appStartTime));
        clientApps.Start(Seconds(appStartTime));
        serverApps.Stop(Seconds(simTime));
        clientApps.Stop(Seconds(simTime));

        ue.server = serverApps.Get(0)->GetObject<UdpServer>();
        if (ue.dev)
        {
            g_imsiToUe[ue.dev->GetImsi()] = ueIdx;
        }
        g_ues.push_back(ue);
    }

    // ------------------------------
    // 8. 运行时复位、回调注册与周期事件调度
    // ------------------------------
    ResetRuntimeState(gNbNum);
    if (g_satBeamTrace.is_open())
    {
        g_satBeamTrace.close();
    }
    g_satBeamTrace.open(attenuationInputPath, std::ios::out | std::ios::trunc);
    NS_ABORT_MSG_IF(!g_satBeamTrace.is_open(), "Failed to open beam trace CSV: " << attenuationInputPath);
    g_satBeamTrace
        << "time_s,ue,sat,cell,beam_locked,scan_loss_db,pattern_loss_db,fspl_db,atm_loss_db,rsrp_dbm,"
        << "attached_ue_count,offered_packet_rate,load_score,admission_allowed\n";

    g_gnbRrcs.clear();
    for (const auto& sat : g_satellites)
    {
        g_gnbRrcs.push_back(sat.rrc);
    }
    if (g_strictNrtGuard)
    {
        if (g_useCustomA3Executor)
        {
            for (const auto& rrc : g_gnbRrcs)
            {
                rrc->SetNrHandoverManagementSapProvider(nullptr);
            }
            g_a3GateServingSat = std::numeric_limits<uint32_t>::max();
        }
        else
        {
            ApplyA3Gate(std::numeric_limits<uint32_t>::max(), false);
        }
    }

    Config::Connect("/NodeList/*/DeviceList/*/$ns3::NrGnbNetDevice/NrGnbRrc/HandoverStart",
                    MakeCallback(&ReportHandoverStart));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::NrGnbNetDevice/NrGnbRrc/HandoverEndOk",
                    MakeCallback(&ReportHandoverEndOk));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::NrUeNetDevice/NrUeRrc/StateTransition",
                    MakeCallback(&ReportStateTransition));

    // 可选的周期吞吐输出，默认关闭，避免压过切换日志。
    for (auto& ue : g_ues)
    {
        ue.lastRxPackets = 0;
        ue.lastThroughputTime = Seconds(appStartTime);
    }
    if (throughputReportIntervalSeconds > 0.0)
    {
        Simulator::Schedule(Seconds(appStartTime + throughputReportIntervalSeconds),
                            &PrintDlThroughput,
                            udpPacketSize,
                            Seconds(throughputReportIntervalSeconds));
    }

    for (uint32_t ueIdx = 0; ueIdx < g_ues.size(); ++ueIdx)
    {
        const auto& ue = g_ues[ueIdx];
        std::cout << "[Setup] ue" << ueIdx << " start attach=sat" << ue.initialAttachIdx
                  << "(cell=" << g_satellites[ue.initialAttachIdx].dev->GetCellId() << ")"
                  << " role=" << ue.placementRole
                  << " offset=(" << ue.eastOffsetMeters / 1000.0 << "kmE,"
                  << ue.northOffsetMeters / 1000.0 << "kmN)";
        if (ue.hasPredictedHandover)
        {
            std::cout << " predicted=sat" << ue.expectedSourceIndex
                      << "->sat" << ue.expectedTargetIndex
                      << "@" << ue.predictedHandoverTimeSeconds << "s";
        }
        else
        {
            std::cout << " predicted=none";
        }
        std::cout << std::endl;
    }

    // `UpdateConstellation` 是整个场景的时变主循环。
    Simulator::Schedule(Seconds(0.0),
                        &UpdateConstellation,
                        MilliSeconds(updateIntervalMs),
                        Seconds(simTime));
    if (g_printSimulationProgress)
    {
        Simulator::Schedule(Seconds(std::min(g_progressReportIntervalSeconds, simTime)),
                            &PrintSimulationProgress,
                            Seconds(g_progressReportIntervalSeconds));
    }

    // ------------------------------
    // 9. 仿真运行与结果输出
    // ------------------------------
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    const double appDuration = std::max(0.0, simTime - appStartTime);
    PrintDlTrafficSummary(g_ues, appDuration, udpPacketSize);
    PrintHandoverSummary(g_ues);

    if (g_satBeamTrace.is_open())
    {
        g_satBeamTrace.close();
    }

    if (runAttenuationScript)
    {
        std::remove(attenuationSummaryOutputPath.c_str());
        std::remove(attenuationTimeMatrixOutputPath.c_str());
        // 交给外部 Python 脚本生成逐时刻衰减明细 CSV。
        std::ostringstream cmdline;
        cmdline << "python3 \"" << attenuationScriptPath << "\""
                << " --input \"" << attenuationInputPath << "\""
                << " --per-time-out \"" << attenuationPerTimePath << "\"";
        const int scriptRc = std::system(cmdline.str().c_str());
        if (scriptRc == 0)
        {
            std::cout << "[Attenuation] external report generated successfully" << std::endl;
        }
        else
        {
            std::cout << "[Attenuation] external report script failed, rc=" << scriptRc << std::endl;
        }
    }

    Simulator::Destroy();
    return 0;
}
