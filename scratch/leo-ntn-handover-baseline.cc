#include "ns3/antenna-module.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/geocentric-constant-position-mobility-model.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-leo-a3-measurement-handover-algorithm.h"
#include "ns3/nr-gnb-rrc.h"
#include "ns3/nr-module.h"
#include "ns3/nr-ue-rrc.h"
#include "handover/beam-link-budget.h"
#include "handover/leo-ntn-handover-config.h"
#include "handover/leo-orbit-calculator.h"
#include "handover/leo-ntn-handover-reporting.h"
#include "handover/leo-ntn-handover-runtime.h"
#include "handover/leo-ntn-handover-update.h"
#include "handover/leo-ntn-handover-utils.h"
#include "handover/wgs84-hex-grid.h"
#include <algorithm>
#include <cmath>
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
static double g_hoHysteresisDb = 2.0;
// 将 A->B->A 识别为 ping-pong 的时间窗口。
static double g_pingPongWindowSeconds = 1.5;
// 吞吐恢复判定：以切换前短窗口平均吞吐为参考，结束后连续若干个采样达到阈值即视为恢复。
static constexpr uint32_t g_recoveryReferenceWindowSamples = 20;
static constexpr uint32_t g_recoveryRequiredConsecutiveSamples = 3;
static constexpr double g_recoveryThresholdRatio = 0.9;

// 当前 handover 主链统一走标准 PHY/RRC MeasurementReport。
enum class HandoverMode
{
    BASELINE,
    IMPROVED,
};

static HandoverMode g_handoverMode = HandoverMode::BASELINE;
static double g_improvedSignalWeight = 0.7;
static double g_improvedLoadWeight = 0.3;
static double g_improvedVisibilityWeight = 0.2;
static double g_improvedMinLoadScoreDelta = 0.2;
static double g_improvedMaxSignalGapDb = 3.0;
static double g_improvedReturnGuardSeconds = 1.5;
static double g_improvedMinVisibilitySeconds = 1.0;
static double g_improvedVisibilityHorizonSeconds = 8.0;
static double g_improvedVisibilityPredictionStepSeconds = 0.5;
static uint16_t g_measurementReportIntervalMs = 120;
static uint8_t g_measurementMaxReportCells = 8;
static std::vector<Ptr<NrLeoA3MeasurementHandoverAlgorithm>> g_handoverAlgorithms;

// 逐时刻卫星波束锚点导出文件。
static std::ofstream g_satAnchorTrace;
// 切换窗口下行吞吐采样导出文件。
static std::ofstream g_handoverThroughputTrace;
// 精确记录 HO-START / HO-END-OK 时刻的事件导出文件。
static std::ofstream g_handoverEventTrace;
// ============================================================================
// 运行时复位与事件回调
// ============================================================================

/**
 * 在新一轮仿真开始前，统一重置全局运行时状态。
 */
static void
ResetRuntimeState(uint32_t gNbNum)
{
    for (auto& ue : g_ues)
    {
        ResetUeRuntime(ue, gNbNum);
    }
}

/**
 * 导出当前时刻每颗卫星的地面波束锚点位置。
 */
