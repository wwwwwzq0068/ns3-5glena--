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
#include "handover/leo-orbit-calculator.h"
#include "handover/leo-ntn-handover-reporting.h"
#include "handover/leo-ntn-handover-runtime.h"
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

// 当前 handover 主链统一走标准 PHY/RRC MeasurementReport。
enum class HandoverMode
{
    BASELINE,
    IMPROVED,
};

static HandoverMode g_handoverMode = HandoverMode::BASELINE;

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
static double g_improvedSignalWeight = 0.7;
static double g_improvedRsrqWeight = 0.3;
static double g_improvedLoadWeight = 0.3;
static double g_improvedVisibilityWeight = 0.2;
static double g_improvedMinLoadScoreDelta = 0.2;
static double g_improvedMaxSignalGapDb = 3.0;
static double g_improvedMinStableLeadTimeSeconds = 0.12;
static double g_improvedMinVisibilitySeconds = 1.0;
static double g_improvedVisibilityHorizonSeconds = 8.0;
static double g_improvedVisibilityPredictionStepSeconds = 0.5;
static double g_improvedMinJointScoreMargin = 0.03;
static double g_improvedMinCandidateRsrpDbm = -110.0;
static double g_improvedMinCandidateRsrqDb = -17.0;
static double g_improvedServingWeakRsrpDbm = -108.0;
static double g_improvedServingWeakRsrqDb = -15.0;
static double g_improvedMinRsrqAdvantageDb = 0.0;
static bool g_improvedEnableCrossLayerPhyAssist = false;
static double g_improvedCrossLayerPhyAlpha = 0.02;
static double g_improvedCrossLayerTblerThreshold = 0.48;
static double g_improvedCrossLayerSinrThresholdDb = -5.0;
static uint32_t g_improvedCrossLayerMinSamples = 50;
static uint16_t g_measurementReportIntervalMs = 120;
static uint8_t g_measurementMaxReportCells = 8;
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

enum class AntennaElementMode
{
    ISOTROPIC,
    THREE_GPP,
    B00_CUSTOM,
};

static AntennaElementMode
ParseAntennaElementMode(const std::string& mode)
{
    if (mode == "three-gpp")
    {
        return AntennaElementMode::THREE_GPP;
    }
    if (mode == "b00-custom")
    {
        return AntennaElementMode::B00_CUSTOM;
    }
    return AntennaElementMode::ISOTROPIC;
}

static const char*
ToString(AntennaElementMode mode)
{
    if (mode == AntennaElementMode::THREE_GPP)
    {
        return "three-gpp";
    }
    if (mode == AntennaElementMode::B00_CUSTOM)
    {
        return "b00-custom";
    }
    return "isotropic";
}

static Ptr<AntennaModel>
CreateAntennaElement(AntennaElementMode mode, const BaselineSimulationConfig& cfg)
{
    if (mode == AntennaElementMode::THREE_GPP)
    {
        return CreateObject<ThreeGppAntennaModel>();
    }
    if (mode == AntennaElementMode::B00_CUSTOM)
    {
        Ptr<B00EquivalentAntennaModel> model = CreateObject<B00EquivalentAntennaModel>();
        model->SetMaxGainDb(cfg.b00MaxGainDb);
        model->SetBeamwidthDeg(cfg.b00BeamwidthDeg);
        model->SetMaxAttenuationDb(cfg.b00MaxAttenuationDb);
        return model;
    }
    return CreateObject<IsotropicAntennaModel>();
}

enum class BeamformingMode
{
    IDEAL_DIRECT_PATH,
    IDEAL_EARTH_FIXED,
    IDEAL_CELL_SCAN,
    IDEAL_DIRECT_PATH_QUASI_OMNI,
    IDEAL_CELL_SCAN_QUASI_OMNI,
    IDEAL_QUASI_OMNI_DIRECT_PATH,
    REALISTIC,
};

static BeamformingMode
ParseBeamformingMode(const std::string& mode)
{
    if (mode == "ideal-earth-fixed")
    {
        return BeamformingMode::IDEAL_EARTH_FIXED;
    }
    if (mode == "ideal-cell-scan")
    {
        return BeamformingMode::IDEAL_CELL_SCAN;
    }
    if (mode == "ideal-direct-path-quasi-omni")
    {
        return BeamformingMode::IDEAL_DIRECT_PATH_QUASI_OMNI;
    }
    if (mode == "ideal-cell-scan-quasi-omni")
    {
        return BeamformingMode::IDEAL_CELL_SCAN_QUASI_OMNI;
    }
    if (mode == "ideal-quasi-omni-direct-path")
    {
        return BeamformingMode::IDEAL_QUASI_OMNI_DIRECT_PATH;
    }
    if (mode == "realistic")
    {
        return BeamformingMode::REALISTIC;
    }
    return BeamformingMode::IDEAL_DIRECT_PATH;
}

