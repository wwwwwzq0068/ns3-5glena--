#include "ns3/antenna-module.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/geocentric-constant-position-mobility-model.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-leo-a3-measurement-handover-algorithm.h"
#include "ns3/nr-gnb-rrc.h"
#include "ns3/nr-module.h"
#include "ns3/nr-ue-rrc.h"
#include "handover/beam-link-budget.h"
#include "handover/leo-ntn-handover-config.h"
#include "handover/leo-ntn-handover-decision.h"
#include "handover/leo-ntn-handover-output.h"
#include "handover/leo-ntn-handover-radio.h"
#include "handover/leo-orbit-calculator.h"
#include "handover/leo-ntn-handover-reporting.h"
#include "handover/leo-ntn-handover-runtime.h"
#include "handover/leo-ntn-handover-scenario.h"
#include "handover/leo-ntn-handover-update.h"
#include "handover/b00-equivalent-antenna-model.h"
#include "handover/earth-fixed-beam-target.h"
#include "handover/earth-fixed-gnb-beamforming.h"
#include "handover/grid-anchor-landing-rule.h"
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

// 卫星全局索引到 gNB 设备的显式映射（用于 carrier reuse 模式）。
static std::vector<Ptr<NrGnbNetDevice>> g_satelliteGnbDevices;

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
static double g_anchorGridHexRadiusKm = 20.0;
// 每颗卫星用于锚点选择的最近网格候选数。
static uint32_t g_gridNearestK = 3;
// 是否禁止多个卫星波束锚点落在同一小区或彼此的一圈邻区内。
static bool g_enforceBeamExclusionRing = true;
// 启用波束排他时，用于搜索可用锚点的最近候选数。
static uint32_t g_beamExclusionCandidateK = 32;
// 是否在真实接入/切换候选上强制检查 UE 是否落在当前锚点波束覆盖内。
static bool g_enforceBeamCoverageForRealLinks = true;
// 是否进一步要求 UE 最近的 hex 小区必须等于候选卫星当前锚点小区。
static bool g_enforceAnchorCellForRealLinks = true;
// 是否优先把卫星锚点分配到实际存在 UE 的 hex 小区。
static bool g_preferDemandAnchorCells = true;
static EarthFixedBeamTargetMode g_earthFixedBeamTargetMode =
    EarthFixedBeamTargetMode::NADIR_CONTINUOUS;
// 新锚点至少需要领先多少距离，才会进入切换门控。
static double g_anchorGridSwitchGuardMeters = 0.0;
// 新锚点需要持续领先多久，才允许真正切换。
static double g_anchorGridHysteresisSeconds = 0.0;
static std::string g_outputDir = "scratch/results";
static bool g_printGridCatalog = true;
static std::string g_gridCatalogPath = JoinOutputPath(g_outputDir, "hex_grid_cells.csv");
static double g_phyDlTbIntervalSeconds = 1.0;
static std::map<uint64_t, PhyDlTbIntervalAccumulator> g_phyDlTbIntervals;
static bool g_enablePhyDlTbTrace = false;
static std::ofstream g_phyDlTbTrace;
// 预生成的六边形网格目录。
static std::vector<Wgs84HexGridCell> g_hexGridCells;
// 当前 UE 布局实际占用的 hex 小区。
static std::set<uint32_t> g_demandGridIds;
// 每个需求 hex 小区内的真实驻留 UE 数量。
static std::map<uint32_t, uint32_t> g_demandGridUeCounts;
// 运行时 anchor 选择使用的 demand 优先级权重。
static std::map<uint32_t, uint32_t> g_demandGridPriorityWeights;

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
static double g_offeredPacketRatePerUe = 250.0;
static double g_maxSupportedUesPerSatellite = 5.0;
static double g_loadCongestionThreshold = 0.8;
static double g_hoHysteresisDb = 2.0;
// 将 A->B->A 识别为 ping-pong 的时间窗口。
static double g_pingPongWindowSeconds = 1.5;

enum class AnchorSelectionMode
{
    DEMAND_NEAREST,
    DEMAND_MAX_UE_NEAR_NADIR,
};

static AnchorSelectionMode g_anchorSelectionMode = AnchorSelectionMode::DEMAND_NEAREST;

enum class DemandSnapshotMode
{
    STATIC_UE_LAYOUT,
    RUNTIME_UNDERSERVED_UE,
};

static DemandSnapshotMode g_demandSnapshotMode = DemandSnapshotMode::RUNTIME_UNDERSERVED_UE;

struct MeasurementDrivenHandoverConfig
{
    HandoverMode handoverMode = HandoverMode::BASELINE;
    double improvedSignalWeight = 0.7;
    double improvedRsrqWeight = 0.3;
    double improvedLoadWeight = 0.3;
    double improvedVisibilityWeight = 0.2;
    double improvedMinLoadScoreDelta = 0.2;
    double improvedMaxSignalGapDb = 3.0;
    double improvedMinStableLeadTimeSeconds = 0.12;
    double improvedMinVisibilitySeconds = 1.0;
    double improvedVisibilityHorizonSeconds = 8.0;
    double improvedVisibilityPredictionStepSeconds = 0.5;
    double improvedMinJointScoreMargin = 0.03;
    double improvedMinCandidateRsrpDbm = -110.0;
    double improvedMinCandidateRsrqDb = -17.0;
    double improvedServingWeakRsrpDbm = -108.0;
    double improvedServingWeakRsrqDb = -15.0;
    double improvedMinRsrqAdvantageDb = 0.0;
    bool improvedEnableCrossLayerPhyAssist = false;
    double improvedCrossLayerPhyAlpha = 0.02;
    double improvedCrossLayerTblerThreshold = 0.48;
    double improvedCrossLayerSinrThresholdDb = -5.0;
    uint32_t improvedCrossLayerMinSamples = 50;
    uint16_t measurementReportIntervalMs = 120;
    uint8_t measurementMaxReportCells = 8;
};

static MeasurementDrivenHandoverConfig g_measurementDrivenConfig;
static std::vector<Ptr<NrLeoA3MeasurementHandoverAlgorithm>> g_handoverAlgorithms;

// 逐时刻卫星波束锚点导出文件。
static std::ofstream g_satAnchorTrace;
// 逐时刻卫星真实连续地面投影导出文件。
static std::ofstream g_satGroundTrackTrace;
// 切换窗口下行吞吐采样导出文件。
static std::ofstream g_handoverThroughputTrace;
// 精确记录 HO-START / HO-END-OK 时刻的事件导出文件。
static std::ofstream g_handoverEventTrace;

static std::string
FormatUintList(const std::vector<uint32_t>& values)
{
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < values.size(); ++i)
    {
        if (i > 0)
        {
            oss << ",";
        }
        oss << values[i];
    }
    oss << "]";
    return oss.str();
}

static double
DistanceMeters(const Vector& a, const Vector& b)
{
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    const double dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

static std::optional<uint32_t> FindNearestGridCellId(const Vector& pointEcef);

static Vector
ResolveSatelliteBeamTargetEcef(uint32_t satIdx, const Vector& satEcef)
{
    if (satIdx >= g_satellites.size())
    {
        return ProjectNadirToWgs84(satEcef);
    }
    if (g_earthFixedBeamTargetMode == EarthFixedBeamTargetMode::NADIR_CONTINUOUS &&
        g_useWgs84HexGrid && !g_hexGridCells.empty() && g_satellites[satIdx].currentAnchorGridId != 0)
    {
        return ResolveDiscreteSafeNadirBeamTarget(satEcef,
                                                  g_hexGridCells,
                                                  g_satellites[satIdx].currentAnchorGridId,
                                                  g_gridCenterLatitudeDeg,
                                                  g_gridCenterLongitudeDeg,
                                                  g_anchorGridHexRadiusKm * 1000.0);
    }
    return ResolveEarthFixedBeamTarget(
        satEcef, g_satellites[satIdx].cellAnchorEcef, g_earthFixedBeamTargetMode);
}

static Vector
EcefDeltaToGridCenterEnu(const Vector& deltaEcef)
{
    const double latRad = LeoOrbitCalculator::DegToRad(g_gridCenterLatitudeDeg);
    const double lonRad = LeoOrbitCalculator::DegToRad(g_gridCenterLongitudeDeg);
    const double sinLat = std::sin(latRad);
    const double cosLat = std::cos(latRad);
    const double sinLon = std::sin(lonRad);
    const double cosLon = std::cos(lonRad);

    const double east = -sinLon * deltaEcef.x + cosLon * deltaEcef.y;
    const double north =
        -sinLat * cosLon * deltaEcef.x - sinLat * sinLon * deltaEcef.y + cosLat * deltaEcef.z;
    const double up =
        cosLat * cosLon * deltaEcef.x + cosLat * sinLon * deltaEcef.y + sinLat * deltaEcef.z;
    return Vector(east, north, up);
}

static LeoOrbitCalculator::GroundPoint
BuildTraceGroundPointFromEcef(const Vector& pointEcef)
{
    const Vector centerEcef = Wgs84GeodeticToEcef(g_gridCenterLatitudeDeg, g_gridCenterLongitudeDeg, 0.0);
    const Vector delta(pointEcef.x - centerEcef.x, pointEcef.y - centerEcef.y, pointEcef.z - centerEcef.z);
    const Vector enu = EcefDeltaToGridCenterEnu(delta);
    const auto latLon =
        OffsetMetersToLatLonDeg(g_gridCenterLatitudeDeg, g_gridCenterLongitudeDeg, enu.x, enu.y);
    return LeoOrbitCalculator::CreateGroundPoint(latLon.first, latLon.second, 0.0);
}

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
    if (!g_satAnchorTrace.is_open() && !g_satGroundTrackTrace.is_open())
    {
        return;
    }

    for (uint32_t satIdx = 0; satIdx < g_satellites.size(); ++satIdx)
    {
        const auto& sat = g_satellites[satIdx];
        uint32_t anchorGridId = sat.currentAnchorGridId;
        double anchorLatDeg = std::numeric_limits<double>::quiet_NaN();
        double anchorLonDeg = std::numeric_limits<double>::quiet_NaN();
        double anchorEastMeters = std::numeric_limits<double>::quiet_NaN();
        double anchorNorthMeters = std::numeric_limits<double>::quiet_NaN();

        Ptr<MobilityModel> satMobility = sat.node ? sat.node->GetObject<MobilityModel>() : nullptr;
        if (satMobility)
        {
            const Vector satEcef = satMobility->GetPosition();
            const Vector beamTargetEcef =
                ResolveSatelliteBeamTargetEcef(satIdx, satEcef);
            const Wgs84HexGridCell* beamGridCell = nullptr;
            if (const auto beamGridId = FindNearestGridCellId(beamTargetEcef))
            {
                anchorGridId = beamGridId.value();
                beamGridCell = FindHexGridCellById(g_hexGridCells, anchorGridId);
            }

            const Vector centerEcef =
                Wgs84GeodeticToEcef(g_gridCenterLatitudeDeg, g_gridCenterLongitudeDeg, 0.0);
            if (beamGridCell != nullptr && DistanceMeters(beamTargetEcef, beamGridCell->ecef) <= 1.0)
            {
                anchorLatDeg = beamGridCell->latitudeDeg;
                anchorLonDeg = beamGridCell->longitudeDeg;
                anchorEastMeters = beamGridCell->eastMeters;
                anchorNorthMeters = beamGridCell->northMeters;
            }
            else
            {
                const auto beamGroundPoint = BuildTraceGroundPointFromEcef(beamTargetEcef);
                anchorLatDeg = LeoOrbitCalculator::RadToDeg(beamGroundPoint.latitudeRad);
                anchorLonDeg = LeoOrbitCalculator::RadToDeg(beamGroundPoint.longitudeRad);

                const Vector delta(beamTargetEcef.x - centerEcef.x,
                                   beamTargetEcef.y - centerEcef.y,
                                   beamTargetEcef.z - centerEcef.z);
                const Vector enu = EcefDeltaToGridCenterEnu(delta);
                anchorEastMeters = enu.x;
                anchorNorthMeters = enu.y;
            }

            if (g_satGroundTrackTrace.is_open())
            {
                const auto subpointGround = BuildTraceGroundPointFromEcef(satEcef);
                const Vector satDelta(satEcef.x - centerEcef.x,
                                      satEcef.y - centerEcef.y,
                                      satEcef.z - centerEcef.z);
                const Vector satEnu = EcefDeltaToGridCenterEnu(satDelta);
                g_satGroundTrackTrace << std::fixed << std::setprecision(3) << nowSeconds << ","
                                      << satIdx << "," << sat.orbitPlaneIndex << ","
                                      << sat.orbitSlotIndex << "," << sat.dev->GetCellId() << ","
                                      << std::setprecision(8)
                                      << LeoOrbitCalculator::RadToDeg(subpointGround.latitudeRad) << ","
                                      << LeoOrbitCalculator::RadToDeg(subpointGround.longitudeRad) << ","
                                      << std::setprecision(3) << satEnu.x << "," << satEnu.y << ","
                                      << satEcef.x << "," << satEcef.y << "," << satEcef.z << "\n";
            }
        }

        if (g_satAnchorTrace.is_open())
        {
            g_satAnchorTrace << std::fixed << std::setprecision(3) << nowSeconds << "," << satIdx
                             << "," << sat.orbitPlaneIndex << "," << sat.orbitSlotIndex << ","
                             << sat.dev->GetCellId() << "," << anchorGridId << ",";
            g_satAnchorTrace << std::setprecision(8) << anchorLatDeg << "," << anchorLonDeg << ",";
            g_satAnchorTrace << std::setprecision(3) << anchorEastMeters << "," << anchorNorthMeters
                             << "\n";
        }
    }
}