static void
FlushSatelliteAnchorTraceRows(double nowSeconds)
{
    if (!g_satAnchorTrace.is_open())
    {
        return;
    }

    for (uint32_t satIdx = 0; satIdx < g_satellites.size(); ++satIdx)
    {
        const auto& sat = g_satellites[satIdx];
        double anchorLatDeg = std::numeric_limits<double>::quiet_NaN();
        double anchorLonDeg = std::numeric_limits<double>::quiet_NaN();
        double anchorEastMeters = std::numeric_limits<double>::quiet_NaN();
        double anchorNorthMeters = std::numeric_limits<double>::quiet_NaN();

        if (const auto* anchorCell = FindHexGridCellById(g_hexGridCells, sat.currentAnchorGridId))
        {
            anchorLatDeg = anchorCell->latitudeDeg;
            anchorLonDeg = anchorCell->longitudeDeg;
            anchorEastMeters = anchorCell->eastMeters;
            anchorNorthMeters = anchorCell->northMeters;
        }

        g_satAnchorTrace << std::fixed << std::setprecision(3) << nowSeconds << "," << satIdx << ","
                         << sat.orbitPlaneIndex << "," << sat.orbitSlotIndex << ","
                         << sat.dev->GetCellId() << "," << sat.currentAnchorGridId << ",";
        g_satAnchorTrace << std::setprecision(8) << anchorLatDeg << "," << anchorLonDeg << ",";
        g_satAnchorTrace << std::setprecision(3) << anchorEastMeters << "," << anchorNorthMeters
                         << "\n";
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

static int32_t
ResolveSatelliteIndexFromCellId(uint16_t cellId)
{
    const auto it = g_cellToSatellite.find(cellId);
    if (it == g_cellToSatellite.end())
    {
        return -1;
    }
    return static_cast<int32_t>(it->second);
}

static HandoverMode
ParseHandoverMode(const std::string& handoverMode)
{
    if (handoverMode == "improved")
    {
        return HandoverMode::IMPROVED;
    }
    return HandoverMode::BASELINE;
}

static const char*
ToString(HandoverMode handoverMode)
{
    return handoverMode == HandoverMode::IMPROVED ? "improved" : "baseline";
}

static std::optional<uint32_t>
ResolveUeIndexFromServingCellAndRnti(uint16_t servingCellId, uint16_t rnti)
{
    for (uint32_t ueIdx = 0; ueIdx < g_ues.size(); ++ueIdx)
    {
        const auto& ue = g_ues[ueIdx];
        if (!ue.dev || !ue.dev->GetRrc())
        {
            continue;
        }

        if (ue.dev->GetRrc()->GetCellId() == servingCellId && ue.dev->GetRrc()->GetRnti() == rnti)
        {
            return ueIdx;
        }
    }

    return std::nullopt;
}

struct MeasurementCandidate
{
    uint32_t satIdx = std::numeric_limits<uint32_t>::max();
    uint16_t cellId = 0;
    double rsrpDbm = -std::numeric_limits<double>::infinity();
    double loadScore = 1.0;
    bool admissionAllowed = false;
    double remainingVisibilitySeconds = 0.0;
    double visibilityScore = 0.0;
    double jointScore = -std::numeric_limits<double>::infinity();
};

static double
PredictRemainingVisibilitySeconds(uint32_t satIdx,
                                  const LeoOrbitCalculator::GroundPoint& ueGroundPoint,
                                  double nowSeconds)
{
    if (satIdx >= g_satellites.size())
    {
        return 0.0;
    }

    const double horizonSeconds = std::max(0.0, g_improvedVisibilityHorizonSeconds);
    if (horizonSeconds <= 0.0)
    {
        return 0.0;
    }

    const auto computeVisible = [&](double sampleTimeSeconds) {
        return LeoOrbitCalculator::Calculate(sampleTimeSeconds,
                                             g_satellites[satIdx].orbit,
                                             g_gmstAtEpochRad,
                                             ueGroundPoint,
                                             g_carrierFrequencyHz,
                                             g_minElevationRad)
            .visible;
    };

    if (!computeVisible(nowSeconds))
    {
        return 0.0;
    }

    const double stepSeconds = std::max(0.05, g_improvedVisibilityPredictionStepSeconds);
    const double horizonEndSeconds = nowSeconds + horizonSeconds;
    double previousVisibleTimeSeconds = nowSeconds;

    for (double probeTimeSeconds = nowSeconds + stepSeconds;
         probeTimeSeconds <= horizonEndSeconds + 1e-9;
         probeTimeSeconds += stepSeconds)
    {
        const double clampedProbeTimeSeconds = std::min(probeTimeSeconds, horizonEndSeconds);
        if (computeVisible(clampedProbeTimeSeconds))
        {
            previousVisibleTimeSeconds = clampedProbeTimeSeconds;
            continue;
        }

        double lowSeconds = previousVisibleTimeSeconds;
        double highSeconds = clampedProbeTimeSeconds;
        for (uint32_t iter = 0; iter < 8; ++iter)
        {
            const double midSeconds = 0.5 * (lowSeconds + highSeconds);
            if (computeVisible(midSeconds))
            {
                lowSeconds = midSeconds;
            }
            else
            {
                highSeconds = midSeconds;
            }
        }
        return std::max(0.0, lowSeconds - nowSeconds);
    }

    return horizonSeconds;
}

static double
ComputeLoadPressureFromScore(double loadScore)
{
    // 将负载分数按当前拥塞阈值重新归一化，便于在源站接近拥塞时更快放大负载导向。
    const double congestionSpan = std::max(1e-9, g_loadCongestionThreshold);
    return std::clamp(loadScore / congestionSpan, 0.0, 1.0);
}

static std::optional<MeasurementCandidate>
SelectMeasurementDrivenTarget(uint16_t servingCellId,
                              uint32_t sourceSatIdx,
                              const UeRuntime& ue,
                              const NrRrcSap::MeasResults& measResults)
{
    std::vector<MeasurementCandidate> candidates;
    candidates.reserve(measResults.measResultListEutra.size());

    for (const auto& neighbour : measResults.measResultListEutra)
    {
        if (!neighbour.haveRsrpResult || neighbour.physCellId == servingCellId)
        {
            continue;
        }

        const auto satIt = g_cellToSatellite.find(neighbour.physCellId);
        if (satIt == g_cellToSatellite.end() || satIt->second >= g_satellites.size())
        {
            continue;
        }

        const auto& satellite = g_satellites[satIt->second];
        MeasurementCandidate candidate;
        candidate.satIdx = satIt->second;
        candidate.cellId = neighbour.physCellId;
        candidate.rsrpDbm = nr::EutranMeasurementMapping::RsrpRange2Dbm(neighbour.rsrpResult);
        candidate.loadScore = satellite.loadScore;
        candidate.admissionAllowed = satellite.admissionAllowed;
        candidate.remainingVisibilitySeconds =
            PredictRemainingVisibilitySeconds(candidate.satIdx, ue.groundPoint, Simulator::Now().GetSeconds());
        candidates.push_back(candidate);
    }

    if (candidates.empty())
    {
        return std::nullopt;
    }

    const double sourceLoadScore =
        (sourceSatIdx < g_satellites.size()) ? g_satellites[sourceSatIdx].loadScore : 0.0;
    const double sourceLoadPressure = ComputeLoadPressureFromScore(sourceLoadScore);

    if (g_handoverMode == HandoverMode::BASELINE)
    {
        return *std::max_element(candidates.begin(),
                                 candidates.end(),
                                 [](const auto& lhs, const auto& rhs) {
                                     return lhs.rsrpDbm < rhs.rsrpDbm;
                                 });
    }

    std::vector<MeasurementCandidate> filteredCandidates;
    filteredCandidates.reserve(candidates.size());
    std::copy_if(candidates.begin(),
                 candidates.end(),
                 std::back_inserter(filteredCandidates),
                 [](const auto& candidate) { return candidate.admissionAllowed; });
    if (filteredCandidates.empty())
    {
        filteredCandidates = candidates;
    }

    const double nowSeconds = Simulator::Now().GetSeconds();
    const bool withinReturnGuard =
        g_improvedReturnGuardSeconds > 0.0 && ue.lastSuccessfulHoTimeSeconds >= 0.0 &&
        (nowSeconds - ue.lastSuccessfulHoTimeSeconds <= g_improvedReturnGuardSeconds) &&
        ue.lastSuccessfulHoSourceCell != 0;

    if (withinReturnGuard)
    {
        std::vector<MeasurementCandidate> guardedCandidates;
        guardedCandidates.reserve(filteredCandidates.size());
        std::copy_if(filteredCandidates.begin(),
                     filteredCandidates.end(),
                     std::back_inserter(guardedCandidates),
                     [&](const auto& candidate) {
                         return candidate.cellId != ue.lastSuccessfulHoSourceCell;
                     });
        if (!guardedCandidates.empty())
        {
            filteredCandidates = std::move(guardedCandidates);
        }
    }

    if (g_improvedMinVisibilitySeconds > 0.0)
    {
        std::vector<MeasurementCandidate> visibilityQualifiedCandidates;
        visibilityQualifiedCandidates.reserve(filteredCandidates.size());
        std::copy_if(filteredCandidates.begin(),
                     filteredCandidates.end(),
                     std::back_inserter(visibilityQualifiedCandidates),
                     [](const auto& candidate) {
                         return candidate.remainingVisibilitySeconds >=
                                g_improvedMinVisibilitySeconds;
                     });
        if (!visibilityQualifiedCandidates.empty())
        {
            filteredCandidates = std::move(visibilityQualifiedCandidates);
        }
    }

    const auto bestSignalIt =
        std::max_element(filteredCandidates.begin(),
                         filteredCandidates.end(),
                         [](const auto& lhs, const auto& rhs) {
                             return lhs.rsrpDbm < rhs.rsrpDbm;
                         });
    const MeasurementCandidate bestSignalCandidate = *bestSignalIt;

    const double dynamicMaxSignalGapDb =
        g_improvedMaxSignalGapDb + 2.0 * sourceLoadPressure;
    const double dynamicMinLoadScoreDelta =
        std::max(0.05, g_improvedMinLoadScoreDelta * (1.0 - 0.5 * sourceLoadPressure));
    const double effectiveSignalWeight =
        std::max(0.15, g_improvedSignalWeight * (1.0 - 0.4 * sourceLoadPressure));
    const double effectiveLoadWeight = g_improvedLoadWeight + 0.6 * sourceLoadPressure;
    const double effectiveVisibilityWeight =
        g_improvedVisibilityWeight * (1.0 - 0.2 * sourceLoadPressure);

    std::vector<MeasurementCandidate> scoredCandidates;
    scoredCandidates.reserve(filteredCandidates.size());
    for (const auto& candidate : filteredCandidates)
    {
        const double signalGapDb = bestSignalCandidate.rsrpDbm - candidate.rsrpDbm;
        const double loadAdvantage = bestSignalCandidate.loadScore - candidate.loadScore;
        const bool keepCandidate = candidate.cellId == bestSignalCandidate.cellId ||
                                   signalGapDb <= dynamicMaxSignalGapDb ||
                                   loadAdvantage >= dynamicMinLoadScoreDelta;
        if (!keepCandidate)
        {
            continue;
        }
        scoredCandidates.push_back(candidate);
    }

    if (scoredCandidates.empty())
    {
        return bestSignalCandidate;
    }

    double minRsrpDbm = std::numeric_limits<double>::infinity();
    double maxRsrpDbm = -std::numeric_limits<double>::infinity();
    for (const auto& candidate : scoredCandidates)
    {
        minRsrpDbm = std::min(minRsrpDbm, candidate.rsrpDbm);
        maxRsrpDbm = std::max(maxRsrpDbm, candidate.rsrpDbm);
    }

    const double signalSpanDb = std::max(1e-9, maxRsrpDbm - minRsrpDbm);
    const double totalWeight = std::max(
        1e-9,
        effectiveSignalWeight + effectiveLoadWeight + effectiveVisibilityWeight);
    const double normalizedSignalWeight = effectiveSignalWeight / totalWeight;
    const double normalizedLoadWeight = effectiveLoadWeight / totalWeight;
    const double normalizedVisibilityWeight = effectiveVisibilityWeight / totalWeight;

    for (auto& candidate : scoredCandidates)
    {
        const double signalScore = (candidate.rsrpDbm - minRsrpDbm) / signalSpanDb;
        const double loadUtility = std::clamp(1.0 - candidate.loadScore, 0.0, 1.0);
        const double loadReliefUtility =
            std::clamp(sourceLoadScore - candidate.loadScore, 0.0, 1.0);
        candidate.visibilityScore = std::clamp(candidate.remainingVisibilitySeconds /
                                                   std::max(1e-9, g_improvedVisibilityHorizonSeconds),
                                               0.0,
                                               1.0);
        candidate.jointScore = normalizedSignalWeight * signalScore +
                               normalizedLoadWeight * loadUtility +
                               normalizedVisibilityWeight * candidate.visibilityScore +
                               0.25 * sourceLoadPressure * loadReliefUtility;
    }

    return *std::max_element(scoredCandidates.begin(),
                             scoredCandidates.end(),
                             [](const auto& lhs, const auto& rhs) {
                                 if (std::abs(lhs.jointScore - rhs.jointScore) > 1e-9)
                                 {
                                     return lhs.jointScore < rhs.jointScore;
                                 }
                                 return lhs.rsrpDbm < rhs.rsrpDbm;
                             });
}

static void
HandleMeasurementDrivenHandoverReport(uint16_t sourceCellId,
                                      uint16_t rnti,
                                      NrRrcSap::MeasResults measResults)
{
    const int32_t sourceSatIdx = ResolveSatelliteIndexFromCellId(sourceCellId);
    if (sourceSatIdx < 0 || static_cast<uint32_t>(sourceSatIdx) >= g_satellites.size())
    {
        return;
    }

    const auto ueIdx = ResolveUeIndexFromServingCellAndRnti(sourceCellId, rnti);
    if (!ueIdx.has_value())
    {
        return;
    }

    auto& ue = g_ues[*ueIdx];
    if (ue.hasPendingHoStart)
    {
        return;
    }

    const auto selectedTarget = SelectMeasurementDrivenTarget(sourceCellId, static_cast<uint32_t>(sourceSatIdx), ue, measResults);
    if (!selectedTarget.has_value() || selectedTarget->cellId == 0)
    {
        return;
    }

    ue.hasPendingHoStart = true;
    ue.lastHoStartSourceCell = sourceCellId;
    ue.lastHoStartTargetCell = selectedTarget->cellId;
    g_satellites[sourceSatIdx].rrc->GetNrHandoverManagementSapUser()->TriggerHandover(
        rnti,
        selectedTarget->cellId);

    if (!g_compactReport || g_printNrtEvents)
    {
        std::cout << "[HO-MEAS-TRIGGER] t=" << std::fixed << std::setprecision(3)
                  << Simulator::Now().GetSeconds() << "s"
                  << " mode=" << ToString(g_handoverMode)
                  << " ue=" << *ueIdx
                  << " sourceCell=" << sourceCellId
                  << " targetCell=" << selectedTarget->cellId
                  << " targetRsrp=" << selectedTarget->rsrpDbm << "dBm";
        if (g_handoverMode == HandoverMode::IMPROVED)
        {
            const double sourceLoadScore = g_satellites[sourceSatIdx].loadScore;
            std::cout << " jointScore=" << selectedTarget->jointScore
                      << " loadScore=" << selectedTarget->loadScore
                      << " sourceLoad=" << sourceLoadScore
                      << " sourceLoadPressure=" << ComputeLoadPressureFromScore(sourceLoadScore)
                      << " remainVis=" << selectedTarget->remainingVisibilitySeconds << "s"
                      << " visScore=" << selectedTarget->visibilityScore
                      << " admission=" << (selectedTarget->admissionAllowed ? "YES" : "NO");
        }
        std::cout << std::endl;
    }
}

static void
InstallMeasurementDrivenHandoverAlgorithms(const BaselineSimulationConfig& config)
{
    g_handoverAlgorithms.clear();
    g_handoverAlgorithms.reserve(g_satellites.size());

    const uint8_t maxReportCells = static_cast<uint8_t>(
        std::clamp<uint16_t>(config.measurementMaxReportCells, 1, 32));

    for (auto& sat : g_satellites)
    {
        auto algorithm = CreateObject<NrLeoA3MeasurementHandoverAlgorithm>();
        algorithm->SetAttribute("Hysteresis", DoubleValue(config.hoHysteresisDb));
        algorithm->SetAttribute("TimeToTrigger", TimeValue(MilliSeconds(config.hoTttMs)));
        algorithm->SetAttribute("ReportIntervalMs", UintegerValue(config.measurementReportIntervalMs));
        algorithm->SetAttribute("MaxReportCells", UintegerValue(maxReportCells));
        algorithm->SetAttribute("TriggerHandover", BooleanValue(false));
        sat.rrc->SetNrHandoverManagementSapProvider(algorithm->GetNrHandoverManagementSapProvider());
        algorithm->SetNrHandoverManagementSapUser(sat.rrc->GetNrHandoverManagementSapUser());
        algorithm->TraceConnectWithoutContext("MeasurementReport",
                                              MakeBoundCallback(&HandleMeasurementDrivenHandoverReport,
                                                                sat.dev->GetCellId()));
        sat.rrc->AggregateObject(algorithm);
        algorithm->Initialize();
        g_handoverAlgorithms.push_back(algorithm);
    }
}

static void
InstallStaticNeighbourRelations()
{
    for (uint32_t servingSatIdx = 0; servingSatIdx < g_satellites.size(); ++servingSatIdx)
    {
        auto& servingSat = g_satellites[servingSatIdx];
        servingSat.activeNeighbours.clear();
        for (uint32_t neighbourSatIdx = 0; neighbourSatIdx < g_satellites.size(); ++neighbourSatIdx)
        {
            if (neighbourSatIdx == servingSatIdx)
            {
                continue;
            }

            const uint16_t neighbourCellId = g_satellites[neighbourSatIdx].dev->GetCellId();
            servingSat.activeNeighbours.insert(neighbourCellId);
            servingSat.rrc->AddX2Neighbour(neighbourCellId);
        }
    }
}

static double
ComputeMeanThroughputMbps(const std::deque<double>& samples)
{
    if (samples.empty())
    {
        return std::numeric_limits<double>::quiet_NaN();
    }

    double sum = 0.0;
    uint32_t count = 0;
    for (double value : samples)
    {
        if (!std::isfinite(value))
        {
            continue;
        }
        sum += value;
        ++count;
    }

    if (count == 0)
    {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return sum / static_cast<double>(count);
}

static void
WriteHandoverEventTraceRow(double nowSeconds,
                           uint32_t ueIdx,
                           uint32_t handoverId,
                           const std::string& eventName,
                           uint16_t sourceCellId,
                           uint16_t targetCellId,
                           double delayMs,
                           double recoveryMs,
                           bool pingPongDetected)
{
    if (!g_handoverEventTrace.is_open())
    {
        return;
    }

    g_handoverEventTrace << std::fixed << std::setprecision(3) << nowSeconds << "," << ueIdx << ","
                         << handoverId << "," << eventName << "," << sourceCellId << ","
                         << targetCellId << "," << ResolveSatelliteIndexFromCellId(sourceCellId)
                         << "," << ResolveSatelliteIndexFromCellId(targetCellId) << ",";
    if (std::isfinite(delayMs))
    {
        g_handoverEventTrace << std::fixed << std::setprecision(3) << delayMs;
    }
    g_handoverEventTrace << ",";
    if (std::isfinite(recoveryMs))
    {
        g_handoverEventTrace << std::fixed << std::setprecision(3) << recoveryMs;
    }
    g_handoverEventTrace << "," << (pingPongDetected ? 1 : 0) << "\n";
    g_handoverEventTrace.flush();
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
    std::ostringstream prefix;
    if (const auto ueIdx = ResolveUeIndexFromImsi(g_imsiToUe, imsi))
    {
        auto& ue = g_ues[*ueIdx];
        ue.handoverTraceSequence++;
        ue.activeHandoverTraceId = ue.handoverTraceSequence;
        ue.hasPendingHoStart = true;
        ue.waitingForThroughputRecovery = false;
        ue.waitingRecoveryHoId = 0;
        ue.waitingRecoveryStartTimeSeconds = -1.0;
        ue.waitingRecoverySatisfiedSamples = 0;
        ue.lastHoStartSourceCell = cellId;
        ue.lastHoStartTargetCell = targetCellId;
        ue.lastHoStartTimeSeconds = Simulator::Now().GetSeconds();
        ue.pendingRecoveryReferenceThroughputMbps =
            ComputeMeanThroughputMbps(ue.recentThroughputSamplesMbps);
        if (std::isfinite(ue.pendingRecoveryReferenceThroughputMbps) &&
            ue.pendingRecoveryReferenceThroughputMbps > 0.0)
        {
            ue.pendingRecoveryThresholdThroughputMbps =
                ue.pendingRecoveryReferenceThroughputMbps * g_recoveryThresholdRatio;
        }
        else
        {
            ue.pendingRecoveryThresholdThroughputMbps = std::numeric_limits<double>::quiet_NaN();
        }
        ue.handoverStartCount++;
        WriteHandoverEventTraceRow(ue.lastHoStartTimeSeconds,
                                   *ueIdx,
                                   ue.activeHandoverTraceId,
                                   "HO_START",
                                   cellId,
                                   targetCellId,
                                   std::numeric_limits<double>::quiet_NaN(),
                                   std::numeric_limits<double>::quiet_NaN(),
                                   false);
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
 * - 判断是否发生短时回切与吞吐恢复。
 */
static void
ReportHandoverEndOk(std::string, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
    std::ostringstream prefix;
    double handoverDelayMs = -1.0;
    bool pingPongDetected = false;
    double pingPongGapSeconds = -1.0;
    uint16_t previousSourceCell = 0;
    uint16_t previousTargetCell = 0;
    uint16_t currentSourceCell = 0;
    uint32_t activeHandoverTraceId = 0;
    if (const auto ueIdx = ResolveUeIndexFromImsi(g_imsiToUe, imsi))
    {
        auto& ue = g_ues[*ueIdx];
        ue.handoverEndOkCount++;
        const double nowSeconds = Simulator::Now().GetSeconds();
        activeHandoverTraceId = ue.activeHandoverTraceId;
        previousSourceCell = ue.lastSuccessfulHoSourceCell;
        previousTargetCell = ue.lastSuccessfulHoTargetCell;
        currentSourceCell = ue.lastHoStartSourceCell;
        if (ue.lastHoStartTimeSeconds >= 0.0)
        {
            const double delaySeconds = nowSeconds - ue.lastHoStartTimeSeconds;
            ue.totalHandoverExecutionDelaySeconds += delaySeconds;
            handoverDelayMs = delaySeconds * 1000.0;
        }
        if (ue.hasPendingHoStart && ue.lastSuccessfulHoTimeSeconds >= 0.0 &&
            previousSourceCell == cellId && previousTargetCell == currentSourceCell)
        {
            pingPongGapSeconds = nowSeconds - ue.lastSuccessfulHoTimeSeconds;
            if (pingPongGapSeconds <= g_pingPongWindowSeconds)
            {
                ue.pingPongCount++;
                pingPongDetected = true;
            }
        }
        if (ue.hasPendingHoStart)
        {
            ue.lastSuccessfulHoSourceCell = ue.lastHoStartSourceCell;
            ue.lastSuccessfulHoTargetCell = cellId;
            ue.lastSuccessfulHoTimeSeconds = nowSeconds;
        }
        const bool hasRecoveryThreshold =
            std::isfinite(ue.pendingRecoveryThresholdThroughputMbps) &&
            ue.pendingRecoveryThresholdThroughputMbps > 0.0;
        ue.waitingForThroughputRecovery = hasRecoveryThreshold;
        ue.waitingRecoveryHoId = hasRecoveryThreshold ? activeHandoverTraceId : 0;
        ue.waitingRecoveryStartTimeSeconds = hasRecoveryThreshold ? nowSeconds : -1.0;
        ue.waitingRecoverySatisfiedSamples = 0;
        WriteHandoverEventTraceRow(nowSeconds,
                                   *ueIdx,
                                   activeHandoverTraceId,
                                   "HO_END_OK",
                                   ue.lastHoStartSourceCell,
                                   cellId,
                                   handoverDelayMs,
                                   std::numeric_limits<double>::quiet_NaN(),
                                   pingPongDetected);
        ue.hasPendingHoStart = false;
        ue.lastHoStartTimeSeconds = -1.0;
        ue.activeHandoverTraceId = 0;
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
    if (pingPongDetected)
    {
        std::cout << "[PING-PONG] t=" << std::fixed << std::setprecision(3)
                  << Simulator::Now().GetSeconds() << "s"
                  << prefix.str()
                  << " path=" << previousSourceCell
                  << "->" << previousTargetCell
                  << "->" << cellId
                  << " currentReturn=" << currentSourceCell
                  << "->" << cellId
                  << " gap=" << std::fixed << std::setprecision(3) << pingPongGapSeconds << "s"
                  << std::endl;
    }
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
 * 周期性导出“切换窗口下的每 UE 瞬时吞吐”。
 *
 * 设计目的不是替代最终平均吞吐统计，而是保留切换前后的小时间窗曲线，
 * 便于直接画出 HO Start / HO Success 附近的吞吐坑谷。
 */
static void
SampleHandoverDlThroughput(uint32_t packetSizeBytes, Time interval)
{
    if (!g_handoverThroughputTrace.is_open())
    {
        return;
    }

    const double nowSeconds = Simulator::Now().GetSeconds();
    const double dtSeconds = interval.GetSeconds();

    for (uint32_t ueIdx = 0; ueIdx < g_ues.size(); ++ueIdx)
    {
        auto& ue = g_ues[ueIdx];
        const uint64_t currentRxPackets = ue.server ? ue.server->GetReceived() : 0;
        const uint64_t deltaPackets = currentRxPackets - ue.lastThroughputTraceRxPackets;
        ue.lastThroughputTraceRxPackets = currentRxPackets;

        uint16_t servingCellId = 0;
        if (ue.dev && ue.dev->GetRrc())
        {
            servingCellId = ue.dev->GetRrc()->GetCellId();
        }

        const double throughputMbps =
            (dtSeconds > 0.0) ? (deltaPackets * packetSizeBytes * 8.0) / dtSeconds / 1e6 : 0.0;

        if (!ue.hasPendingHoStart && !ue.waitingForThroughputRecovery)
        {
            ue.recentThroughputSamplesMbps.push_back(throughputMbps);
            while (ue.recentThroughputSamplesMbps.size() > g_recoveryReferenceWindowSamples)
            {
                ue.recentThroughputSamplesMbps.pop_front();
            }
        }

        if (!ue.hasPendingHoStart && ue.waitingForThroughputRecovery)
        {
            const double thresholdMbps = ue.pendingRecoveryThresholdThroughputMbps;
            if (std::isfinite(thresholdMbps) && throughputMbps + 1e-9 >= thresholdMbps)
            {
                ue.waitingRecoverySatisfiedSamples++;
            }
            else
            {
                ue.waitingRecoverySatisfiedSamples = 0;
            }

            if (ue.waitingRecoverySatisfiedSamples >= g_recoveryRequiredConsecutiveSamples &&
                ue.waitingRecoveryStartTimeSeconds >= 0.0)
            {
                const double recoverySeconds = nowSeconds - ue.waitingRecoveryStartTimeSeconds;
                ue.throughputRecoveryCount++;
                ue.totalThroughputRecoverySeconds += recoverySeconds;
                ue.lastThroughputRecoverySeconds = recoverySeconds;
                WriteHandoverEventTraceRow(nowSeconds,
                                           ueIdx,
                                           ue.waitingRecoveryHoId,
                                           "THROUGHPUT_RECOVERED",
                                           ue.lastSuccessfulHoSourceCell,
                                           ue.lastSuccessfulHoTargetCell,
                                           std::numeric_limits<double>::quiet_NaN(),
                                           recoverySeconds * 1000.0,
                                           false);
                ue.waitingForThroughputRecovery = false;
                ue.waitingRecoveryHoId = 0;
                ue.waitingRecoveryStartTimeSeconds = -1.0;
                ue.waitingRecoverySatisfiedSamples = 0;
                ue.pendingRecoveryReferenceThroughputMbps = std::numeric_limits<double>::quiet_NaN();
                ue.pendingRecoveryThresholdThroughputMbps = std::numeric_limits<double>::quiet_NaN();
                ue.recentThroughputSamplesMbps.clear();
                ue.recentThroughputSamplesMbps.push_back(throughputMbps);
            }
        }

        g_handoverThroughputTrace << std::fixed << std::setprecision(3) << nowSeconds << ","
                                  << ueIdx << "," << servingCellId << ","
                                  << ResolveSatelliteIndexFromCellId(servingCellId) << ","
                                  << throughputMbps << "," << deltaPackets << ","
                                  << currentRxPackets << "," << (ue.hasPendingHoStart ? 1 : 0)
                                  << "," << ue.activeHandoverTraceId << ","
                                  << ue.lastHoStartSourceCell << "," << ue.lastHoStartTargetCell
                                  << "\n";
    }

    Simulator::Schedule(interval, &SampleHandoverDlThroughput, packetSizeBytes, interval);
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

// ============================================================================
// 周期更新主循环
// ============================================================================

/**
 * 周期更新整张星座和所有 UE 的观测状态。
 *
 * 这是整个场景的“时间推进核心”：
 * - 更新卫星位置；
 * - 计算每个 UE 到所有卫星的链路预算；
 * - 根据实时服务小区统计卫星负载；
 * - 为测量驱动切换链提供最新运行时上下文。
 */
static void
UpdateConstellation(Time interval, Time stopTime)
{
    const double nowSeconds = Simulator::Now().GetSeconds();
    std::vector<uint32_t> nextAttachedUeCounts(g_satellites.size(), 0);

    std::vector<LeoOrbitCalculator::OrbitState> referenceStates(g_satellites.size());
    for (uint32_t i = 0; i < g_satellites.size(); ++i)
    {
        referenceStates[i] = LeoOrbitCalculator::CalculateSatelliteState(
            nowSeconds, g_satellites[i].orbit, g_gmstAtEpochRad);
        // 卫星节点位置直接用几何计算结果覆盖，形成时变轨道运动。
        g_satellites[i].node->GetObject<MobilityModel>()->SetPosition(referenceStates[i].ecef);
        UpdateSatelliteAnchorFromGrid(i, referenceStates[i].ecef, nowSeconds, false);
    }
    FlushSatelliteAnchorTraceRows(nowSeconds);

    for (uint32_t ueIdx = 0; ueIdx < g_ues.size(); ++ueIdx)
    {
        auto& ue = g_ues[ueIdx];
        uint16_t servingCellId = 0;
        if (ue.dev && ue.dev->GetRrc())
        {
            servingCellId = ue.dev->GetRrc()->GetCellId();
        }

        const int32_t servingSatIdx = ResolveSatelliteIndexFromCellId(servingCellId);
        if (servingSatIdx >= 0 && static_cast<uint32_t>(servingSatIdx) < g_satellites.size())
        {
            nextAttachedUeCounts[servingSatIdx]++;
        }

        const bool servingChanged =
            (ue.lastServingCellForLog != 0 && servingCellId != 0 &&
             servingCellId != ue.lastServingCellForLog);
        if (servingChanged)
        {
            std::cout << "[SERVING] t=" << std::fixed << std::setprecision(3) << nowSeconds << "s ue="
                      << ueIdx << " cell " << ue.lastServingCellForLog << " -> " << servingCellId
                      << std::endl;
        }

        const bool periodic = g_printKpiReports &&
                              ((ue.lastKpiReportTime < 0.0) ||
                               (nowSeconds - ue.lastKpiReportTime >=
                                g_kpiIntervalSeconds - 1e-9));
        if (periodic)
        {
            std::ostringstream kpi;
            kpi << std::fixed << std::setprecision(3);
            kpi << "[KPI] t=" << nowSeconds << "s"
                << " ue=" << ueIdx
                << " servingCell=" << servingCellId;
            if (servingSatIdx >= 0 && static_cast<uint32_t>(servingSatIdx) < g_satellites.size())
            {
                kpi << " servingSat=sat" << servingSatIdx
                    << " loadScore=" << g_satellites[servingSatIdx].loadScore
                    << " attachedUeCount=" << g_satellites[servingSatIdx].attachedUeCount;
            }
            std::cout << kpi.str() << std::endl;
            ue.lastKpiReportTime = nowSeconds;
        }

        ue.lastServingCellForLog = servingCellId;
    }

    for (uint32_t satIdx = 0; satIdx < g_satellites.size(); ++satIdx)
    {
        g_satellites[satIdx].attachedUeCount = nextAttachedUeCounts[satIdx];
    }
    UpdateSatelliteLoadStats(
        g_satellites, g_offeredPacketRatePerUe, g_maxSupportedUesPerSatellite, g_loadCongestionThreshold);

    if (Simulator::Now() + interval <= stopTime)
    {
        Simulator::Schedule(interval, &UpdateConstellation, interval, stopTime);
    }
}

/**
 * 将解析后的配置同步到主脚本使用的全局镜像变量。
 *
 * 这些变量主要服务于周期更新、日志输出和测量驱动切换链。
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
    g_hoHysteresisDb = config.hoHysteresisDb;
    g_pingPongWindowSeconds = config.pingPongWindowSeconds;
    g_measurementReportIntervalMs = config.measurementReportIntervalMs;
    g_measurementMaxReportCells =
        static_cast<uint8_t>(std::clamp<uint16_t>(config.measurementMaxReportCells, 1, 32));
    g_handoverMode = ParseHandoverMode(config.handoverMode);
    g_improvedSignalWeight = config.improvedSignalWeight;
    g_improvedLoadWeight = config.improvedLoadWeight;
    g_improvedVisibilityWeight = config.improvedVisibilityWeight;
    g_improvedMinLoadScoreDelta = config.improvedMinLoadScoreDelta;
    g_improvedMaxSignalGapDb = config.improvedMaxSignalGapDb;
    g_improvedReturnGuardSeconds = config.improvedReturnGuardSeconds;
    g_improvedMinVisibilitySeconds = config.improvedMinVisibilitySeconds;
    g_improvedVisibilityHorizonSeconds = config.improvedVisibilityHorizonSeconds;
    g_improvedVisibilityPredictionStepSeconds = config.improvedVisibilityPredictionStepSeconds;
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

    ResolveBaselineOutputPaths(config);
    ValidateBaselineSimulationConfig(config);
    ApplyBaselineDerivedLocationConfig(config);

    const auto& cfg = config;
    double orbitRaanDeg = cfg.orbitRaanDeg;
    double baseTrueAnomalyDeg = cfg.baseTrueAnomalyDeg;

    // ------------------------------
    // 3. 几何场景与链路预算参数初始化
    // ------------------------------
    g_ueGroundPoint = LeoOrbitCalculator::CreateGroundPoint(
        cfg.ueLatitudeDeg, cfg.ueLongitudeDeg, cfg.ueAltitudeMeters);
    g_cellGroundPoint =
        LeoOrbitCalculator::CreateGroundPoint(cfg.cellLatitudeDeg, cfg.cellLongitudeDeg, cfg.cellAltitudeMeters);
    g_gmstAtEpochRad = LeoOrbitCalculator::DegToRad(cfg.gmstAtEpochDeg);
    g_minElevationRad = LeoOrbitCalculator::DegToRad(cfg.minElevationDeg);
    g_carrierFrequencyHz = cfg.centralFrequency;
    g_beamModelConfig.carrierFrequencyHz = cfg.centralFrequency;
    g_beamModelConfig.txPowerDbm = cfg.gnbTxPower;
    g_beamModelConfig.gMax0Dbi = cfg.beamMaxGainDbi;
    g_beamModelConfig.alphaMaxRad = LeoOrbitCalculator::DegToRad(cfg.scanMaxDeg);
    g_beamModelConfig.theta3dBRad = LeoOrbitCalculator::DegToRad(cfg.theta3dBDeg);
    g_beamModelConfig.slaVDb = cfg.sideLobeAttenuationDb;
    g_beamModelConfig.rxGainDbi = cfg.ueRxGainDbi;
    g_beamModelConfig.atmLossDb = cfg.atmLossDb;
    ApplyGlobalMirrorConfig(config);

    const bool outputDirsReady = EnsureParentDirectoryForFile(g_gridCatalogPath) &&
                                 EnsureParentDirectoryForFile(cfg.satAnchorTracePath) &&
                                 EnsureParentDirectoryForFile(cfg.handoverThroughputTracePath) &&
                                 EnsureParentDirectoryForFile(cfg.handoverEventTracePath) &&
                                 EnsureParentDirectoryForFile(cfg.ueLayoutPath) &&
                                 EnsureParentDirectoryForFile(cfg.gridSvgPath);
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

        if (cfg.startupVerbose)
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
            if (cfg.startupVerbose)
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
              << " alphaMax=" << cfg.scanMaxDeg << "deg"
              << " theta3dB=" << cfg.theta3dBDeg << "deg" << std::endl;
    std::cout << "[A3-Measure] source=PHY MeasurementReport"
              << " reportInterval=" << g_measurementReportIntervalMs << "ms"
              << " maxReportCells=" << static_cast<uint32_t>(g_measurementMaxReportCells)
              << std::endl;
    std::cout << "[Constellation] satellites=" << cfg.gNbNum
              << " planes=" << cfg.orbitPlaneCount
              << " ue=" << cfg.ueNum
              << " raanSpacing=" << cfg.interPlaneRaanSpacingDeg << "deg"
              << " planeTimeOffset=" << cfg.interPlaneTimeOffsetSeconds << "s" << std::endl;
    std::cout << "[UE-Layout] type=" << cfg.ueLayoutType;
    if (cfg.ueLayoutType == "seven-cell")
    {
        std::cout << " groups=center(9)+ring(16 across 6 cells)"
                  << " hexRadius=" << cfg.hexCellRadiusKm << "km"
                  << " centerSpacing=" << cfg.ueCenterSpacingMeters / 1000.0 << "km"
                  << " ringPointOffset=" << cfg.ueRingPointOffsetMeters / 1000.0 << "km";
    }
    else
    {
        std::cout << " spacing=" << cfg.ueSpacingMeters / 1000.0 << "km";
    }
    std::cout << std::endl;
    std::cout << "[Handover] a3=measurement-report"
              << " mode=" << ToString(g_handoverMode)
              << " hysteresis=" << g_hoHysteresisDb << "dB"
              << " ttt=" << static_cast<double>(cfg.hoTttMs) / 1000.0 << "s"
              << " improvedSignalWeight=" << g_improvedSignalWeight
              << " improvedLoadWeight=" << g_improvedLoadWeight
              << " pingPongWindow=" << g_pingPongWindowSeconds << "s";
    if (g_handoverMode == HandoverMode::IMPROVED)
    {
        std::cout << " improvedVisibilityWeight=" << g_improvedVisibilityWeight
                  << " improvedMinVisibility=" << g_improvedMinVisibilitySeconds << "s"
                  << " improvedVisibilityHorizon=" << g_improvedVisibilityHorizonSeconds << "s"
                  << " improvedMinLoadScoreDelta=" << g_improvedMinLoadScoreDelta
                  << " improvedMaxSignalGapDb=" << g_improvedMaxSignalGapDb
                  << " improvedReturnGuard=" << g_improvedReturnGuardSeconds << "s"
                  << " improvedVisibilityStep=" << g_improvedVisibilityPredictionStepSeconds
                  << "s";
    }
    std::cout << std::endl;
    std::cout << "[UserPlane] rlc="
              << (cfg.forceRlcAmForEpc ? "AM(default override)" : "helper default")
              << " ueIpv4Forwarding=" << (cfg.disableUeIpv4Forwarding ? "OFF" : "ON")
              << std::endl;
    std::cout << "[Output] dir=" << g_outputDir << std::endl;
    if (cfg.enableHandoverThroughputTrace)
    {
        std::cout << "[Output] handoverThroughputTrace=" << cfg.handoverThroughputTracePath
                  << " eventTrace=" << cfg.handoverEventTracePath
                  << " sampleInterval=" << cfg.handoverThroughputTraceIntervalSeconds << "s"
                  << std::endl;
    }

    const double semiMajorAxisMeters =
        LeoOrbitCalculator::kWgs84SemiMajorAxisMeters + cfg.satAltitudeMeters;
    const double meanMotionRadPerSec =
        std::sqrt(LeoOrbitCalculator::kEarthGravitationalMu / std::pow(semiMajorAxisMeters, 3.0));

    const double inclinationRad = LeoOrbitCalculator::DegToRad(cfg.orbitInclinationDeg);
    double raanRad = LeoOrbitCalculator::DegToRad(orbitRaanDeg);
    const double argPerigeeRad = LeoOrbitCalculator::DegToRad(cfg.orbitArgPerigeeDeg);
    double baseTrueAnomalyRad = LeoOrbitCalculator::DegToRad(baseTrueAnomalyDeg);

    if (cfg.autoAlignToUe)
    {
        const auto aligned = AutoAlignOrbitToUe(cfg.alignmentReferenceTimeSeconds,
                                                cfg.descendingPass,
                                                semiMajorAxisMeters,
                                                cfg.orbitEccentricity,
                                                inclinationRad,
                                                argPerigeeRad,
                                                raanRad,
                                                baseTrueAnomalyRad,
                                                meanMotionRadPerSec,
                                                g_gmstAtEpochRad,
                                                g_ueGroundPoint,
                                                cfg.centralFrequency,
                                                g_minElevationRad);
        raanRad = aligned.raanRad;
        baseTrueAnomalyRad = aligned.baseTrueAnomalyRad;
        orbitRaanDeg = LeoOrbitCalculator::RadToDeg(raanRad);
        baseTrueAnomalyDeg = LeoOrbitCalculator::RadToDeg(baseTrueAnomalyRad);

        std::cout << "[Setup] autoAlign=ON branch="
                  << (cfg.descendingPass ? "descending" : "ascending")
                  << " peakEl=" << LeoOrbitCalculator::RadToDeg(aligned.peakElevationRad) << "deg" << std::endl;
    }
    if (cfg.orbitPlaneCount > 1)
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
    if (cfg.forceRlcAmForEpc)
    {
        Config::SetDefault("ns3::NrGnbRrc::EpsBearerToRlcMapping",
                           EnumValue(NrGnbRrc::RLC_AM_ALWAYS));
    }

    if (g_printOrbitCheck)
    {
        std::cout << "=== Orbit math check: Kepler/ECI/ECEF/ENU (0~100s, step 10s) ===" << std::endl;
    }
    LeoOrbitCalculator::KeplerElements checkOrbit;
    checkOrbit.semiMajorAxisMeters = semiMajorAxisMeters;
    checkOrbit.eccentricity = cfg.orbitEccentricity;
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
                                                         cfg.centralFrequency,
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

    Ptr<NrChannelHelper> channelHelper = CreateObject<NrChannelHelper>();
    channelHelper->ConfigureFactories("NTN-Rural", "LOS", "ThreeGpp");
    channelHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(true));

    CcBwpCreator ccBwpCreator;
    CcBwpCreator::SimpleOperationBandConf bandConf(cfg.centralFrequency, cfg.bandwidth, 1);
    OperationBandInfo band = ccBwpCreator.CreateOperationBandContiguousCc(bandConf);
    channelHelper->AssignChannelsToBands({band});
    BandwidthPartInfoPtrVector allBwps = CcBwpCreator::GetAllBwps({band});

    nrHelper->SetGnbPhyAttribute("TxPower", DoubleValue(cfg.gnbTxPower));
    nrHelper->SetUePhyAttribute("TxPower", DoubleValue(cfg.ueTxPower));

    nrHelper->SetSchedulerTypeId(TypeId::LookupByName("ns3::NrMacSchedulerTdmaRR"));
    nrHelper->SetSchedulerAttribute("EnableSrsInFSlots", BooleanValue(cfg.enableSrsInFSlots));
    nrHelper->SetSchedulerAttribute("EnableSrsInUlSlots", BooleanValue(cfg.enableSrsInUlSlots));
    nrHelper->SetSchedulerAttribute("SrsSymbols", UintegerValue(cfg.srsSymbols));
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
    gNbNodes.Create(cfg.gNbNum);
    NodeContainer ueNodes;
    ueNodes.Create(cfg.ueNum);

    MobilityHelper gnbMobility;
    gnbMobility.SetMobilityModel("ns3::GeocentricConstantPositionMobilityModel");
    gnbMobility.Install(gNbNodes);

    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::GeocentricConstantPositionMobilityModel");
    ueMobility.Install(ueNodes);

    UeLayoutConfig ueLayout;
    ueLayout.layoutType = cfg.ueLayoutType;
    ueLayout.lineSpacingMeters = cfg.ueSpacingMeters;
    ueLayout.hexCellRadiusMeters = cfg.hexCellRadiusKm * 1000.0;
    ueLayout.centerSpacingMeters = cfg.ueCenterSpacingMeters;
    ueLayout.ringPointOffsetMeters = cfg.ueRingPointOffsetMeters;
    const auto uePlacements =
        BuildUePlacements(cfg.ueLatitudeDeg, cfg.ueLongitudeDeg, cfg.ueAltitudeMeters, cfg.ueNum, ueLayout);
    NS_ABORT_MSG_IF(uePlacements.size() != cfg.ueNum, "UE placement count does not match ueNum");
    DumpUeLayoutCsv(cfg.ueLayoutPath, uePlacements);
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        ueNodes.Get(i)->GetObject<MobilityModel>()->SetPosition(uePlacements[i].groundPoint.ecef);
    }

    NetDeviceContainer gNbNetDev = nrHelper->InstallGnbDevice(gNbNodes, allBwps);
    NetDeviceContainer ueNetDev = nrHelper->InstallUeDevice(ueNodes, allBwps);

    int64_t randomStream = 1;
    randomStream += nrHelper->AssignStreams(gNbNetDev, randomStream);
    randomStream += nrHelper->AssignStreams(ueNetDev, randomStream);

    // 先一次性建立完整 X2 网格；随后为每个 gNB 安装静态邻区关系。
    for (uint32_t i = 0; i < gNbNodes.GetN(); ++i)
    {
        for (uint32_t j = i + 1; j < gNbNodes.GetN(); ++j)
        {
            nrEpcHelper->AddX2Interface(gNbNodes.Get(i), gNbNodes.Get(j));
        }
    }

    g_satellites.clear();
    g_cellToSatellite.clear();
    g_satellites.reserve(cfg.gNbNum);
    std::vector<uint32_t> satellitesPerPlane(cfg.orbitPlaneCount, cfg.gNbNum / cfg.orbitPlaneCount);
    for (uint32_t planeIdx = 0; planeIdx < (cfg.gNbNum % cfg.orbitPlaneCount); ++planeIdx)
    {
        satellitesPerPlane[planeIdx]++;
    }

    // 为每颗卫星绑定轨道、初始位置和初始网格锚点。
    uint32_t globalSatIdx = 0;
    for (uint32_t planeIdx = 0; planeIdx < cfg.orbitPlaneCount; ++planeIdx)
    {
        const uint32_t satsInPlane = satellitesPerPlane[planeIdx];
        const double planeCenterIndex = (static_cast<double>(satsInPlane) - 1.0) / 2.0;
        const double planeRaanRad =
            LeoOrbitCalculator::NormalizeAngle(raanRad +
                                               LeoOrbitCalculator::DegToRad(cfg.interPlaneRaanSpacingDeg) * planeIdx);
        const double planeRaanDeg = LeoOrbitCalculator::RadToDeg(planeRaanRad);
        const double planeTimeOffset = cfg.interPlaneTimeOffsetSeconds * planeIdx;

        for (uint32_t slotIdx = 0; slotIdx < satsInPlane; ++slotIdx, ++globalSatIdx)
        {
            auto dev = DynamicCast<NrGnbNetDevice>(gNbNetDev.Get(globalSatIdx));
            auto rrc = dev->GetRrc();

            const double overpassTime =
                cfg.alignmentReferenceTimeSeconds +
                (static_cast<double>(slotIdx) - planeCenterIndex) * cfg.overpassGapSeconds +
                cfg.overpassTimeOffsetSeconds + planeTimeOffset;
            LeoOrbitCalculator::KeplerElements orbit;
            orbit.semiMajorAxisMeters = semiMajorAxisMeters;
            orbit.eccentricity = cfg.orbitEccentricity;
            orbit.inclinationRad = inclinationRad;
            orbit.raanRad = planeRaanRad;
            orbit.argPerigeeRad = argPerigeeRad;
            // 对近圆 LEO 而言，平移真近点角可近似实现按时间错开的过顶相位。
            orbit.trueAnomalyAtEpochRad = baseTrueAnomalyRad - meanMotionRadPerSec * overpassTime;

            const auto initState = LeoOrbitCalculator::Calculate(0.0,
                                                                 orbit,
                                                                 g_gmstAtEpochRad,
                                                                 g_ueGroundPoint,
                                                                 cfg.centralFrequency,
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

            if (cfg.startupVerbose && g_useWgs84HexGrid && !g_hexGridCells.empty())
            {
                UpdateSatelliteAnchorFromGrid(globalSatIdx, initState.ecef, 0.0, true);
            }

            std::cout << "[Setup] sat" << globalSatIdx << " plane=" << planeIdx << " slot=" << slotIdx
                      << " cell=" << dev->GetCellId();
            if (cfg.startupVerbose)
            {
                std::cout << " nu0=" << LeoOrbitCalculator::RadToDeg(orbit.trueAnomalyAtEpochRad) << "deg"
                          << " i=" << cfg.orbitInclinationDeg << "deg"
                          << " RAAN=" << planeRaanDeg << "deg";
                if (g_useWgs84HexGrid && g_satellites[globalSatIdx].currentAnchorGridId != 0)
                {
                    std::cout << " anchorGrid=" << g_satellites[globalSatIdx].currentAnchorGridId;
                }
            }
            std::cout << std::endl;
        }
    }

    InstallStaticNeighbourRelations();
    InstallMeasurementDrivenHandoverAlgorithms(cfg);

    auto remoteHostWithAddr = nrEpcHelper->SetupRemoteHost("100Gb/s", 2500, Seconds(0.0));
    Ptr<Node> remoteHost = remoteHostWithAddr.first;

    InternetStackHelper internet;
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIpIface = nrEpcHelper->AssignUeIpv4Address(NetDeviceContainer(ueNetDev));
    Ipv4StaticRoutingHelper routingHelper;
    if (cfg.disableUeIpv4Forwarding)
    {
        for (uint32_t ueIdx = 0; ueIdx < ueNodes.GetN(); ++ueIdx)
        {
            Ptr<Ipv4> ueIpv4 = ueNodes.Get(ueIdx)->GetObject<Ipv4>();
            if (!ueIpv4)
            {
                continue;
            }
            for (uint32_t ifIdx = 0; ifIdx < ueIpv4->GetNInterfaces(); ++ifIdx)
            {
                ueIpv4->SetForwarding(ifIdx, false);
            }
        }
    }

    g_ues.clear();
    g_ues.reserve(cfg.ueNum);
    g_imsiToUe.clear();

    // ------------------------------
    // 7. UE 初始接入与业务安装
    // ------------------------------
    // 为每个 UE 单独选择初始服务星和业务端口。
    for (uint32_t ueIdx = 0; ueIdx < cfg.ueNum; ++ueIdx)
    {
        UeRuntime ue;
        ue.node = ueNodes.Get(ueIdx);
        ue.dev = DynamicCast<NrUeNetDevice>(ueNetDev.Get(ueIdx));
        ue.groundPoint = uePlacements[ueIdx].groundPoint;
        ue.placementRole = uePlacements[ueIdx].role;
        ue.eastOffsetMeters = uePlacements[ueIdx].eastOffsetMeters;
        ue.northOffsetMeters = uePlacements[ueIdx].northOffsetMeters;
        ResetUeRuntime(ue, cfg.gNbNum);

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
                                                             cfg.centralFrequency,
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

        nrHelper->AttachToGnb(ueNetDev.Get(ueIdx), gNbNetDev.Get(initialAttachIdx));

        Ptr<Ipv4StaticRouting> ueStaticRouting =
            routingHelper.GetStaticRouting(ueNodes.Get(ueIdx)->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(nrEpcHelper->GetUeDefaultGatewayAddress(), 1);

        const uint16_t dlPort = static_cast<uint16_t>(1234 + ueIdx);
        UdpServerHelper ueUdpServer(dlPort);
        ApplicationContainer serverApps = ueUdpServer.Install(ueNodes.Get(ueIdx));

        UdpClientHelper dlClient(ueIpIface.GetAddress(ueIdx), dlPort);
        dlClient.SetAttribute("PacketSize", UintegerValue(cfg.udpPacketSize));
        dlClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
        dlClient.SetAttribute("Interval", TimeValue(Seconds(1.0 / cfg.lambda)));
        ApplicationContainer clientApps = dlClient.Install(remoteHost);

        // 为每个 UE 建立独立的专用承载，确保业务流和切换统计可区分。
        NrEpsBearer bearer(NrEpsBearer::GBR_CONV_VOICE);
        Ptr<NrEpcTft> tft = Create<NrEpcTft>();
        NrEpcTft::PacketFilter dlpf;
        dlpf.localPortStart = dlPort;
        dlpf.localPortEnd = dlPort;
        tft->Add(dlpf);
        nrHelper->ActivateDedicatedEpsBearer(ueNetDev.Get(ueIdx), bearer, tft);

        serverApps.Start(Seconds(cfg.appStartTime));
        clientApps.Start(Seconds(cfg.appStartTime));
        serverApps.Stop(Seconds(cfg.simTime));
        clientApps.Stop(Seconds(cfg.simTime));

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
    ResetRuntimeState(cfg.gNbNum);
    if (g_satAnchorTrace.is_open())
    {
        g_satAnchorTrace.close();
    }
    if (g_handoverThroughputTrace.is_open())
    {
        g_handoverThroughputTrace.close();
    }
    if (g_handoverEventTrace.is_open())
    {
        g_handoverEventTrace.close();
    }
    g_satAnchorTrace.open(cfg.satAnchorTracePath, std::ios::out | std::ios::trunc);
    NS_ABORT_MSG_IF(!g_satAnchorTrace.is_open(),
                    "Failed to open satellite anchor trace CSV: " << cfg.satAnchorTracePath);
    g_satAnchorTrace
        << "time_s,sat,plane,slot,cell,anchor_grid_id,anchor_latitude_deg,anchor_longitude_deg,"
        << "anchor_east_m,anchor_north_m\n";
    if (cfg.enableHandoverThroughputTrace)
    {
        g_handoverThroughputTrace.open(cfg.handoverThroughputTracePath,
                                       std::ios::out | std::ios::trunc);
        NS_ABORT_MSG_IF(!g_handoverThroughputTrace.is_open(),
                        "Failed to open handover throughput trace CSV: "
                            << cfg.handoverThroughputTracePath);
        g_handoverThroughputTrace
            << "time_s,ue,serving_cell,serving_sat,throughput_mbps,delta_rx_packets,"
            << "total_rx_packets,in_handover,active_ho_id,pending_source_cell,pending_target_cell\n";

        g_handoverEventTrace.open(cfg.handoverEventTracePath, std::ios::out | std::ios::trunc);
        NS_ABORT_MSG_IF(!g_handoverEventTrace.is_open(),
                        "Failed to open handover event trace CSV: " << cfg.handoverEventTracePath);
        g_handoverEventTrace
            << "time_s,ue,ho_id,event,source_cell,target_cell,source_sat,target_sat,delay_ms,recovery_ms,"
            << "ping_pong_detected\n";
        g_handoverEventTrace.flush();
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
        ue.lastThroughputTraceRxPackets = 0;
    }
    if (cfg.throughputReportIntervalSeconds > 0.0)
    {
        Simulator::Schedule(Seconds(cfg.appStartTime + cfg.throughputReportIntervalSeconds),
                            &PrintDlThroughput,
                            cfg.udpPacketSize,
                            Seconds(cfg.throughputReportIntervalSeconds));
    }
    if (cfg.enableHandoverThroughputTrace)
    {
        const double firstTraceSampleTimeSeconds =
            cfg.appStartTime + 0.5 * cfg.handoverThroughputTraceIntervalSeconds;
        Simulator::Schedule(Seconds(firstTraceSampleTimeSeconds),
                            &SampleHandoverDlThroughput,
                            cfg.udpPacketSize,
                            Seconds(cfg.handoverThroughputTraceIntervalSeconds));
    }

    for (uint32_t ueIdx = 0; ueIdx < g_ues.size(); ++ueIdx)
    {
        const auto& ue = g_ues[ueIdx];
        std::cout << "[Setup] ue" << ueIdx << " start attach=sat" << ue.initialAttachIdx
                  << "(cell=" << g_satellites[ue.initialAttachIdx].dev->GetCellId() << ")"
                  << " role=" << ue.placementRole
                  << " offset=(" << ue.eastOffsetMeters / 1000.0 << "kmE,"
                  << ue.northOffsetMeters / 1000.0 << "kmN)"
                  << " handoverMode=" << ToString(g_handoverMode);
        std::cout << std::endl;
    }

    // `UpdateConstellation` 是整个场景的时变主循环。
    Simulator::Schedule(Seconds(0.0),
                        &UpdateConstellation,
                        MilliSeconds(cfg.updateIntervalMs),
                        Seconds(cfg.simTime));
    if (g_printSimulationProgress)
    {
        Simulator::Schedule(Seconds(std::min(g_progressReportIntervalSeconds, cfg.simTime)),
                            &PrintSimulationProgress,
                            Seconds(g_progressReportIntervalSeconds));
    }

    // ------------------------------
    // 9. 仿真运行与结果输出
    // ------------------------------
    Simulator::Stop(Seconds(cfg.simTime));
    Simulator::Run();

    const double appDuration = std::max(0.0, cfg.simTime - cfg.appStartTime);
    PrintDlTrafficSummary(g_ues, appDuration, cfg.udpPacketSize);
    PrintHandoverSummary(g_ues, g_pingPongWindowSeconds);

    if (g_satAnchorTrace.is_open())
    {
        g_satAnchorTrace.close();
    }
    if (g_handoverThroughputTrace.is_open())
    {
        g_handoverThroughputTrace.close();
    }
    if (g_handoverEventTrace.is_open())
    {
        g_handoverEventTrace.close();
    }

    if (cfg.runGridSvgScript)
    {
        if (!g_useWgs84HexGrid || !g_printGridCatalog)
        {
            std::cout << "[GridSvg] skipped because hex-grid catalog export is disabled" << std::endl;
        }
        else
        {
            std::ostringstream cmdline;
            cmdline << "python3 \"" << cfg.plotHexGridScriptPath << "\""
                    << " --csv \"" << g_gridCatalogPath << "\""
                    << " --sat-anchor-csv \"" << cfg.satAnchorTracePath << "\""
                    << " --out \"" << cfg.gridSvgPath << "\"";
            const int scriptRc = std::system(cmdline.str().c_str());
            if (scriptRc == 0)
            {
                std::cout << "[GridSvg] SVG generated successfully" << std::endl;
            }
            else
            {
                std::cout << "[GridSvg] SVG generation failed, rc=" << scriptRc << std::endl;
            }
        }
    }

    Simulator::Destroy();
    return 0;
}