static const char*
ToString(BeamformingMode mode)
{
    switch (mode)
    {
    case BeamformingMode::IDEAL_EARTH_FIXED:
        return "ideal-earth-fixed";
    case BeamformingMode::IDEAL_CELL_SCAN:
        return "ideal-cell-scan";
    case BeamformingMode::IDEAL_DIRECT_PATH_QUASI_OMNI:
        return "ideal-direct-path-quasi-omni";
    case BeamformingMode::IDEAL_CELL_SCAN_QUASI_OMNI:
        return "ideal-cell-scan-quasi-omni";
    case BeamformingMode::IDEAL_QUASI_OMNI_DIRECT_PATH:
        return "ideal-quasi-omni-direct-path";
    case BeamformingMode::REALISTIC:
        return "realistic";
    case BeamformingMode::IDEAL_DIRECT_PATH:
    default:
        return "ideal-direct-path";
    }
}

static TypeId
GetIdealBeamformingMethodTypeId(BeamformingMode mode)
{
    switch (mode)
    {
    case BeamformingMode::IDEAL_EARTH_FIXED:
        return EarthFixedGnbBeamforming::GetTypeId();
    case BeamformingMode::IDEAL_CELL_SCAN:
        return CellScanBeamforming::GetTypeId();
    case BeamformingMode::IDEAL_DIRECT_PATH_QUASI_OMNI:
        return DirectPathQuasiOmniBeamforming::GetTypeId();
    case BeamformingMode::IDEAL_CELL_SCAN_QUASI_OMNI:
        return CellScanQuasiOmniBeamforming::GetTypeId();
    case BeamformingMode::IDEAL_QUASI_OMNI_DIRECT_PATH:
        return QuasiOmniDirectPathBeamforming::GetTypeId();
    case BeamformingMode::IDEAL_DIRECT_PATH:
    default:
        return DirectPathBeamforming::GetTypeId();
    }
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

static RealisticBfManager::TriggerEvent
ParseRealisticBfTriggerEvent(const std::string& triggerEvent)
{
    if (triggerEvent == "delayed-update")
    {
        return RealisticBfManager::DELAYED_UPDATE;
    }
    return RealisticBfManager::SRS_COUNT;
}

static const char*
ToString(RealisticBfManager::TriggerEvent triggerEvent)
{
    return triggerEvent == RealisticBfManager::DELAYED_UPDATE ? "delayed-update" : "srs-count";
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

    const double alpha = std::clamp(g_improvedCrossLayerPhyAlpha, 1e-6, 1.0);
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

struct MeasurementCandidate
{
    uint32_t satIdx = std::numeric_limits<uint32_t>::max();
    uint16_t cellId = 0;
    double rsrpDbm = -std::numeric_limits<double>::infinity();
    double rsrqDb = -std::numeric_limits<double>::infinity();
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
NormalizeMetric(double value, double minValue, double maxValue)
{
    if (!std::isfinite(value) || !std::isfinite(minValue) || !std::isfinite(maxValue))
    {
        return 0.0;
    }
    const double span = std::max(1e-9, maxValue - minValue);
    return std::clamp((value - minValue) / span, 0.0, 1.0);
}

static double
ComputeVisibilityScore(double remainingVisibilitySeconds)
{
    return std::clamp(remainingVisibilitySeconds /
                          std::max(1e-9, g_improvedVisibilityHorizonSeconds),
                      0.0,
                      1.0);
}

static double
ComputeJointScore(const MeasurementCandidate& candidate,
                  double minRsrpDbm,
                  double maxRsrpDbm,
                  double minRsrqDb,
                  double maxRsrqDb,
                  double normalizedSignalWeight,
                  double normalizedRsrqWeight,
                  double normalizedLoadWeight,
                  double normalizedVisibilityWeight,
                  double sourceLoadScore,
                  double sourceLoadPressure)
{
    const double rsrpScore = NormalizeMetric(candidate.rsrpDbm, minRsrpDbm, maxRsrpDbm);
    const double rsrqScore = NormalizeMetric(candidate.rsrqDb, minRsrqDb, maxRsrqDb);
    const double loadUtility = std::clamp(1.0 - candidate.loadScore, 0.0, 1.0);
    const double loadReliefUtility = std::clamp(sourceLoadScore - candidate.loadScore, 0.0, 1.0);
    return normalizedSignalWeight * rsrpScore +
           normalizedRsrqWeight * rsrqScore +
           normalizedLoadWeight * loadUtility +
           normalizedVisibilityWeight * candidate.visibilityScore +
           0.25 * sourceLoadPressure * loadReliefUtility;
}

static double
ComputeLoadPressureFromScore(double loadScore)
{
    // 将负载分数按当前拥塞阈值重新归一化，便于在源站接近拥塞时更快放大负载导向。
    const double congestionSpan = std::max(1e-9, g_loadCongestionThreshold);
    return std::clamp(loadScore / congestionSpan, 0.0, 1.0);
}

static bool
IsServingLinkWeak(const NrRrcSap::MeasResults& measResults)
{
    const double servingRsrpDbm =
        nr::EutranMeasurementMapping::RsrpRange2Dbm(measResults.measResultPCell.rsrpResult);
    const double servingRsrqDb =
        nr::EutranMeasurementMapping::RsrqRange2Db(measResults.measResultPCell.rsrqResult);
    return servingRsrpDbm <= g_improvedServingWeakRsrpDbm ||
           servingRsrqDb <= g_improvedServingWeakRsrqDb;
}

static bool
IsCrossLayerPhyWeak(const UeRuntime& ue)
{
    if (!g_improvedEnableCrossLayerPhyAssist ||
        ue.recentPhySampleCount < g_improvedCrossLayerMinSamples)
    {
        return false;
    }

    const bool tblerWeak = ue.recentPhyTblerEwma >= g_improvedCrossLayerTblerThreshold;
    const bool sinrWeak = std::isfinite(ue.recentPhySinrDbEwma) &&
                          ue.recentPhySinrDbEwma <= g_improvedCrossLayerSinrThresholdDb;
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
        if (!IsUeInsideAssignedBeam(satIt->second, ue))
        {
            continue;
        }

        const auto& satellite = g_satellites[satIt->second];
        MeasurementCandidate candidate;
        candidate.satIdx = satIt->second;
        candidate.cellId = neighbour.physCellId;
        candidate.rsrpDbm = nr::EutranMeasurementMapping::RsrpRange2Dbm(neighbour.rsrpResult);
        if (neighbour.haveRsrqResult)
        {
            candidate.rsrqDb = nr::EutranMeasurementMapping::RsrqRange2Db(neighbour.rsrqResult);
        }
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
    MeasurementCandidate servingCandidate;
    servingCandidate.satIdx = sourceSatIdx;
    servingCandidate.cellId = servingCellId;
    servingCandidate.rsrpDbm =
        nr::EutranMeasurementMapping::RsrpRange2Dbm(measResults.measResultPCell.rsrpResult);
    servingCandidate.rsrqDb =
        nr::EutranMeasurementMapping::RsrqRange2Db(measResults.measResultPCell.rsrqResult);
    servingCandidate.loadScore = sourceLoadScore;
    servingCandidate.admissionAllowed = true;
    servingCandidate.remainingVisibilitySeconds =
        PredictRemainingVisibilitySeconds(sourceSatIdx, ue.groundPoint, Simulator::Now().GetSeconds());

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

    std::vector<MeasurementCandidate> qualityQualifiedCandidates;
    qualityQualifiedCandidates.reserve(filteredCandidates.size());
    std::copy_if(filteredCandidates.begin(),
                 filteredCandidates.end(),
                 std::back_inserter(qualityQualifiedCandidates),
                 [](const auto& candidate) {
                     const bool rsrpOk = candidate.rsrpDbm >= g_improvedMinCandidateRsrpDbm;
                     const bool rsrqOk = std::isfinite(candidate.rsrqDb) &&
                                         candidate.rsrqDb >= g_improvedMinCandidateRsrqDb;
                     return rsrpOk && rsrqOk;
                 });
    if (!qualityQualifiedCandidates.empty())
    {
        filteredCandidates = std::move(qualityQualifiedCandidates);
    }

    // Same-frequency interference guard: a target with stronger RSRP still needs
    // enough RSRQ advantage over serving, otherwise we risk cutting into a trap.
    const double servingRsrqDb =
        std::isfinite(servingCandidate.rsrqDb) ? servingCandidate.rsrqDb : -19.5;
    const bool servingWeak = servingCandidate.rsrpDbm <= g_improvedServingWeakRsrpDbm ||
                             servingCandidate.rsrqDb <= g_improvedServingWeakRsrqDb;
    if (g_improvedMinRsrqAdvantageDb > 0.0 && !servingWeak)
    {
        std::vector<MeasurementCandidate> rsrqGuardCandidates;
        rsrqGuardCandidates.reserve(filteredCandidates.size());
        for (const auto& candidate : filteredCandidates)
        {
            const bool rsrqAdvantageOk = std::isfinite(candidate.rsrqDb) &&
                                         candidate.rsrqDb >=
                                             servingRsrqDb + g_improvedMinRsrqAdvantageDb;
            if (rsrqAdvantageOk)
            {
                rsrqGuardCandidates.push_back(candidate);
            }
        }
        if (!rsrqGuardCandidates.empty())
        {
            filteredCandidates = std::move(rsrqGuardCandidates);
        }
        else if (!filteredCandidates.empty())
        {
            const auto bestSignal = std::max_element(filteredCandidates.begin(),
                                                     filteredCandidates.end(),
                                                     [](const auto& lhs, const auto& rhs) {
                                                         return lhs.rsrpDbm < rhs.rsrpDbm;
                                                     });
            if (bestSignal->rsrpDbm > servingCandidate.rsrpDbm)
            {
                return *bestSignal;
            }
        }
    }

    const auto bestSignalIt =
        std::max_element(filteredCandidates.begin(),
                         filteredCandidates.end(),
                         [](const auto& lhs, const auto& rhs) {
                             return lhs.rsrpDbm < rhs.rsrpDbm;
                         });
    const MeasurementCandidate bestSignalCandidate = *bestSignalIt;
    const bool crossLayerPhyWeak = IsCrossLayerPhyWeak(ue);
    if (servingWeak || crossLayerPhyWeak)
    {
        return bestSignalCandidate;
    }

    const double dynamicMaxSignalGapDb =
        g_improvedMaxSignalGapDb + 2.0 * sourceLoadPressure;
    const double dynamicMinLoadScoreDelta =
        std::max(0.05, g_improvedMinLoadScoreDelta * (1.0 - 0.5 * sourceLoadPressure));
    const double effectiveSignalWeight =
        std::max(0.15, g_improvedSignalWeight * (1.0 - 0.4 * sourceLoadPressure));
    const double effectiveRsrqWeight =
        std::max(0.0, g_improvedRsrqWeight * (1.0 - 0.2 * sourceLoadPressure));
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
    double minRsrqDb = std::numeric_limits<double>::infinity();
    double maxRsrqDb = -std::numeric_limits<double>::infinity();
    if (std::isfinite(servingCandidate.rsrpDbm))
    {
        minRsrpDbm = servingCandidate.rsrpDbm;
        maxRsrpDbm = servingCandidate.rsrpDbm;
    }
    if (std::isfinite(servingCandidate.rsrqDb))
    {
        minRsrqDb = servingCandidate.rsrqDb;
        maxRsrqDb = servingCandidate.rsrqDb;
    }
    for (const auto& candidate : scoredCandidates)
    {
        minRsrpDbm = std::min(minRsrpDbm, candidate.rsrpDbm);
        maxRsrpDbm = std::max(maxRsrpDbm, candidate.rsrpDbm);
        if (std::isfinite(candidate.rsrqDb))
        {
            minRsrqDb = std::min(minRsrqDb, candidate.rsrqDb);
            maxRsrqDb = std::max(maxRsrqDb, candidate.rsrqDb);
        }
    }

    const double totalWeight = std::max(
        1e-9,
        effectiveSignalWeight + effectiveRsrqWeight + effectiveLoadWeight + effectiveVisibilityWeight);
    const double normalizedSignalWeight = effectiveSignalWeight / totalWeight;
    const double normalizedRsrqWeight = effectiveRsrqWeight / totalWeight;
    const double normalizedLoadWeight = effectiveLoadWeight / totalWeight;
    const double normalizedVisibilityWeight = effectiveVisibilityWeight / totalWeight;

    for (auto& candidate : scoredCandidates)
    {
        candidate.visibilityScore = ComputeVisibilityScore(candidate.remainingVisibilitySeconds);
        candidate.jointScore = ComputeJointScore(candidate,
                                                minRsrpDbm,
                                                maxRsrpDbm,
                                                minRsrqDb,
                                                maxRsrqDb,
                                                normalizedSignalWeight,
                                                normalizedRsrqWeight,
                                                normalizedLoadWeight,
                                                normalizedVisibilityWeight,
                                                sourceLoadScore,
                                                sourceLoadPressure);
    }

    const auto bestJointIt =
        std::max_element(scoredCandidates.begin(),
                         scoredCandidates.end(),
                         [](const auto& lhs, const auto& rhs) {
                             if (std::abs(lhs.jointScore - rhs.jointScore) > 1e-9)
                             {
                                 return lhs.jointScore < rhs.jointScore;
                             }
                             return lhs.rsrpDbm < rhs.rsrpDbm;
                         });

    if (std::isfinite(servingCandidate.rsrpDbm))
    {
        servingCandidate.visibilityScore =
            ComputeVisibilityScore(servingCandidate.remainingVisibilitySeconds);
        servingCandidate.jointScore = ComputeJointScore(servingCandidate,
                                                        minRsrpDbm,
                                                        maxRsrpDbm,
                                                        minRsrqDb,
                                                        maxRsrqDb,
                                                        normalizedSignalWeight,
                                                        normalizedRsrqWeight,
                                                        normalizedLoadWeight,
                                                        normalizedVisibilityWeight,
                                                        sourceLoadScore,
                                                        sourceLoadPressure);
        if (bestJointIt->jointScore < servingCandidate.jointScore + g_improvedMinJointScoreMargin)
        {
            return std::nullopt;
        }
    }

    return *bestJointIt;
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

    const bool servingWeak =
        (g_handoverMode == HandoverMode::IMPROVED) &&
        (IsServingLinkWeak(measResults) || IsCrossLayerPhyWeak(ue));

    const auto selectedTarget = SelectMeasurementDrivenTarget(sourceCellId,
                                                              static_cast<uint32_t>(sourceSatIdx),
                                                              ue,
                                                              measResults);
    if (!selectedTarget.has_value() || selectedTarget->cellId == 0)
    {
        if (g_handoverMode == HandoverMode::IMPROVED)
        {
            ClearStableLeadTracking(ue);
        }
        return;
    }

    if (g_handoverMode == HandoverMode::IMPROVED && !servingWeak)
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

        if (g_improvedMinStableLeadTimeSeconds > 0.0 &&
            (nowSeconds - ue.stableLeadSinceSeconds + 1e-9 < g_improvedMinStableLeadTimeSeconds))
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
    g_measurementReportIntervalMs = config.measurementReportIntervalMs;
    g_measurementMaxReportCells =
        static_cast<uint8_t>(std::clamp<uint16_t>(config.measurementMaxReportCells, 1, 32));
    g_handoverMode = ParseHandoverMode(config.handoverMode);
    g_improvedSignalWeight = config.improvedSignalWeight;
    g_improvedLoadWeight = config.improvedLoadWeight;
    g_improvedRsrqWeight = config.improvedRsrqWeight;
    g_improvedVisibilityWeight = config.improvedVisibilityWeight;
    g_improvedMinLoadScoreDelta = config.improvedMinLoadScoreDelta;
    g_improvedMaxSignalGapDb = config.improvedMaxSignalGapDb;
    g_improvedMinStableLeadTimeSeconds = config.improvedMinStableLeadTimeSeconds;
    g_improvedMinVisibilitySeconds = config.improvedMinVisibilitySeconds;
    g_improvedVisibilityHorizonSeconds = config.improvedVisibilityHorizonSeconds;
    g_improvedVisibilityPredictionStepSeconds = config.improvedVisibilityPredictionStepSeconds;
    g_improvedMinJointScoreMargin = config.improvedMinJointScoreMargin;
    g_improvedMinCandidateRsrpDbm = config.improvedMinCandidateRsrpDbm;
    g_improvedMinCandidateRsrqDb = config.improvedMinCandidateRsrqDb;
    g_improvedServingWeakRsrpDbm = config.improvedServingWeakRsrpDbm;
    g_improvedServingWeakRsrqDb = config.improvedServingWeakRsrqDb;
    g_improvedMinRsrqAdvantageDb = config.improvedMinRsrqAdvantageDb;
    g_improvedEnableCrossLayerPhyAssist = config.improvedEnableCrossLayerPhyAssist;
    g_improvedCrossLayerPhyAlpha = config.improvedCrossLayerPhyAlpha;
    g_improvedCrossLayerTblerThreshold = config.improvedCrossLayerTblerThreshold;
    g_improvedCrossLayerSinrThresholdDb = config.improvedCrossLayerSinrThresholdDb;
    g_improvedCrossLayerMinSamples = config.improvedCrossLayerMinSamples;
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
    const auto gnbAntennaElementMode = ParseAntennaElementMode(cfg.gnbAntennaElement);
    const auto ueAntennaElementMode = ParseAntennaElementMode(cfg.ueAntennaElement);
    const auto beamformingMode = ParseBeamformingMode(cfg.beamformingMode);
    const auto realisticBfTriggerEvent =
        ParseRealisticBfTriggerEvent(cfg.realisticBfTriggerEvent);
    const bool useRealisticBeamforming = (beamformingMode == BeamformingMode::REALISTIC);
    const uint32_t effectiveSrsSymbols =
        (useRealisticBeamforming && cfg.srsSymbols == 0) ? 1u : cfg.srsSymbols;

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
                                 EnsureParentDirectoryForFile(cfg.satGroundTrackTracePath) &&
                                 EnsureParentDirectoryForFile(cfg.handoverThroughputTracePath) &&
                                 EnsureParentDirectoryForFile(cfg.handoverEventTracePath) &&
                                 EnsureParentDirectoryForFile(cfg.e2eFlowMetricsPath) &&
                                 EnsureParentDirectoryForFile(cfg.phyDlTbMetricsPath) &&
                                 EnsureParentDirectoryForFile(cfg.phyDlTbIntervalMetricsPath) &&
                                 (!cfg.enablePhyDlTbTrace ||
                                  EnsureParentDirectoryForFile(cfg.phyDlTbTracePath)) &&
                                 EnsureParentDirectoryForFile(cfg.ueLayoutPath) &&
                                 EnsureParentDirectoryForFile(cfg.gridSvgPath) &&
                                 EnsureParentDirectoryForFile(cfg.gridHtmlPath);
    NS_ABORT_MSG_IF(!outputDirsReady, "Failed to create simulation output directories");

    if (beamformingMode == BeamformingMode::IDEAL_CELL_SCAN ||
        beamformingMode == BeamformingMode::IDEAL_CELL_SCAN_QUASI_OMNI ||
        beamformingMode == BeamformingMode::REALISTIC)
    {
        std::cout << "[DIAG-WARN] beamformingMode=" << ToString(beamformingMode)
                  << " is intended for short diagnostic runs in the current stack; "
                     "it may trigger NR PHY control/beam-update assertions and should not "
                     "be treated as the formal B00/I31 baseline by default"
                  << std::endl;
    }
    if (useRealisticBeamforming && cfg.srsSymbols == 0)
    {
        std::cout << "[DIAG-INFO] beamformingMode=realistic requires SRS feedback; "
                     "automatically promoting srsSymbols from 0 to "
                  << effectiveSrsSymbols << " for this run" << std::endl;
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
                  << " theta3dB=" << cfg.theta3dBDeg << "deg"
                  << " gnbArray=" << cfg.gnbAntennaRows << "x" << cfg.gnbAntennaColumns
                  << " ueArray=" << cfg.ueAntennaRows << "x" << cfg.ueAntennaColumns
                  << " gnbElem=" << ToString(gnbAntennaElementMode)
                  << " ueElem=" << ToString(ueAntennaElementMode)
                  << " beamforming=" << ToString(beamformingMode)
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
        if (useRealisticBeamforming)
        {
            std::cout << "[BeamModel] realisticTrigger="
                      << ToString(realisticBfTriggerEvent)
                      << " updatePeriodicity=" << cfg.realisticBfUpdatePeriodicity
                      << " updateDelay=" << cfg.realisticBfUpdateDelayMs << "ms"
                      << " effectiveSrsSymbols=" << effectiveSrsSymbols
                      << std::endl;
        }
        std::cout << "[A3-Measure] source=PHY MeasurementReport"
                  << " reportInterval=" << g_measurementReportIntervalMs << "ms"
                  << " maxReportCells=" << static_cast<uint32_t>(g_measurementMaxReportCells)
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
                  << " mode=" << ToString(g_handoverMode)
                  << " hysteresis=" << g_hoHysteresisDb << "dB"
                  << " ttt=" << static_cast<double>(cfg.hoTttMs) / 1000.0 << "s"
                  << " improvedSignalWeight=" << g_improvedSignalWeight
                  << " improvedLoadWeight=" << g_improvedLoadWeight
                  << " pingPongWindow=" << g_pingPongWindowSeconds << "s";
        if (g_handoverMode == HandoverMode::IMPROVED)
        {
            std::cout << " improvedVisibilityWeight=" << g_improvedVisibilityWeight
                      << " improvedRsrqWeight=" << g_improvedRsrqWeight
                      << " improvedMinVisibility=" << g_improvedMinVisibilitySeconds << "s"
                      << " improvedVisibilityHorizon=" << g_improvedVisibilityHorizonSeconds
                      << "s"
                      << " improvedMinLoadScoreDelta=" << g_improvedMinLoadScoreDelta
                      << " improvedMaxSignalGapDb=" << g_improvedMaxSignalGapDb
                      << " improvedMinStableLead=" << g_improvedMinStableLeadTimeSeconds << "s"
                      << " improvedVisibilityStep=" << g_improvedVisibilityPredictionStepSeconds
                      << "s"
                      << " improvedMinJointScoreMargin=" << g_improvedMinJointScoreMargin
                      << " improvedMinCandidateRsrp=" << g_improvedMinCandidateRsrpDbm << "dBm"
                      << " improvedMinCandidateRsrq=" << g_improvedMinCandidateRsrqDb << "dB"
                      << " improvedServingWeakRsrp=" << g_improvedServingWeakRsrpDbm << "dBm"
                      << " improvedServingWeakRsrq=" << g_improvedServingWeakRsrqDb << "dB"
                      << " crossLayerPhyAssist="
                      << (g_improvedEnableCrossLayerPhyAssist ? "ON" : "OFF");
            if (g_improvedEnableCrossLayerPhyAssist)
            {
                std::cout << " crossLayerPhyAlpha=" << g_improvedCrossLayerPhyAlpha
                          << " crossLayerTblerTh=" << g_improvedCrossLayerTblerThreshold
                          << " crossLayerSinrTh=" << g_improvedCrossLayerSinrThresholdDb << "dB"
                          << " crossLayerMinSamples=" << g_improvedCrossLayerMinSamples;
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
    channelHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(cfg.shadowingEnabled));

    CcBwpCreator ccBwpCreator;
    CcBwpCreator::SimpleOperationBandConf bandConf(cfg.centralFrequency, cfg.bandwidth, 1);
    OperationBandInfo band = ccBwpCreator.CreateOperationBandContiguousCc(bandConf);
    std::vector<std::reference_wrapper<OperationBandInfo>> bandRefs{std::ref(band)};
    channelHelper->AssignChannelsToBands(bandRefs);
    BandwidthPartInfoPtrVector allBwps = CcBwpCreator::GetAllBwps({band});

    nrHelper->SetGnbPhyAttribute("TxPower", DoubleValue(cfg.gnbTxPower));
    nrHelper->SetUePhyAttribute("TxPower", DoubleValue(cfg.ueTxPower));

    nrHelper->SetSchedulerTypeId(TypeId::LookupByName("ns3::NrMacSchedulerOfdmaRR"));
    nrHelper->SetSchedulerAttribute("EnableSrsInFSlots", BooleanValue(cfg.enableSrsInFSlots));
    nrHelper->SetSchedulerAttribute("EnableSrsInUlSlots", BooleanValue(cfg.enableSrsInUlSlots));
    nrHelper->SetSchedulerAttribute("SrsSymbols", UintegerValue(effectiveSrsSymbols));
    nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(cfg.ueAntennaRows));
    nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(cfg.ueAntennaColumns));
    nrHelper->SetUeAntennaAttribute("AntennaElement",
                                    PointerValue(CreateAntennaElement(ueAntennaElementMode, cfg)));
    nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(cfg.gnbAntennaRows));
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(cfg.gnbAntennaColumns));
    nrHelper->SetGnbAntennaAttribute("AntennaElement",
                                     PointerValue(CreateAntennaElement(gnbAntennaElementMode, cfg)));

    EarthFixedGnbBeamforming::SetAnchorResolver(&ResolveEarthFixedGnbAnchorPosition);
    g_phyDlTbIntervals.clear();
    Ptr<BeamformingHelperBase> beamformingHelper;
    if (useRealisticBeamforming)
    {
        Ptr<RealisticBeamformingHelper> realisticBeamformingHelper =
            CreateObject<RealisticBeamformingHelper>();
        realisticBeamformingHelper->SetBeamformingMethod(
            RealisticBeamformingAlgorithm::GetTypeId());
        nrHelper->SetGnbBeamManagerTypeId(RealisticBfManager::GetTypeId());
        nrHelper->SetGnbBeamManagerAttribute("TriggerEvent",
                                             EnumValue(realisticBfTriggerEvent));
        nrHelper->SetGnbBeamManagerAttribute("UpdatePeriodicity",
                                             UintegerValue(cfg.realisticBfUpdatePeriodicity));
        nrHelper->SetGnbBeamManagerAttribute("UpdateDelay",
                                             TimeValue(MilliSeconds(cfg.realisticBfUpdateDelayMs)));
        beamformingHelper = realisticBeamformingHelper;
    }
    else
    {
        Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject<IdealBeamformingHelper>();
        idealBeamformingHelper->SetAttribute(
            "BeamformingMethod",
            TypeIdValue(GetIdealBeamformingMethodTypeId(beamformingMode)));
        idealBeamformingHelper->SetAttribute(
            "BeamformingPeriodicity",
            TimeValue(MilliSeconds(cfg.beamformingPeriodicityMs)));
        beamformingHelper = idealBeamformingHelper;
    }
    nrHelper->SetBeamformingHelper(beamformingHelper);

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
            const auto budget = CalculateEarthFixedBeamBudget(
                state.ecef,
                ue.groundPoint.ecef,
                ResolveSatelliteBeamTargetEcef(satIdx, state.ecef),
                g_beamModelConfig);

            const bool accessAllowed =
                !g_enforceBeamCoverageForRealLinks ||
                (budget.beamLocked &&
                 budget.offBoresightAngleRad <= g_beamModelConfig.theta3dBRad &&
                 IsUeInSatelliteAnchorCell(satIdx, ue));
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
            if (state.visible && accessAllowed && budget.rsrpDbm > bestEligibleRsrp)
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
            if (cfg.startupVerbose)
            {
                std::cout << "[Setup] warning: ue" << ueIdx
                          << " visible satellites exist but none satisfy real-link access gate at t=0, "
                          << "fallback to highest-elevation visible sat" << initialAttachIdx
                          << std::endl;
            }
        }
        else
        {
            initialAttachIdx = bestAnyIdx;
            if (cfg.startupVerbose)
            {
                std::cout << "[Setup] warning: ue" << ueIdx
                          << " no visible satellite at t=0, attaching to "
                          << "highest-elevation sat" << initialAttachIdx
                          << " (el=" << LeoOrbitCalculator::RadToDeg(bestAnyElevation) << "deg)"
                          << std::endl;
            }
        }

        ue.initialAttachIdx = initialAttachIdx;

        nrHelper->AttachToGnb(ueNetDev.Get(ueIdx), g_satelliteGnbDevices[initialAttachIdx]);

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

        ue.dlPort = dlPort;
        ue.server = serverApps.Get(0)->GetObject<UdpServer>();
        if (ue.dev)
        {
            g_imsiToUe[ue.dev->GetImsi()] = ueIdx;
        }
        g_ues.push_back(ue);
    }

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
    if (g_satAnchorTrace.is_open())
    {
        g_satAnchorTrace.close();
    }
    if (g_satGroundTrackTrace.is_open())
    {
        g_satGroundTrackTrace.close();
    }
    if (g_handoverThroughputTrace.is_open())
    {
        g_handoverThroughputTrace.close();
    }
    if (g_handoverEventTrace.is_open())
    {
        g_handoverEventTrace.close();
    }
    if (g_phyDlTbTrace.is_open())
    {
        g_phyDlTbTrace.close();
    }
    g_satAnchorTrace.open(cfg.satAnchorTracePath, std::ios::out | std::ios::trunc);
    NS_ABORT_MSG_IF(!g_satAnchorTrace.is_open(),
                    "Failed to open satellite anchor trace CSV: " << cfg.satAnchorTracePath);
    g_satAnchorTrace
        << "time_s,sat,plane,slot,cell,anchor_grid_id,anchor_latitude_deg,anchor_longitude_deg,"
        << "anchor_east_m,anchor_north_m\n";
    g_satGroundTrackTrace.open(cfg.satGroundTrackTracePath, std::ios::out | std::ios::trunc);
    NS_ABORT_MSG_IF(!g_satGroundTrackTrace.is_open(),
                    "Failed to open satellite ground-track CSV: "
                        << cfg.satGroundTrackTracePath);
    g_satGroundTrackTrace
        << "time_s,sat,plane,slot,cell,subpoint_latitude_deg,subpoint_longitude_deg,"
        << "subpoint_east_m,subpoint_north_m,sat_ecef_x,sat_ecef_y,sat_ecef_z\n";
    g_handoverEventTrace.open(cfg.handoverEventTracePath, std::ios::out | std::ios::trunc);
    NS_ABORT_MSG_IF(!g_handoverEventTrace.is_open(),
                    "Failed to open handover event trace CSV: " << cfg.handoverEventTracePath);
    g_handoverEventTrace
        << "time_s,ue,ho_id,event,source_cell,target_cell,source_sat,target_sat,delay_ms,"
        << "ping_pong_detected,failure_reason\n";
    g_handoverEventTrace.flush();
    if (cfg.enablePhyDlTbTrace)
    {
        g_phyDlTbTrace.open(cfg.phyDlTbTracePath, std::ios::out | std::ios::trunc);
        NS_ABORT_MSG_IF(!g_phyDlTbTrace.is_open(),
                        "Failed to open PHY DL TB trace CSV: " << cfg.phyDlTbTracePath);
        g_phyDlTbTrace
            << "time_s,ue,cell,serving_sat,rnti,bwp,frame,subframe,slot,sym_start,num_sym,"
            << "tb_size,mcs,rank,rv,rb_assigned,cqi,sinr_db,min_sinr_db,tbler,corrupt\n";
    }
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
    }

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
                      << " handoverMode=" << ToString(g_handoverMode) << std::endl;
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

    if (g_satAnchorTrace.is_open())
    {
        g_satAnchorTrace.close();
    }
    if (g_satGroundTrackTrace.is_open())
    {
        g_satGroundTrackTrace.close();
    }
    if (g_handoverThroughputTrace.is_open())
    {
        g_handoverThroughputTrace.close();
    }
    if (g_handoverEventTrace.is_open())
    {
        g_handoverEventTrace.close();
    }
    if (g_phyDlTbTrace.is_open())
    {
        g_phyDlTbTrace.close();
    }

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