/**
 * 判断两个六边形网格中心是否处于同一小区或彼此的一圈邻区内。
 */
static bool
IsSameOrNeighborHexCell(const Wgs84HexGridCell& a, const Wgs84HexGridCell& b)
{
    const double eastDelta = a.eastMeters - b.eastMeters;
    const double northDelta = a.northMeters - b.northMeters;
    const double neighborDistanceMeters = std::sqrt(3.0) * g_anchorGridHexRadiusKm * 1000.0;
    const double guardDistanceMeters = neighborDistanceMeters * 1.01;
    return eastDelta * eastDelta + northDelta * northDelta <= guardDistanceMeters * guardDistanceMeters;
}

static std::vector<uint32_t>
FindFirstRingNeighborGridIds(uint32_t centerGridId)
{
    std::vector<uint32_t> neighborGridIds;
    const auto* centerCell = FindHexGridCellById(g_hexGridCells, centerGridId);
    if (centerCell == nullptr)
    {
        return neighborGridIds;
    }

    for (const auto& cell : g_hexGridCells)
    {
        if (cell.id == centerGridId)
        {
            continue;
        }
        if (IsSameOrNeighborHexCell(cell, *centerCell))
        {
            neighborGridIds.push_back(cell.id);
        }
    }
    return neighborGridIds;
}

/**
 * 收集其它卫星波束已经占用的锚点及其一圈邻区。
 */
static std::set<uint32_t>
BuildBeamAnchorExclusionSet(uint32_t selfSatIndex)
{
    std::set<uint32_t> blockedGridIds;
    if (!g_enforceBeamExclusionRing || g_hexGridCells.empty())
    {
        return blockedGridIds;
    }

    for (uint32_t satIdx = 0; satIdx < g_satellites.size(); ++satIdx)
    {
        if (satIdx == selfSatIndex || g_satellites[satIdx].currentAnchorGridId == 0)
        {
            continue;
        }

        const auto* occupiedCell =
            FindHexGridCellById(g_hexGridCells, g_satellites[satIdx].currentAnchorGridId);
        if (occupiedCell == nullptr)
        {
            continue;
        }

        for (const auto& cell : g_hexGridCells)
        {
            if (IsSameOrNeighborHexCell(cell, *occupiedCell))
            {
                blockedGridIds.insert(cell.id);
            }
        }
    }
    return blockedGridIds;
}

static uint32_t
GetGridAnchorCandidateCount()
{
    if (!g_enforceBeamExclusionRing)
    {
        return std::max<uint32_t>(1, g_gridNearestK);
    }
    return std::max<uint32_t>({1, g_gridNearestK, g_beamExclusionCandidateK});
}

static int32_t ResolveSatelliteIndexFromCellId(uint16_t cellId);
static bool IsCrossLayerPhyWeak(const UeRuntime& ue);
static bool IsUeInsideAssignedBeam(uint32_t satIdx, const UeRuntime& ue);
static void ClearStableLeadTracking(UeRuntime& ue);

static std::optional<uint32_t>
FindNearestGridCellId(const Vector& pointEcef)
{
    if (g_hexGridCells.empty())
    {
        return std::nullopt;
    }

    const auto nearestIndices = FindNearestHexCellIndices(g_hexGridCells, pointEcef, 1);
    if (nearestIndices.empty())
    {
        return std::nullopt;
    }
    return g_hexGridCells[nearestIndices.front()].id;
}

static void
ClearDemandGridSnapshot()
{
    g_demandGridIds.clear();
    g_demandGridUeCounts.clear();
    g_demandGridPriorityWeights.clear();
}

static void
AccumulateDemandGridSnapshot(uint32_t gridId, uint32_t residentUeIncrement, uint32_t priorityWeightIncrement)
{
    if (residentUeIncrement == 0 && priorityWeightIncrement == 0)
    {
        return;
    }
    g_demandGridIds.insert(gridId);
    g_demandGridUeCounts[gridId] += residentUeIncrement;
    g_demandGridPriorityWeights[gridId] += priorityWeightIncrement;
}

static void
RefreshDemandGridSnapshotFromPlacements(const std::vector<UePlacement>& uePlacements)
{
    ClearDemandGridSnapshot();
    if (!g_useWgs84HexGrid || g_hexGridCells.empty())
    {
        return;
    }

    for (const auto& placement : uePlacements)
    {
        if (const auto gridId = FindNearestGridCellId(placement.groundPoint.ecef))
        {
            AccumulateDemandGridSnapshot(gridId.value(), 1, 1);
        }
    }
}

static void
RefreshDemandGridSnapshotFromRuntime()
{
    ClearDemandGridSnapshot();
    if (!g_useWgs84HexGrid || g_hexGridCells.empty())
    {
        return;
    }

    for (const auto& ue : g_ues)
    {
        const auto ueGridId = FindNearestGridCellId(ue.groundPoint.ecef);
        if (!ueGridId.has_value())
        {
            continue;
        }

        uint32_t priorityWeight = 1;
        uint16_t servingCellId = 0;
        if (ue.dev && ue.dev->GetRrc())
        {
            servingCellId = ue.dev->GetRrc()->GetCellId();
        }
        const int32_t servingSatIdx = ResolveSatelliteIndexFromCellId(servingCellId);
        if (g_demandSnapshotMode == DemandSnapshotMode::RUNTIME_UNDERSERVED_UE)
        {
            if (servingSatIdx < 0 || static_cast<uint32_t>(servingSatIdx) >= g_satellites.size())
            {
                priorityWeight += 2;
            }
            else
            {
                const auto& servingSat = g_satellites[static_cast<uint32_t>(servingSatIdx)];
                if (servingSat.currentAnchorGridId != 0 && servingSat.currentAnchorGridId != ueGridId.value())
                {
                    priorityWeight += 1;
                }
                if (!IsUeInsideAssignedBeam(static_cast<uint32_t>(servingSatIdx), ue))
                {
                    priorityWeight += 1;
                }
                if (!servingSat.admissionAllowed)
                {
                    priorityWeight += 1;
                }
            }
            if (IsCrossLayerPhyWeak(ue))
            {
                priorityWeight += 1;
            }
        }

        AccumulateDemandGridSnapshot(ueGridId.value(), 1, priorityWeight);
    }
}

static uint32_t
GetDemandGridPriorityWeight(uint32_t gridId)
{
    const auto it = g_demandGridPriorityWeights.find(gridId);
    return (it != g_demandGridPriorityWeights.end()) ? it->second : 0;
}

static uint32_t
GetDemandGridUeCount(uint32_t gridId)
{
    const auto it = g_demandGridUeCounts.find(gridId);
    return (it != g_demandGridUeCounts.end()) ? it->second : 0;
}

static bool
IsUeInSatelliteAnchorCell(uint32_t satIdx, const UeRuntime& ue)
{
    if (!g_enforceAnchorCellForRealLinks ||
        !RequiresDiscreteAnchorCellGate(g_earthFixedBeamTargetMode))
    {
        return true;
    }
    if (satIdx >= g_satellites.size() || g_satellites[satIdx].currentAnchorGridId == 0)
    {
        return false;
    }

    const auto ueGridId = FindNearestGridCellId(ue.groundPoint.ecef);
    return ueGridId.has_value() && ueGridId.value() == g_satellites[satIdx].currentAnchorGridId;
}

static GridAnchorSelection
ComputeDemandAwareGridAnchorSelection(const Vector& satEcef,
                                      uint32_t candidateCount,
                                      const std::set<uint32_t>& blockedGridIds)
{
    auto fallback = ComputeGridAnchorSelection(g_hexGridCells, satEcef, candidateCount, blockedGridIds);
    if (!g_preferDemandAnchorCells || g_demandGridIds.empty())
    {
        return fallback;
    }

    const Vector nadirPoint = ProjectNadirToWgs84(satEcef);
    const auto computeDemandCandidateScore =
        [&](const Wgs84HexGridCell& cell) -> std::optional<double> {
        const auto anchorBudget =
            CalculateEarthFixedBeamBudget(satEcef, cell.ecef, cell.ecef, g_beamModelConfig);
        if (!anchorBudget.valid || !anchorBudget.beamLocked)
        {
            return std::nullopt;
        }

        const double distanceMeters = DistanceMeters(nadirPoint, cell.ecef);
        return distanceMeters + anchorBudget.scanLossDb * 1000.0;
    };

    const auto makeLandingCandidate = [&](uint32_t gridId) -> GridAnchorLandingCandidate {
        GridAnchorLandingCandidate candidate;
        candidate.gridId = gridId;

        const auto* cell = FindHexGridCellById(g_hexGridCells, gridId);
        if (cell == nullptr)
        {
            return candidate;
        }

        candidate.ueCount = GetDemandGridUeCount(gridId);
        candidate.priorityWeight = GetDemandGridPriorityWeight(gridId);
        const auto score = computeDemandCandidateScore(*cell);
        candidate.legal = blockedGridIds.find(gridId) == blockedGridIds.end() && score.has_value();
        if (score.has_value())
        {
            candidate.score = score.value();
        }
        return candidate;
    };

    if (!fallback.nearestGridIds.empty())
    {
        const uint32_t primaryGridId = fallback.nearestGridIds.front();
        std::vector<GridAnchorLandingCandidate> firstRingCandidates;
        for (const auto neighborGridId : FindFirstRingNeighborGridIds(primaryGridId))
        {
            firstRingCandidates.push_back(makeLandingCandidate(neighborGridId));
        }

        const auto landingDecision =
            SelectPreferredGridAnchorLanding(makeLandingCandidate(primaryGridId), firstRingCandidates);
        if (landingDecision.found)
        {
            if (const auto* chosenCell = FindHexGridCellById(g_hexGridCells, landingDecision.anchorGridId))
            {
                fallback.found = true;
                fallback.anchorGridId = chosenCell->id;
                fallback.anchorEcef = chosenCell->ecef;
                return fallback;
            }
        }
    }

    if (g_anchorSelectionMode == AnchorSelectionMode::DEMAND_MAX_UE_NEAR_NADIR)
    {
        uint32_t bestPriorityWeight = 0;
        uint32_t bestUeCount = 0;
        double bestScore = std::numeric_limits<double>::infinity();
        const Wgs84HexGridCell* bestCell = nullptr;

        for (const auto gridId : fallback.nearestGridIds)
        {
            const auto* cell = FindHexGridCellById(g_hexGridCells, gridId);
            if (cell == nullptr || g_demandGridIds.find(cell->id) == g_demandGridIds.end() ||
                blockedGridIds.find(cell->id) != blockedGridIds.end())
            {
                continue;
            }

            const uint32_t priorityWeight = GetDemandGridPriorityWeight(cell->id);
            const uint32_t ueCount = GetDemandGridUeCount(cell->id);
            if (ueCount == 0)
            {
                continue;
            }

            const auto score = computeDemandCandidateScore(*cell);
            if (!score.has_value())
            {
                continue;
            }

            if (bestCell == nullptr || priorityWeight > bestPriorityWeight ||
                (priorityWeight == bestPriorityWeight && ueCount > bestUeCount) ||
                (priorityWeight == bestPriorityWeight && ueCount == bestUeCount &&
                 score.value() < bestScore))
            {
                bestCell = cell;
                bestPriorityWeight = priorityWeight;
                bestUeCount = ueCount;
                bestScore = score.value();
            }
        }

        if (bestCell != nullptr)
        {
            fallback.found = true;
            fallback.anchorGridId = bestCell->id;
            fallback.anchorEcef = bestCell->ecef;
            return fallback;
        }
    }

    double bestScore = std::numeric_limits<double>::infinity();
    const Wgs84HexGridCell* bestCell = nullptr;

    for (const auto& cell : g_hexGridCells)
    {
        if (g_demandGridIds.find(cell.id) == g_demandGridIds.end() ||
            blockedGridIds.find(cell.id) != blockedGridIds.end())
        {
            continue;
        }

        const auto score = computeDemandCandidateScore(cell);
        if (!score.has_value())
        {
            continue;
        }

        if (score.value() < bestScore)
        {
            bestScore = score.value();
            bestCell = &cell;
        }
    }

    if (bestCell == nullptr)
    {
        return fallback;
    }

    fallback.found = true;
    fallback.anchorGridId = bestCell->id;
    fallback.anchorEcef = bestCell->ecef;
    return fallback;
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
    const auto blockedGridIds = BuildBeamAnchorExclusionSet(satIndex);
    const auto anchor = ComputeDemandAwareGridAnchorSelection(satEcef,
                                                              GetGridAnchorCandidateCount(),
                                                              blockedGridIds);
    if (!anchor.found)
    {
        return;
    }

    sat.nearestGridIds = anchor.nearestGridIds;
    const auto logAnchor = [&](uint32_t anchorGridId) {
        std::cout << "[GRID-ANCHOR] t=" << std::fixed << std::setprecision(3) << nowSeconds << "s"
                  << " sat" << satIndex
                  << " cell=" << sat.dev->GetCellId()
                  << " anchorGrid=" << anchorGridId
                  << " nearestK=" << FormatUintList(sat.nearestGridIds) << std::endl;
    };
    const auto clearPendingAnchor = [&]() {
        sat.pendingAnchorGridId = 0;
        sat.pendingAnchorEcef = Vector(0.0, 0.0, 0.0);
        sat.pendingAnchorLeadStartTimeSeconds = -1.0;
    };
    const auto commitAnchor = [&](uint32_t anchorGridId, const Vector& anchorEcef) {
        sat.currentAnchorGridId = anchorGridId;
        sat.cellAnchorEcef = anchorEcef;
        clearPendingAnchor();
    };

    if (sat.currentAnchorGridId == 0)
    {
        commitAnchor(anchor.anchorGridId, anchor.anchorEcef);
        if (forceLog || g_printGridAnchorEvents)
        {
            logAnchor(sat.currentAnchorGridId);
        }
        return;
    }

    if (anchor.anchorGridId == sat.currentAnchorGridId)
    {
        sat.cellAnchorEcef = anchor.anchorEcef;
        clearPendingAnchor();
        if (forceLog)
        {
            logAnchor(sat.currentAnchorGridId);
        }
        return;
    }

    const bool currentAnchorBlocked =
        (blockedGridIds.find(sat.currentAnchorGridId) != blockedGridIds.end());
    const Vector nadirPoint = ProjectNadirToWgs84(satEcef);
    const double candidateDistanceMeters = DistanceMeters(nadirPoint, anchor.anchorEcef);
    double currentDistanceMeters = std::numeric_limits<double>::infinity();
    if (const auto* currentCell = FindHexGridCellById(g_hexGridCells, sat.currentAnchorGridId))
    {
        currentDistanceMeters = DistanceMeters(nadirPoint, currentCell->ecef);
    }
    else
    {
        commitAnchor(anchor.anchorGridId, anchor.anchorEcef);
        if (forceLog || g_printGridAnchorEvents)
        {
            logAnchor(sat.currentAnchorGridId);
        }
        return;
    }

    if (currentAnchorBlocked)
    {
        commitAnchor(anchor.anchorGridId, anchor.anchorEcef);
        if (forceLog || g_printGridAnchorEvents)
        {
            logAnchor(sat.currentAnchorGridId);
        }
        return;
    }

    const bool candidateHasGuardAdvantage =
        (currentDistanceMeters - candidateDistanceMeters >= g_anchorGridSwitchGuardMeters - 1e-9);
    if (!candidateHasGuardAdvantage)
    {
        clearPendingAnchor();
        if (forceLog)
        {
            logAnchor(sat.currentAnchorGridId);
        }
        return;
    }

    if (g_anchorGridHysteresisSeconds <= 0.0)
    {
        commitAnchor(anchor.anchorGridId, anchor.anchorEcef);
        if (forceLog || g_printGridAnchorEvents)
        {
            logAnchor(sat.currentAnchorGridId);
        }
        return;
    }

    if (sat.pendingAnchorGridId != anchor.anchorGridId)
    {
        sat.pendingAnchorGridId = anchor.anchorGridId;
        sat.pendingAnchorEcef = anchor.anchorEcef;
        sat.pendingAnchorLeadStartTimeSeconds = nowSeconds;
        if (forceLog)
        {
            logAnchor(sat.currentAnchorGridId);
        }
        return;
    }

    sat.pendingAnchorEcef = anchor.anchorEcef;
    if (nowSeconds - sat.pendingAnchorLeadStartTimeSeconds + 1e-9 >= g_anchorGridHysteresisSeconds)
    {
        commitAnchor(anchor.anchorGridId, anchor.anchorEcef);
        if (forceLog || g_printGridAnchorEvents)
        {
            logAnchor(sat.currentAnchorGridId);
        }
    }
    else if (forceLog)
    {
        logAnchor(sat.currentAnchorGridId);
    }
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

static AnchorSelectionMode
ParseAnchorSelectionMode(const std::string& anchorSelectionMode)
{
    if (anchorSelectionMode == "demand-max-ue-near-nadir")
    {
        return AnchorSelectionMode::DEMAND_MAX_UE_NEAR_NADIR;
    }
    return AnchorSelectionMode::DEMAND_NEAREST;
}

static const char*
ToString(AnchorSelectionMode anchorSelectionMode)
{
    return anchorSelectionMode == AnchorSelectionMode::DEMAND_MAX_UE_NEAR_NADIR
               ? "demand-max-ue-near-nadir"
               : "demand-nearest";
}

static DemandSnapshotMode
ParseDemandSnapshotMode(const std::string& demandSnapshotMode)
{
    if (demandSnapshotMode == "static-layout")
    {
        return DemandSnapshotMode::STATIC_UE_LAYOUT;
    }
    return DemandSnapshotMode::RUNTIME_UNDERSERVED_UE;
}

static const char*
ToString(DemandSnapshotMode demandSnapshotMode)
{
    return demandSnapshotMode == DemandSnapshotMode::STATIC_UE_LAYOUT ? "static-layout"
                                                                     : "runtime-underserved-ue";
}

static std::optional<Vector>
ResolveEarthFixedGnbAnchorPosition(const Ptr<const NetDevice>& gnbDevice)
{
    const auto gnb = DynamicCast<const NrGnbNetDevice>(gnbDevice);
    if (!gnb)
    {
        return std::nullopt;
    }

    const auto satIt = g_cellToSatellite.find(gnb->GetCellId());
    if (satIt == g_cellToSatellite.end() || satIt->second >= g_satellites.size())
    {
        return std::nullopt;
    }

    const auto& sat = g_satellites[satIt->second];
    Ptr<MobilityModel> satMobility = sat.node ? sat.node->GetObject<MobilityModel>() : nullptr;
    if (!satMobility)
    {
        return std::nullopt;
    }
    return ResolveSatelliteBeamTargetEcef(satIt->second, satMobility->GetPosition());
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

static void
ReportDlPhyTbTrace(std::string, RxPacketTraceParams params)
{
    const auto ueIdx = ResolveUeIndexFromServingCellAndRnti(params.m_cellId, params.m_rnti);
    if (!ueIdx || *ueIdx >= g_ues.size())
    {
        return;
    }

    auto& ue = g_ues[*ueIdx];
    ue.phyDlTbCount++;
    ue.phyDlTblerSum += params.m_tbler;
    if (params.m_corrupt)
    {
        ue.phyDlCorruptTbCount++;
    }

    const uint64_t intervalIndex = static_cast<uint64_t>(
        std::floor((Simulator::Now().GetSeconds() + 1e-9) / g_phyDlTbIntervalSeconds));
    auto& interval = g_phyDlTbIntervals[intervalIndex];
    interval.tbCount++;
    interval.tblerSum += params.m_tbler;
    if (params.m_corrupt)
    {
        interval.corruptTbCount++;
    }

    if (g_enablePhyDlTbTrace && g_phyDlTbTrace.is_open())
    {
        const int32_t servingSatIdx =
            ResolveSatelliteIndexFromCellId(static_cast<uint16_t>(params.m_cellId));
        g_phyDlTbTrace << std::fixed << std::setprecision(9)
                       << Simulator::Now().GetSeconds() << "," << *ueIdx << ","
                       << params.m_cellId << "," << servingSatIdx << "," << params.m_rnti
                       << "," << params.m_bwpId << "," << params.m_frameNum << ","
                       << static_cast<uint32_t>(params.m_subframeNum) << ","
                       << params.m_slotNum << "," << static_cast<uint32_t>(params.m_symStart)
                       << "," << static_cast<uint32_t>(params.m_numSym) << ","
                       << params.m_tbSize << "," << static_cast<uint32_t>(params.m_mcs)
                       << "," << static_cast<uint32_t>(params.m_rank) << ","
                       << static_cast<uint32_t>(params.m_rv) << "," << params.m_rbAssignedNum
                       << "," << static_cast<uint32_t>(params.m_cqi) << ",";
        if (params.m_sinr > 0.0)
        {
            g_phyDlTbTrace << 10.0 * std::log10(params.m_sinr);
        }
        g_phyDlTbTrace << ",";
        if (params.m_sinrMin > 0.0)
        {
            g_phyDlTbTrace << 10.0 * std::log10(params.m_sinrMin);
        }
        g_phyDlTbTrace << "," << params.m_tbler << "," << (params.m_corrupt ? 1 : 0)
                       << "\n";
    }

    const double alpha =
        std::clamp(g_measurementDrivenConfig.improvedCrossLayerPhyAlpha, 1e-6, 1.0);
    const double corruptIndicator = params.m_corrupt ? 1.0 : 0.0;
    if (ue.recentPhySampleCount == 0)
    {
        ue.recentPhyCorruptRateEwma = corruptIndicator;
        ue.recentPhyTblerEwma = params.m_tbler;
    }
    else
    {
        ue.recentPhyCorruptRateEwma =
            (1.0 - alpha) * ue.recentPhyCorruptRateEwma + alpha * corruptIndicator;
        ue.recentPhyTblerEwma = (1.0 - alpha) * ue.recentPhyTblerEwma + alpha * params.m_tbler;
    }
    ue.recentPhySampleCount++;

    if (params.m_sinr > 0.0)
    {
        const double sinrDb = 10.0 * std::log10(params.m_sinr);
        ue.phyDlSinrDbSum += sinrDb;
        ue.phyDlMinSinrDb = std::min(ue.phyDlMinSinrDb, sinrDb);
        interval.sinrDbSum += sinrDb;
        interval.minSinrDb = std::min(interval.minSinrDb, sinrDb);
        if (!std::isfinite(ue.recentPhySinrDbEwma) || ue.recentPhySampleCount <= 1)
        {
            ue.recentPhySinrDbEwma = sinrDb;
        }
        else
        {
            ue.recentPhySinrDbEwma =
                (1.0 - alpha) * ue.recentPhySinrDbEwma + alpha * sinrDb;
        }
    }
}

static bool
IsCrossLayerPhyWeak(const UeRuntime& ue)
{
    if (!g_measurementDrivenConfig.improvedEnableCrossLayerPhyAssist ||
        ue.recentPhySampleCount < g_measurementDrivenConfig.improvedCrossLayerMinSamples)
    {
        return false;
    }

    const bool tblerWeak =
        ue.recentPhyTblerEwma >= g_measurementDrivenConfig.improvedCrossLayerTblerThreshold;
    const bool sinrWeak = std::isfinite(ue.recentPhySinrDbEwma) &&
                          ue.recentPhySinrDbEwma <=
                              g_measurementDrivenConfig.improvedCrossLayerSinrThresholdDb;
    return tblerWeak || sinrWeak;
}

static bool
IsUeInsideAssignedBeam(uint32_t satIdx, const UeRuntime& ue)
{
    if (!g_enforceBeamCoverageForRealLinks)
    {
        return true;
    }
    if (satIdx >= g_satellites.size() || !g_satellites[satIdx].node)
    {
        return false;
    }

    Ptr<MobilityModel> satMobility = g_satellites[satIdx].node->GetObject<MobilityModel>();
    if (!satMobility)
    {
        return false;
    }

    const auto budget = CalculateEarthFixedBeamBudget(
        satMobility->GetPosition(),
        ue.groundPoint.ecef,
        ResolveSatelliteBeamTargetEcef(satIdx, satMobility->GetPosition()),
        g_beamModelConfig);
    return budget.valid && budget.beamLocked &&
           budget.offBoresightAngleRad <= g_beamModelConfig.theta3dBRad &&
           std::isfinite(budget.rsrpDbm) && IsUeInSatelliteAnchorCell(satIdx, ue);
}

static void
ClearStableLeadTracking(UeRuntime& ue)
{
    ue.stableLeadSourceCell = 0;
    ue.stableLeadTargetCell = 0;
    ue.stableLeadSinceSeconds = -1.0;
}

static MeasurementDrivenDecisionContext
BuildMeasurementDrivenDecisionContext()
{
    MeasurementDrivenDecisionContext context{g_satellites, g_cellToSatellite};
    context.gmstAtEpochRad = g_gmstAtEpochRad;
    context.carrierFrequencyHz = g_carrierFrequencyHz;
    context.minElevationRad = g_minElevationRad;
    context.loadCongestionThreshold = g_loadCongestionThreshold;
    context.handoverMode = g_measurementDrivenConfig.handoverMode;
    context.improvedSignalWeight = g_measurementDrivenConfig.improvedSignalWeight;
    context.improvedRsrqWeight = g_measurementDrivenConfig.improvedRsrqWeight;
    context.improvedLoadWeight = g_measurementDrivenConfig.improvedLoadWeight;
    context.improvedVisibilityWeight = g_measurementDrivenConfig.improvedVisibilityWeight;
    context.improvedMinLoadScoreDelta = g_measurementDrivenConfig.improvedMinLoadScoreDelta;
    context.improvedMaxSignalGapDb = g_measurementDrivenConfig.improvedMaxSignalGapDb;
    context.improvedMinVisibilitySeconds =
        g_measurementDrivenConfig.improvedMinVisibilitySeconds;
    context.improvedVisibilityHorizonSeconds =
        g_measurementDrivenConfig.improvedVisibilityHorizonSeconds;
    context.improvedVisibilityPredictionStepSeconds =
        g_measurementDrivenConfig.improvedVisibilityPredictionStepSeconds;
    context.improvedMinJointScoreMargin =
        g_measurementDrivenConfig.improvedMinJointScoreMargin;
    context.improvedMinCandidateRsrpDbm =
        g_measurementDrivenConfig.improvedMinCandidateRsrpDbm;
    context.improvedMinCandidateRsrqDb =
        g_measurementDrivenConfig.improvedMinCandidateRsrqDb;
    context.improvedServingWeakRsrpDbm =
        g_measurementDrivenConfig.improvedServingWeakRsrpDbm;
    context.improvedServingWeakRsrqDb =
        g_measurementDrivenConfig.improvedServingWeakRsrqDb;
    context.improvedMinRsrqAdvantageDb =
        g_measurementDrivenConfig.improvedMinRsrqAdvantageDb;
    context.isCandidateAllowed = &IsUeInsideAssignedBeam;
    context.isCrossLayerPhyWeak = &IsCrossLayerPhyWeak;
    return context;
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

    const auto decisionContext = BuildMeasurementDrivenDecisionContext();
    const bool servingWeak =
        (g_measurementDrivenConfig.handoverMode == HandoverMode::IMPROVED) &&
        (IsServingLinkWeak(decisionContext, measResults) ||
         IsCrossLayerPhyWeak(decisionContext, ue));

    const auto selectedTarget = SelectMeasurementDrivenTarget(decisionContext,
                                                              sourceCellId,
                                                              static_cast<uint32_t>(sourceSatIdx),
                                                              ue,
                                                              measResults);
    if (!selectedTarget.has_value() || selectedTarget->cellId == 0)
    {
        if (g_measurementDrivenConfig.handoverMode == HandoverMode::IMPROVED)
        {
            ClearStableLeadTracking(ue);
        }
        return;
    }

    if (g_measurementDrivenConfig.handoverMode == HandoverMode::IMPROVED && !servingWeak)
    {
        const double nowSeconds = Simulator::Now().GetSeconds();
        const bool sameStableLead = ue.stableLeadSourceCell == sourceCellId &&
                                    ue.stableLeadTargetCell == selectedTarget->cellId;
        if (!sameStableLead)
        {
            ue.stableLeadSourceCell = sourceCellId;
            ue.stableLeadTargetCell = selectedTarget->cellId;
            ue.stableLeadSinceSeconds = nowSeconds;
            return;
        }

        if (g_measurementDrivenConfig.improvedMinStableLeadTimeSeconds > 0.0 &&
            (nowSeconds - ue.stableLeadSinceSeconds + 1e-9 <
             g_measurementDrivenConfig.improvedMinStableLeadTimeSeconds))
        {
            return;
        }
    }

    ue.hasPendingHoStart = true;
    ue.lastHoStartSourceCell = sourceCellId;
    ue.lastHoStartTargetCell = selectedTarget->cellId;
    ClearStableLeadTracking(ue);
    g_satellites[sourceSatIdx].rrc->GetNrHandoverManagementSapUser()->TriggerHandover(
        rnti,
        selectedTarget->cellId);

    if (!g_compactReport || g_printNrtEvents)
    {
        std::cout << "[HO-MEAS-TRIGGER] t=" << std::fixed << std::setprecision(3)
                  << Simulator::Now().GetSeconds() << "s"
                  << " mode=" << ToString(g_measurementDrivenConfig.handoverMode)
                  << " ue=" << *ueIdx
                  << " sourceCell=" << sourceCellId
                  << " targetCell=" << selectedTarget->cellId
                  << " targetRsrp=" << selectedTarget->rsrpDbm << "dBm";
        if (g_measurementDrivenConfig.handoverMode == HandoverMode::IMPROVED)
        {
            const double sourceLoadScore = g_satellites[sourceSatIdx].loadScore;
            std::cout << " jointScore=" << selectedTarget->jointScore
                      << " loadScore=" << selectedTarget->loadScore
                      << " sourceLoad=" << sourceLoadScore
                      << " sourceLoadPressure="
                      << ComputeLoadPressureFromScore(sourceLoadScore,
                                                     decisionContext.loadCongestionThreshold)
                      << " remainVis=" << selectedTarget->remainingVisibilitySeconds << "s"
                      << " visScore=" << selectedTarget->visibilityScore
                      << " admission=" << (selectedTarget->admissionAllowed ? "YES" : "NO");
        }
        std::cout << std::endl;
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

static void
WriteHandoverEventTraceRow(double nowSeconds,
                           uint32_t ueIdx,
                           uint32_t handoverId,
                           const std::string& eventName,
                           uint16_t sourceCellId,
                           uint16_t targetCellId,
                           double delayMs,
                           bool pingPongDetected,
                           HandoverFailureReason failureReason = HandoverFailureReason::NONE)
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
    g_handoverEventTrace << "," << (pingPongDetected ? 1 : 0) << ","
                         << ToString(failureReason) << "\n";
    g_handoverEventTrace.flush();
}

static void
RegisterHandoverFailure(UeRuntime& ue, HandoverFailureReason reason)
{
    switch (reason)
    {
    case HandoverFailureReason::NO_PREAMBLE:
        ue.handoverFailureNoPreambleCount++;
        break;
    case HandoverFailureReason::MAX_RACH:
        ue.handoverFailureMaxRachCount++;
        break;
    case HandoverFailureReason::LEAVING_TIMEOUT:
        ue.handoverFailureLeavingCount++;
        break;
    case HandoverFailureReason::JOINING_TIMEOUT:
        ue.handoverFailureJoiningCount++;
        break;
    case HandoverFailureReason::UNKNOWN:
    case HandoverFailureReason::NONE:
        ue.handoverFailureUnknownCount++;
        break;
    }
}

static void
MarkPendingHandoverFailureReason(uint64_t imsi, HandoverFailureReason reason)
{
    const auto ueIdx = ResolveUeIndexFromImsi(g_imsiToUe, imsi);
    if (!ueIdx.has_value())
    {
        return;
    }

    auto& ue = g_ues[*ueIdx];
    if (!ue.hasPendingHoStart)
    {
        return;
    }

    ue.pendingFailureReason = reason;
}

static void
ReportHandoverFailureNoPreamble(std::string, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
    MarkPendingHandoverFailureReason(imsi, HandoverFailureReason::NO_PREAMBLE);
}

static void
ReportHandoverFailureMaxRach(std::string, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
    MarkPendingHandoverFailureReason(imsi, HandoverFailureReason::MAX_RACH);
}

static void
ReportHandoverFailureLeaving(std::string, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
    MarkPendingHandoverFailureReason(imsi, HandoverFailureReason::LEAVING_TIMEOUT);
}

static void
ReportHandoverFailureJoining(std::string, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
    MarkPendingHandoverFailureReason(imsi, HandoverFailureReason::JOINING_TIMEOUT);
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
        ue.lastHoStartSourceCell = cellId;
        ue.lastHoStartTargetCell = targetCellId;
        ue.lastHoStartTimeSeconds = Simulator::Now().GetSeconds();
        ue.handoverStartCount++;
        ue.hoStartPhyTblerEwma = ue.recentPhyTblerEwma;
        ue.hoStartPhySinrDbEwma = ue.recentPhySinrDbEwma;
        ue.hoStartPhyCorruptRateEwma = ue.recentPhyCorruptRateEwma;
        WriteHandoverEventTraceRow(ue.lastHoStartTimeSeconds,
                                   *ueIdx,
                                   ue.activeHandoverTraceId,
                                   "HO_START",
                                   cellId,
                                   targetCellId,
                                   std::numeric_limits<double>::quiet_NaN(),
                                   false);
        ue.pendingFailureReason = HandoverFailureReason::NONE;
        prefix << " ue=" << *ueIdx;
    }

    if (!g_compactReport || g_printNrtEvents)
    {
        std::cout << "[HO-START] t=" << std::fixed << std::setprecision(3)
                  << Simulator::Now().GetSeconds() << "s"
                  << prefix.str()
                  << " sourceCell=" << cellId
                  << " targetCell=" << targetCellId << std::endl;
    }
}

/**
 * gNB 侧切换成功回调。
 *
 * 主要工作：
 * - 统计成功次数；
 * - 计算切换执行时延；
 * - 判断是否发生短时回切。
 */
static void
ReportHandoverEndOk(std::string, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
    std::ostringstream prefix;
    double handoverDelayMs = -1.0;
    bool pingPongDetected = false;
    bool interferenceTrapDetected = false;
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
            ClearStableLeadTracking(ue);
        }
        ue.hoEndOkPhyTblerEwma = ue.recentPhyTblerEwma;
        ue.hoEndOkPhySinrDbEwma = ue.recentPhySinrDbEwma;
        ue.lastHoEndOkTimeSeconds = nowSeconds;
        const bool sinrTrap = std::isfinite(ue.hoStartPhySinrDbEwma) &&
                              std::isfinite(ue.hoEndOkPhySinrDbEwma) &&
                              ue.hoStartPhySinrDbEwma > 5.0 &&
                              ue.hoEndOkPhySinrDbEwma < 0.0;
        const bool tblerTrap = std::isfinite(ue.hoStartPhyTblerEwma) &&
                               std::isfinite(ue.hoEndOkPhyTblerEwma) &&
                               ue.hoStartPhyTblerEwma < 0.3 &&
                               ue.hoEndOkPhyTblerEwma > 0.4;
        if (sinrTrap || tblerTrap)
        {
            ue.interferenceTrapHoCount++;
            interferenceTrapDetected = true;
        }
        WriteHandoverEventTraceRow(nowSeconds,
                                   *ueIdx,
                                   activeHandoverTraceId,
                                   "HO_END_OK",
                                   ue.lastHoStartSourceCell,
                                   cellId,
                                   handoverDelayMs,
                                   pingPongDetected);
        ue.hasPendingHoStart = false;
        ue.lastHoStartTimeSeconds = -1.0;
        ue.activeHandoverTraceId = 0;
        ue.pendingFailureReason = HandoverFailureReason::NONE;
        ClearStableLeadTracking(ue);
        prefix << " ue=" << *ueIdx;
    }

    if (!g_compactReport || g_printNrtEvents)
    {
        std::cout << "[HO-END-OK] t=" << std::fixed << std::setprecision(3)
                  << Simulator::Now().GetSeconds() << "s"
                  << prefix.str()
                  << " targetCell=" << cellId;
        if (handoverDelayMs >= 0.0)
        {
            std::cout << " delay=" << std::fixed << std::setprecision(3) << handoverDelayMs
                      << "ms";
        }
        std::cout << std::endl;
        if (interferenceTrapDetected)
        {
            std::cout << "[INTERFERENCE-TRAP] t=" << std::fixed << std::setprecision(3)
                      << Simulator::Now().GetSeconds() << "s"
                      << prefix.str()
                      << " sourceCell=" << currentSourceCell
                      << " targetCell=" << cellId << std::endl;
        }
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
                      << " gap=" << std::fixed << std::setprecision(3) << pingPongGapSeconds
                      << "s" << std::endl;
        }
    }
}

static void
ReportHandoverEndError(std::string, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
    std::ostringstream prefix;
    HandoverFailureReason failureReason = HandoverFailureReason::UNKNOWN;
    uint16_t sourceCellId = 0;
    uint16_t targetCellId = 0;
    uint32_t activeHandoverTraceId = 0;

    if (const auto ueIdx = ResolveUeIndexFromImsi(g_imsiToUe, imsi))
    {
        auto& ue = g_ues[*ueIdx];
        ue.handoverEndErrorCount++;
        activeHandoverTraceId = ue.activeHandoverTraceId;
        sourceCellId = ue.lastHoStartSourceCell;
        targetCellId = ue.lastHoStartTargetCell;
        if (ue.pendingFailureReason != HandoverFailureReason::NONE)
        {
            failureReason = ue.pendingFailureReason;
        }
        RegisterHandoverFailure(ue, failureReason);
        WriteHandoverEventTraceRow(Simulator::Now().GetSeconds(),
                                   *ueIdx,
                                   activeHandoverTraceId,
                                   "HO_END_ERROR",
                                   sourceCellId,
                                   targetCellId,
                                   std::numeric_limits<double>::quiet_NaN(),
                                   false,
                                   failureReason);
        ue.hasPendingHoStart = false;
        ue.lastHoStartTimeSeconds = -1.0;
        ue.activeHandoverTraceId = 0;
        ue.pendingFailureReason = HandoverFailureReason::NONE;
        prefix << " ue=" << *ueIdx;
    }

    if (!g_compactReport || g_printNrtEvents)
    {
        std::cout << "[HO-END-ERROR] t=" << std::fixed << std::setprecision(3)
                  << Simulator::Now().GetSeconds() << "s"
                  << prefix.str()
                  << " sourceCell=" << sourceCellId
                  << " targetCell=" << targetCellId
                  << " failureReason=" << ToString(failureReason) << std::endl;
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
    std::vector<uint16_t> servingCellIds(g_ues.size(), 0);
    std::vector<int32_t> servingSatIndices(g_ues.size(), -1);

    std::vector<LeoOrbitCalculator::OrbitState> referenceStates(g_satellites.size());
    for (uint32_t i = 0; i < g_satellites.size(); ++i)
    {
        referenceStates[i] = LeoOrbitCalculator::CalculateSatelliteState(
            nowSeconds, g_satellites[i].orbit, g_gmstAtEpochRad);
        // 卫星节点位置直接用几何计算结果覆盖，形成时变轨道运动。
        g_satellites[i].node->GetObject<MobilityModel>()->SetPosition(referenceStates[i].ecef);
    }

    for (uint32_t ueIdx = 0; ueIdx < g_ues.size(); ++ueIdx)
    {
        const auto& ue = g_ues[ueIdx];
        uint16_t servingCellId = 0;
        if (ue.dev && ue.dev->GetRrc())
        {
            servingCellId = ue.dev->GetRrc()->GetCellId();
        }
        servingCellIds[ueIdx] = servingCellId;

        const int32_t servingSatIdx = ResolveSatelliteIndexFromCellId(servingCellId);
        servingSatIndices[ueIdx] = servingSatIdx;
        if (servingSatIdx >= 0 && static_cast<uint32_t>(servingSatIdx) < g_satellites.size())
        {
            nextAttachedUeCounts[servingSatIdx]++;
        }
    }

    for (uint32_t satIdx = 0; satIdx < g_satellites.size(); ++satIdx)
    {
        g_satellites[satIdx].attachedUeCount = nextAttachedUeCounts[satIdx];
    }
    UpdateSatelliteLoadStats(
        g_satellites, g_offeredPacketRatePerUe, g_maxSupportedUesPerSatellite, g_loadCongestionThreshold);

    if (g_demandSnapshotMode == DemandSnapshotMode::RUNTIME_UNDERSERVED_UE)
    {
        RefreshDemandGridSnapshotFromRuntime();
    }

    for (uint32_t i = 0; i < g_satellites.size(); ++i)
    {
        UpdateSatelliteAnchorFromGrid(i, referenceStates[i].ecef, nowSeconds, false);
    }
    FlushSatelliteAnchorTraceRows(nowSeconds);

    for (uint32_t ueIdx = 0; ueIdx < g_ues.size(); ++ueIdx)
    {
        auto& ue = g_ues[ueIdx];
        const uint16_t servingCellId = servingCellIds[ueIdx];
        const int32_t servingSatIdx = servingSatIndices[ueIdx];

        const bool servingChanged =
            (ue.lastServingCellForLog != 0 && servingCellId != 0 &&
             servingCellId != ue.lastServingCellForLog);
        if (servingChanged)
        {
            if (!g_compactReport || g_printNrtEvents)
            {
                std::cout << "[SERVING] t=" << std::fixed << std::setprecision(3) << nowSeconds
                          << "s ue=" << ueIdx << " cell " << ue.lastServingCellForLog << " -> "
                          << servingCellId << std::endl;
            }
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
    g_anchorGridHexRadiusKm = config.anchorGridHexRadiusKm;
    g_gridNearestK = config.gridNearestK;
    g_enforceBeamExclusionRing = config.enforceBeamExclusionRing;
    g_beamExclusionCandidateK = config.beamExclusionCandidateK;
    g_enforceBeamCoverageForRealLinks = config.enforceBeamCoverageForRealLinks;
    g_enforceAnchorCellForRealLinks = config.enforceAnchorCellForRealLinks;
    g_preferDemandAnchorCells = config.preferDemandAnchorCells;
    g_anchorSelectionMode = ParseAnchorSelectionMode(config.anchorSelectionMode);
    g_demandSnapshotMode = ParseDemandSnapshotMode(config.demandSnapshotMode);
    g_earthFixedBeamTargetMode = ParseEarthFixedBeamTargetMode(config.earthFixedBeamTargetMode);
    g_anchorGridSwitchGuardMeters = config.anchorGridSwitchGuardMeters;
    g_anchorGridHysteresisSeconds = config.anchorGridHysteresisSeconds;
    g_outputDir = config.outputDir;
    g_printGridCatalog = config.printGridCatalog;
    g_gridCatalogPath = config.gridCatalogPath;
    g_phyDlTbIntervalSeconds = config.phyDlTbIntervalSeconds;
    g_enablePhyDlTbTrace = config.enablePhyDlTbTrace;
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
    g_measurementDrivenConfig.measurementReportIntervalMs = config.measurementReportIntervalMs;
    g_measurementDrivenConfig.measurementMaxReportCells =
        static_cast<uint8_t>(std::clamp<uint16_t>(config.measurementMaxReportCells, 1, 32));
    g_measurementDrivenConfig.handoverMode = ParseHandoverMode(config.handoverMode);
    g_measurementDrivenConfig.improvedSignalWeight = config.improvedSignalWeight;
    g_measurementDrivenConfig.improvedLoadWeight = config.improvedLoadWeight;
    g_measurementDrivenConfig.improvedRsrqWeight = config.improvedRsrqWeight;
    g_measurementDrivenConfig.improvedVisibilityWeight = config.improvedVisibilityWeight;
    g_measurementDrivenConfig.improvedMinLoadScoreDelta = config.improvedMinLoadScoreDelta;
    g_measurementDrivenConfig.improvedMaxSignalGapDb = config.improvedMaxSignalGapDb;
    g_measurementDrivenConfig.improvedMinStableLeadTimeSeconds =
        config.improvedMinStableLeadTimeSeconds;
    g_measurementDrivenConfig.improvedMinVisibilitySeconds =
        config.improvedMinVisibilitySeconds;
    g_measurementDrivenConfig.improvedVisibilityHorizonSeconds =
        config.improvedVisibilityHorizonSeconds;
    g_measurementDrivenConfig.improvedVisibilityPredictionStepSeconds =
        config.improvedVisibilityPredictionStepSeconds;
    g_measurementDrivenConfig.improvedMinJointScoreMargin =
        config.improvedMinJointScoreMargin;
    g_measurementDrivenConfig.improvedMinCandidateRsrpDbm =
        config.improvedMinCandidateRsrpDbm;
    g_measurementDrivenConfig.improvedMinCandidateRsrqDb =
        config.improvedMinCandidateRsrqDb;
    g_measurementDrivenConfig.improvedServingWeakRsrpDbm =
        config.improvedServingWeakRsrpDbm;
    g_measurementDrivenConfig.improvedServingWeakRsrqDb =
        config.improvedServingWeakRsrqDb;
    g_measurementDrivenConfig.improvedMinRsrqAdvantageDb =
        config.improvedMinRsrqAdvantageDb;
    g_measurementDrivenConfig.improvedEnableCrossLayerPhyAssist =
        config.improvedEnableCrossLayerPhyAssist;
    g_measurementDrivenConfig.improvedCrossLayerPhyAlpha = config.improvedCrossLayerPhyAlpha;
    g_measurementDrivenConfig.improvedCrossLayerTblerThreshold =
        config.improvedCrossLayerTblerThreshold;
    g_measurementDrivenConfig.improvedCrossLayerSinrThresholdDb =
        config.improvedCrossLayerSinrThresholdDb;
    g_measurementDrivenConfig.improvedCrossLayerMinSamples =
        config.improvedCrossLayerMinSamples;
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
    const auto radioModes = ResolveRadioBootstrapModes(cfg);

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
    g_beamModelConfig = DeriveBeamModelConfig(cfg);
    ApplyGlobalMirrorConfig(config);

    const bool outputDirsReady =
        EnsureParentDirectoriesForFiles(g_gridCatalogPath,
                                        cfg.satAnchorTracePath,
                                        cfg.satGroundTrackTracePath,
                                        cfg.handoverThroughputTracePath,
                                        cfg.handoverEventTracePath,
                                        cfg.e2eFlowMetricsPath,
                                        cfg.phyDlTbMetricsPath,
                                        cfg.phyDlTbIntervalMetricsPath,
                                        cfg.ueLayoutPath,
                                        cfg.gridSvgPath,
                                        cfg.gridHtmlPath) &&
        (!cfg.enablePhyDlTbTrace || EnsureParentDirectoryForFile(cfg.phyDlTbTracePath));
    NS_ABORT_MSG_IF(!outputDirsReady, "Failed to create simulation output directories");

    if (radioModes.beamformingMode == BeamformingMode::IDEAL_CELL_SCAN ||
        radioModes.beamformingMode == BeamformingMode::IDEAL_CELL_SCAN_QUASI_OMNI ||
        radioModes.beamformingMode == BeamformingMode::REALISTIC)
    {
        std::cout << "[DIAG-WARN] beamformingMode=" << ToString(radioModes.beamformingMode)
                  << " is intended for short diagnostic runs in the current stack; "
                     "it may trigger NR PHY control/beam-update assertions and should not "
                     "be treated as the formal B00/I31 baseline by default"
                  << std::endl;
    }
    if (radioModes.useRealisticBeamforming && cfg.srsSymbols == 0)
    {
        std::cout << "[DIAG-INFO] beamformingMode=realistic requires SRS feedback; "
                     "automatically promoting srsSymbols from 0 to "
                  << radioModes.effectiveSrsSymbols << " for this run" << std::endl;
    }

    // 生成用于波束锚定和后续可视化的六边形网格。
    if (g_useWgs84HexGrid)
    {
        g_hexGridCells = BuildWgs84HexGrid(g_gridCenterLatitudeDeg,
                                           g_gridCenterLongitudeDeg,
                                           g_gridWidthKm * 1000.0,
                                           g_gridHeightKm * 1000.0,
                                           g_anchorGridHexRadiusKm * 1000.0);
        NS_ABORT_MSG_IF(g_hexGridCells.empty(), "hex-grid generation produced 0 cells");

        if (cfg.startupVerbose)
        {
            std::cout << std::fixed << std::setprecision(3)
                      << "[Grid] WGS84 hex-grid enabled center=(" << g_gridCenterLatitudeDeg << "deg, "
                      << g_gridCenterLongitudeDeg << "deg)"
                      << " size=" << g_gridWidthKm << "x" << g_gridHeightKm << "km"
                      << " anchorHexRadius=" << g_anchorGridHexRadiusKm << "km"
                      << " cells=" << g_hexGridCells.size()
                      << " K=" << g_gridNearestK
                      << " beamExclusion=" << (g_enforceBeamExclusionRing ? "on" : "off")
                      << " exclusionK=" << GetGridAnchorCandidateCount()
                      << " realLinkAccessGate=" << (g_enforceBeamCoverageForRealLinks ? "on" : "off")
                      << " anchorCellGate=" << (g_enforceAnchorCellForRealLinks ? "on" : "off")
                      << " demandAnchor=" << (g_preferDemandAnchorCells ? "on" : "off")
                      << " anchorMode=" << ToString(g_anchorSelectionMode)
                      << " demandSnapshot=" << ToString(g_demandSnapshotMode)
                      << " earthFixedTarget=" << ToString(g_earthFixedBeamTargetMode)
                      << " switchGuard=" << g_anchorGridSwitchGuardMeters << "m"
                      << " hysteresis=" << g_anchorGridHysteresisSeconds << "s"
                      << std::endl;
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

    if (cfg.startupVerbose)
    {
        std::cout << std::fixed << std::setprecision(3)
                  << "[BeamModel] mode=" << (g_useWgs84HexGrid ? "HEX_GRID" : "FIXED_ANCHOR")
                  << " alphaMax=" << cfg.scanMaxDeg << "deg"
                  << " theta3dB=" << cfg.b00BeamwidthDeg << "deg"
                  << " gMax0=" << g_beamModelConfig.gMax0Dbi << "dBi"
                  << " gnbArray=" << cfg.gnbAntennaRows << "x" << cfg.gnbAntennaColumns
                  << " ueArray=" << cfg.ueAntennaRows << "x" << cfg.ueAntennaColumns
                  << " gnbElem=" << ToString(radioModes.gnbAntennaElementMode)
                  << " ueElem=" << ToString(radioModes.ueAntennaElementMode)
                  << " beamforming=" << ToString(radioModes.beamformingMode)
                  << " earthFixedTarget=" << ToString(g_earthFixedBeamTargetMode)
                  << " shadowing=" << (cfg.shadowingEnabled ? "on" : "off") << std::endl;
        std::cout << "[Carrier] centralFrequency=" << cfg.centralFrequency / 1e9 << "GHz"
                  << " bandwidth=" << cfg.bandwidth / 1e6 << "MHz"
                  << " sharedOperationBand=ON"
                  << " coChannelSatellites=" << cfg.gNbNum
                  << " note=current measurement-driven handover is intra-frequency only; "
                     "carrier orthogonalization is diagnostic-only unless inter-frequency "
                     "measurement support is added"
                  << std::endl;
        if (radioModes.useRealisticBeamforming)
        {
            std::cout << "[BeamModel] realisticTrigger="
                      << ToString(radioModes.realisticBfTriggerEvent)
                      << " updatePeriodicity=" << cfg.realisticBfUpdatePeriodicity
                      << " updateDelay=" << cfg.realisticBfUpdateDelayMs << "ms"
                      << " effectiveSrsSymbols=" << radioModes.effectiveSrsSymbols
                      << std::endl;
        }
        std::cout << "[A3-Measure] source=PHY MeasurementReport"
                  << " reportInterval="
                  << g_measurementDrivenConfig.measurementReportIntervalMs << "ms"
                  << " maxReportCells="
                  << static_cast<uint32_t>(g_measurementDrivenConfig.measurementMaxReportCells)
                  << std::endl;
        std::cout << "[Constellation] satellites=" << cfg.gNbNum
                  << " planes=" << cfg.orbitPlaneCount
                  << " ue=" << cfg.ueNum
                  << " raanSpacing=" << cfg.interPlaneRaanSpacingDeg << "deg"
                  << " plane0RaanOffset=" << cfg.plane0RaanOffsetDeg << "deg"
                  << " planeTimeOffset=" << cfg.interPlaneTimeOffsetSeconds << "s"
                  << " plane0TimeOffset=" << cfg.plane0TimeOffsetSeconds << "s"
                  << " overpassGap=" << cfg.overpassGapSeconds << "s"
                  << " plane1OverpassGap=" << cfg.plane1OverpassGapSeconds << "s"
                  << std::endl;
        std::cout << "[UE-Layout] type=" << cfg.ueLayoutType;
        if (cfg.ueLayoutType == "seven-cell")
        {
            std::cout << " groups=center(9)+ring(16 across 6 two-hop cells)"
                      << " layoutHexRadius=" << cfg.hexCellRadiusKm << "km"
                      << " centerSpacing=" << cfg.ueCenterSpacingMeters / 1000.0 << "km"
                      << " ringPointOffset=" << cfg.ueRingPointOffsetMeters / 1000.0 << "km";
        }
        else if (cfg.ueLayoutType == "r2-diagnostic")
        {
            std::cout << " window=two-ring(19 hex centers)"
                      << " layoutHexRadius=" << cfg.hexCellRadiusKm << "km"
                      << " baselineReplacement=no";
        }
        else
        {
            std::cout << " spacing=" << cfg.ueSpacingMeters / 1000.0 << "km";
        }
        std::cout << std::endl;
        std::cout << "[Handover] a3=measurement-report"
                  << " mode=" << ToString(g_measurementDrivenConfig.handoverMode)
                  << " hysteresis=" << g_hoHysteresisDb << "dB"
                  << " ttt=" << static_cast<double>(cfg.hoTttMs) / 1000.0 << "s"
                  << " improvedSignalWeight="
                  << g_measurementDrivenConfig.improvedSignalWeight
                  << " improvedLoadWeight=" << g_measurementDrivenConfig.improvedLoadWeight
                  << " pingPongWindow=" << g_pingPongWindowSeconds << "s";
        if (g_measurementDrivenConfig.handoverMode == HandoverMode::IMPROVED)
        {
            std::cout << " improvedVisibilityWeight="
                      << g_measurementDrivenConfig.improvedVisibilityWeight
                      << " improvedRsrqWeight=" << g_measurementDrivenConfig.improvedRsrqWeight
                      << " improvedMinVisibility="
                      << g_measurementDrivenConfig.improvedMinVisibilitySeconds << "s"
                      << " improvedVisibilityHorizon="
                      << g_measurementDrivenConfig.improvedVisibilityHorizonSeconds
                      << "s"
                      << " improvedMinLoadScoreDelta="
                      << g_measurementDrivenConfig.improvedMinLoadScoreDelta
                      << " improvedMaxSignalGapDb="
                      << g_measurementDrivenConfig.improvedMaxSignalGapDb
                      << " improvedMinStableLead="
                      << g_measurementDrivenConfig.improvedMinStableLeadTimeSeconds << "s"
                      << " improvedVisibilityStep="
                      << g_measurementDrivenConfig.improvedVisibilityPredictionStepSeconds
                      << "s"
                      << " improvedMinJointScoreMargin="
                      << g_measurementDrivenConfig.improvedMinJointScoreMargin
                      << " improvedMinCandidateRsrp="
                      << g_measurementDrivenConfig.improvedMinCandidateRsrpDbm << "dBm"
                      << " improvedMinCandidateRsrq="
                      << g_measurementDrivenConfig.improvedMinCandidateRsrqDb << "dB"
                      << " improvedServingWeakRsrp="
                      << g_measurementDrivenConfig.improvedServingWeakRsrpDbm << "dBm"
                      << " improvedServingWeakRsrq="
                      << g_measurementDrivenConfig.improvedServingWeakRsrqDb << "dB"
                      << " crossLayerPhyAssist="
                      << (g_measurementDrivenConfig.improvedEnableCrossLayerPhyAssist ? "ON"
                                                                                      : "OFF");
            if (g_measurementDrivenConfig.improvedEnableCrossLayerPhyAssist)
            {
                std::cout << " crossLayerPhyAlpha="
                          << g_measurementDrivenConfig.improvedCrossLayerPhyAlpha
                          << " crossLayerTblerTh="
                          << g_measurementDrivenConfig.improvedCrossLayerTblerThreshold
                          << " crossLayerSinrTh="
                          << g_measurementDrivenConfig.improvedCrossLayerSinrThresholdDb << "dB"
                          << " crossLayerMinSamples="
                          << g_measurementDrivenConfig.improvedCrossLayerMinSamples;
            }
        }
        std::cout << std::endl;
        std::cout << "[UserPlane] rlc="
                  << (cfg.forceRlcAmForEpc ? "AM(default override)" : "helper default")
                  << " scheduler=ofdma-rr"
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

        if (cfg.startupVerbose)
        {
            std::cout << "[Setup] autoAlign=ON branch="
                      << (cfg.descendingPass ? "descending" : "ascending")
                      << " peakEl=" << LeoOrbitCalculator::RadToDeg(aligned.peakElevationRad)
                      << "deg" << std::endl;
        }
    }
    if (cfg.startupVerbose && cfg.orbitPlaneCount > 1)
    {
        std::cout << "[Setup] multi-plane mode: plane0 keeps the aligned reference orbit, "
                  << "additional planes use RAAN/time offsets" << std::endl;
    }

    // ------------------------------
    // 4. NR/EPC 与信道配置
    // ------------------------------
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

    g_phyDlTbIntervals.clear();
    auto radioBootstrap =
        BuildNrRadioBootstrap(cfg, radioModes, &ResolveEarthFixedGnbAnchorPosition);
    Ptr<NrHelper> nrHelper = radioBootstrap.nrHelper;
    Ptr<NrPointToPointEpcHelper> nrEpcHelper = radioBootstrap.nrEpcHelper;
    BandwidthPartInfoPtrVector allBwps = radioBootstrap.allBwps;

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
    if (g_useWgs84HexGrid && !g_hexGridCells.empty())
    {
        RefreshDemandGridSnapshotFromPlacements(uePlacements);
        if (cfg.startupVerbose)
        {
            std::vector<uint32_t> demandGridIds(g_demandGridIds.begin(), g_demandGridIds.end());
            std::cout << "[UE-Layout] demandGridIds=" << FormatUintList(demandGridIds)
                      << " demandAnchor=" << (g_preferDemandAnchorCells ? "on" : "off")
                      << " anchorMode=" << ToString(g_anchorSelectionMode)
                      << " demandSnapshot=" << ToString(g_demandSnapshotMode)
                      << std::endl;
        }
    }
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        ueNodes.Get(i)->GetObject<MobilityModel>()->SetPosition(uePlacements[i].groundPoint.ecef);
    }

    g_satelliteGnbDevices.clear();
    g_satelliteGnbDevices.reserve(cfg.gNbNum);

    std::vector<uint32_t> satellitesPerPlane(cfg.orbitPlaneCount, cfg.gNbNum / cfg.orbitPlaneCount);
    for (uint32_t planeIdx = 0; planeIdx < (cfg.gNbNum % cfg.orbitPlaneCount); ++planeIdx)
    {
        satellitesPerPlane[planeIdx]++;
    }

    // 按全局索引顺序安装，保证索引映射正确
    NetDeviceContainer gNbNetDev;
    for (uint32_t satIdx = 0; satIdx < cfg.gNbNum; ++satIdx)
    {
        NodeContainer singleNode;
        singleNode.Add(gNbNodes.Get(satIdx));
        NetDeviceContainer singleDev = nrHelper->InstallGnbDevice(singleNode, allBwps);
        gNbNetDev.Add(singleDev);
        g_satelliteGnbDevices.push_back(DynamicCast<NrGnbNetDevice>(singleDev.Get(0)));
    }

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

    // 为每颗卫星绑定轨道、初始位置和初始网格锚点。
    // 注意：satellitesPerPlane 和 globalSatIdx 已在前面 carrier group 计算时定义
    uint32_t globalSatIdx = 0;
    for (uint32_t planeIdx = 0; planeIdx < cfg.orbitPlaneCount; ++planeIdx)
    {
        const uint32_t satsInPlane = satellitesPerPlane[planeIdx];
        const double planeRaanRad = ComputePlaneRaanRad(raanRad,
                                                        cfg.interPlaneRaanSpacingDeg,
                                                        cfg.plane0RaanOffsetDeg,
                                                        planeIdx);
        const double planeRaanDeg = LeoOrbitCalculator::RadToDeg(planeRaanRad);

        for (uint32_t slotIdx = 0; slotIdx < satsInPlane; ++slotIdx, ++globalSatIdx)
        {
            auto dev = g_satelliteGnbDevices[globalSatIdx];
            auto rrc = dev->GetRrc();

            const double overpassTime =
                ComputeSatelliteOverpassTime(cfg.alignmentReferenceTimeSeconds,
                                             cfg.overpassGapSeconds,
                                             cfg.overpassTimeOffsetSeconds,
                                             cfg.interPlaneTimeOffsetSeconds,
                                             cfg.plane0TimeOffsetSeconds,
                                             cfg.plane1OverpassGapSeconds,
                                             planeIdx,
                                             slotIdx,
                                             satsInPlane);
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
                const auto blockedGridIds = BuildBeamAnchorExclusionSet(std::numeric_limits<uint32_t>::max());
                const auto anchor = ComputeDemandAwareGridAnchorSelection(initState.ecef,
                                                                          GetGridAnchorCandidateCount(),
                                                                          blockedGridIds);
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

            if (cfg.startupVerbose)
            {
                std::cout << "[Setup] sat" << globalSatIdx << " plane=" << planeIdx
                          << " slot=" << slotIdx << " cell=" << dev->GetCellId()
                          << " nu0=" << LeoOrbitCalculator::RadToDeg(orbit.trueAnomalyAtEpochRad)
                          << "deg"
                          << " i=" << cfg.orbitInclinationDeg << "deg"
                          << " RAAN=" << planeRaanDeg << "deg";
                if (g_useWgs84HexGrid && g_satellites[globalSatIdx].currentAnchorGridId != 0)
                {
                    std::cout << " anchorGrid=" << g_satellites[globalSatIdx].currentAnchorGridId;
                }
                std::cout << std::endl;
            }
        }
    }

    if (cfg.startupVerbose)
    {
        std::cout << "=== Radio Plan ===" << std::endl;
        std::cout << "operationBand: " << cfg.centralFrequency / 1e6 << " MHz / "
                  << cfg.bandwidth / 1e6 << " MHz / 1 CC / same-frequency" << std::endl;
        for (uint32_t satIdx = 0; satIdx < g_satellites.size(); ++satIdx)
        {
            const auto& sat = g_satellites[satIdx];
            std::cout << "Sat " << satIdx << " plane=" << sat.orbitPlaneIndex
                      << " slot=" << sat.orbitSlotIndex
                      << " cell=" << sat.dev->GetCellId()
                      << std::endl;
        }
        std::cout << "==================" << std::endl;
    }

    InstallStaticNeighbourRelations();
    InstallMeasurementDrivenHandoverAlgorithms(
        g_satellites, g_handoverAlgorithms, cfg, &HandleMeasurementDrivenHandoverReport);

    auto remoteHostWithAddr = nrEpcHelper->SetupRemoteHost("100Gb/s", 2500, Seconds(0.0));
    Ptr<Node> remoteHost = remoteHostWithAddr.first;

    InternetStackHelper internet;
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIpIface = nrEpcHelper->AssignUeIpv4Address(NetDeviceContainer(ueNetDev));
    Ipv4StaticRoutingHelper routingHelper;
    DisableUeIpv4ForwardingIfRequested(cfg, ueNodes);

    // ------------------------------
    // 7. UE 初始接入与业务安装
    // ------------------------------
    UeScenarioInstallContext ueScenarioContext{g_satellites, g_ues, g_imsiToUe};
    ueScenarioContext.hexGridCells = &g_hexGridCells;
    ueScenarioContext.gmstAtEpochRad = g_gmstAtEpochRad;
    ueScenarioContext.minElevationRad = g_minElevationRad;
    ueScenarioContext.beamModelConfig = g_beamModelConfig;
    ueScenarioContext.resolveSatelliteBeamTargetEcef = &ResolveSatelliteBeamTargetEcef;
    InstallUeInitialAttachAndTraffic(ueScenarioContext,
                                     cfg,
                                     ueNodes,
                                     ueNetDev,
                                     gNbNetDev,
                                     uePlacements,
                                     ueIpIface,
                                     remoteHost,
                                     nrHelper,
                                     nrEpcHelper,
                                     routingHelper,
                                     [](const Ptr<NrUeNetDevice>&) {});

    FlowMonitorHelper flowMonitorHelper;
    NodeContainer flowMonitorNodes;
    flowMonitorNodes.Add(remoteHost);
    flowMonitorNodes.Add(ueNodes);
    Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.Install(flowMonitorNodes);
    flowMonitor->SetAttribute("DelayBinWidth", DoubleValue(0.001));
    flowMonitor->SetAttribute("JitterBinWidth", DoubleValue(0.001));
    flowMonitor->SetAttribute("PacketSizeBinWidth", DoubleValue(20));

    // ------------------------------
    // 8. 运行时复位、回调注册与周期事件调度
    // ------------------------------
    ResetRuntimeState(cfg.gNbNum);
    Config::Connect(
        "/NodeList/*/DeviceList/*/ComponentCarrierMapUe/*/NrUePhy/SpectrumPhy/RxPacketTraceUe",
        MakeCallback(&ReportDlPhyTbTrace));
    const BaselineTraceOutputSet traceOutputs{&g_satAnchorTrace,
                                              &g_satGroundTrackTrace,
                                              &g_handoverThroughputTrace,
                                              &g_handoverEventTrace,
                                              &g_phyDlTbTrace};
    InitializeBaselineTraceOutputs(cfg, traceOutputs);

    Config::Connect("/NodeList/*/DeviceList/*/$ns3::NrGnbNetDevice/NrGnbRrc/HandoverStart",
                    MakeCallback(&ReportHandoverStart));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::NrGnbNetDevice/NrGnbRrc/HandoverEndOk",
                    MakeCallback(&ReportHandoverEndOk));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::NrGnbNetDevice/NrGnbRrc/HandoverFailureNoPreamble",
                    MakeCallback(&ReportHandoverFailureNoPreamble));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::NrGnbNetDevice/NrGnbRrc/HandoverFailureMaxRach",
                    MakeCallback(&ReportHandoverFailureMaxRach));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::NrGnbNetDevice/NrGnbRrc/HandoverFailureLeaving",
                    MakeCallback(&ReportHandoverFailureLeaving));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::NrGnbNetDevice/NrGnbRrc/HandoverFailureJoining",
                    MakeCallback(&ReportHandoverFailureJoining));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::NrUeNetDevice/NrUeRrc/HandoverEndError",
                    MakeCallback(&ReportHandoverEndError));
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
        if (cfg.startupVerbose)
        {
            std::cout << "[Setup] ue" << ueIdx << " start attach=sat" << ue.initialAttachIdx
                      << "(cell=" << g_satellites[ue.initialAttachIdx].dev->GetCellId() << ")"
                      << " role=" << ue.placementRole
                      << " offset=(" << ue.eastOffsetMeters / 1000.0 << "kmE,"
                      << ue.northOffsetMeters / 1000.0 << "kmN)"
                      << " handoverMode="
                      << ToString(g_measurementDrivenConfig.handoverMode) << std::endl;
        }
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
    flowMonitor->CheckForLostPackets();
    const auto flowClassifier = DynamicCast<Ipv4FlowClassifier>(flowMonitorHelper.GetClassifier());
    const E2eFlowAggregate e2eAggregate =
        BuildE2eFlowAggregate(g_ues,
                              flowMonitor->GetFlowStats(),
                              flowClassifier,
                              appDuration,
                              cfg.udpPacketSize);
    const PhyDlTbAggregate phyDlTbAggregate = BuildPhyDlTbAggregate(g_ues);
    const PhyDlTbIntervalAggregate phyDlTbIntervalAggregate =
        BuildPhyDlTbIntervalAggregate(g_phyDlTbIntervals, g_phyDlTbIntervalSeconds);

    PrintDlTrafficSummary(g_ues, appDuration, cfg.udpPacketSize);
    PrintE2eFlowSummary(e2eAggregate);
    PrintPhyDlTbSummary(phyDlTbAggregate);
    PrintHandoverSummary(g_ues, g_pingPongWindowSeconds);
    NS_ABORT_MSG_IF(!WriteE2eFlowMetricsCsv(cfg.e2eFlowMetricsPath, e2eAggregate),
                    "Failed to write E2E flow metrics CSV: " << cfg.e2eFlowMetricsPath);
    NS_ABORT_MSG_IF(!WritePhyDlTbMetricsCsv(cfg.phyDlTbMetricsPath, phyDlTbAggregate),
                    "Failed to write PHY DL TB metrics CSV: " << cfg.phyDlTbMetricsPath);
    NS_ABORT_MSG_IF(!WritePhyDlTbIntervalMetricsCsv(cfg.phyDlTbIntervalMetricsPath,
                                                    phyDlTbIntervalAggregate),
                    "Failed to write PHY DL TB interval metrics CSV: "
                        << cfg.phyDlTbIntervalMetricsPath);

    CloseBaselineTraceOutputs(traceOutputs);

    if (cfg.runGridSvgScript)
    {
        if (!g_useWgs84HexGrid || !g_printGridCatalog)
        {
            if (cfg.startupVerbose)
            {
                std::cout << "[GridSvg] skipped because hex-grid catalog export is disabled"
                          << std::endl;
            }
        }
        else
        {
            std::ostringstream cmdline;
            cmdline << "python3 \"" << cfg.plotHexGridScriptPath << "\""
                    << " --csv \"" << g_gridCatalogPath << "\""
                    << " --sat-anchor-csv \"" << cfg.satAnchorTracePath << "\""
                    << " --sat-ground-track-csv \"" << cfg.satGroundTrackTracePath << "\""
                    << " --out \"" << cfg.gridSvgPath << "\""
                    << " --html-out \"" << cfg.gridHtmlPath << "\"";
            const int scriptRc = std::system(cmdline.str().c_str());
            if (scriptRc == 0)
            {
                if (cfg.startupVerbose)
                {
                    std::cout << "[GridSvg] SVG/HTML generated successfully" << std::endl;
                }
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
