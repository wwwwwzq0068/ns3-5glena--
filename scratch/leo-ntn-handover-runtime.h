#ifndef MYFIRST_RUNTIME_H
#define MYFIRST_RUNTIME_H

/*
 * 文件说明：
 * `leo-ntn-handover-runtime.h` 负责保存 `leo-ntn-handover-baseline.cc`
 * 在仿真运行期间需要维护的核心状态。
 *
 * 这里放的是“运行时上下文”：
 * - 卫星侧对象与邻区状态；
 * - UE 侧对象、预测切换状态和统计计数；
 * - 若干与运行时状态直接相关的辅助函数。
 *
 * 不放在本文件中的内容：
 * - 纯数学/几何工具函数，仍放在 `leo-ntn-handover-utils.h`；
 * - 最终结果汇总与控制台输出，放在 `leo-ntn-handover-reporting.h`。
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/nr-module.h"
#include "leo-orbit-calculator.h"
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ns3
{

struct SatelliteRuntime
{
    /** 卫星节点对象。 */
    Ptr<Node> node;

    /** 卫星侧 gNB 设备。 */
    Ptr<NrGnbNetDevice> dev;

    /** gNB RRC，用于切换与邻区管理。 */
    Ptr<NrGnbRrc> rrc;

    /** 当前卫星对应的轨道参数。 */
    LeoOrbitCalculator::KeplerElements orbit;

    /** 当前卫星所属的轨道面编号。 */
    uint32_t orbitPlaneIndex = 0;

    /** 当前卫星在所属轨道面内的序号。 */
    uint32_t orbitSlotIndex = 0;

    /** 当前卫星波束所指向的小区锚点 ECEF 坐标。 */
    Vector cellAnchorEcef;

    /** 当前选中的六边形网格锚点编号。 */
    uint32_t currentAnchorGridId = 0;

    /** 与当前星下点最近的 K 个网格编号。 */
    std::vector<uint32_t> nearestGridIds;

    /** 当前被加入邻区列表的小区 ID 集合。 */
    std::set<uint16_t> activeNeighbours;

    /** `attachedUeCount`（已接入 UE 数）：当前接入到该卫星小区的 UE 数。 */
    uint32_t attachedUeCount = 0;

    /** `offeredPacketRate`（汇总业务到达率）：当前汇总到该卫星的业务到达率，单位 pkt/s。 */
    double offeredPacketRate = 0.0;

    /** `loadScore`（负载评分）：归一化负载分数，数值越大表示越忙。 */
    double loadScore = 0.0;

    /** `admissionAllowed`（是否允许接纳切入）：当前是否允许继续接纳新的切入 UE。 */
    bool admissionAllowed = true;
};

struct UeRuntime
{
    /** UE 节点对象。 */
    Ptr<Node> node;

    /** UE 侧 NR 设备。 */
    Ptr<NrUeNetDevice> dev;

    /** 用于统计下行接收包数的 UDP Server。 */
    Ptr<UdpServer> server;

    /** UE 所在地面点。 */
    LeoOrbitCalculator::GroundPoint groundPoint;

    /** UE 在场景中的部署角色，例如线性、中心簇或外围簇。 */
    std::string placementRole = "line";

    /** UE 相对参考中心的东西向偏移，单位米。 */
    double eastOffsetMeters = 0.0;

    /** UE 相对参考中心的南北向偏移，单位米。 */
    double northOffsetMeters = 0.0;

    /** 初始接入卫星的索引。 */
    uint32_t initialAttachIdx = std::numeric_limits<uint32_t>::max();

    /** 预测切换中的源卫星索引。 */
    uint32_t expectedSourceIndex = 0;

    /** 预测切换中的目标卫星索引。 */
    uint32_t expectedTargetIndex = 0;

    /** 是否已经生成了预测切换。 */
    bool hasPredictedHandover = false;

    /** 预测切换是否在真实仿真中被观察到。 */
    bool seenExpectedHandover = false;

    /** 预测切换触发时刻，单位秒。 */
    double predictedHandoverTimeSeconds = -1.0;

    /** 当前是否已经记录到尚未闭合的 HO-START。 */
    bool hasPendingHoStart = false;

    /** 最近一次切换开始时的源小区 ID。 */
    uint16_t lastHoStartSourceCell = 0;

    /** 最近一次切换开始时的目标小区 ID。 */
    uint16_t lastHoStartTargetCell = 0;

    /** 最近一次 HO-START 的时刻，单位秒。 */
    double lastHoStartTimeSeconds = -1.0;

    /** 上一次写入服务小区变化日志时的服务小区 ID。 */
    uint16_t lastServingCellForLog = 0;

    /** 上一次 KPI 日志输出时刻，单位秒。 */
    double lastKpiReportTime = -1.0;

    /** 上一次吞吐统计时看到的累计收包数。 */
    uint64_t lastRxPackets = 0;

    /** 已观察到的切换开始次数。 */
    uint32_t handoverStartCount = 0;

    /** 已观察到的切换成功次数。 */
    uint32_t handoverEndOkCount = 0;

    /** 成功切换执行时延累加值，单位秒。 */
    double totalHandoverExecutionDelaySeconds = 0.0;

    /** 已识别到的短时回切（A->B->A）次数。 */
    uint32_t pingPongCount = 0;

    /** 最近一次成功切换的源小区 ID。 */
    uint16_t lastSuccessfulHoSourceCell = 0;

    /** 最近一次成功切换的目标小区 ID。 */
    uint16_t lastSuccessfulHoTargetCell = 0;

    /** 最近一次成功切换完成时刻，单位秒。 */
    double lastSuccessfulHoTimeSeconds = -1.0;

    /** 自定义切换逻辑当前认为的服务卫星索引。 */
    uint32_t manualHoServingIdx = std::numeric_limits<uint32_t>::max();

    /** 自定义切换逻辑当前追踪的候选目标卫星索引。 */
    uint32_t manualHoCandidateIdx = std::numeric_limits<uint32_t>::max();

    /** 当前目标卫星开始持续优于服务卫星的起始时刻。 */
    double manualHoCandidateSince = -1.0;

    /** 上一次主动触发自定义切换的时刻。 */
    double manualHoLastTriggerTime = -1.0;

    /** 上一拍记录的各卫星距离，用于判定最近卫星变化。 */
    std::vector<double> prevDistances;

    /** 每条 UE-卫星观测链当前持有的阴影衰落样本，单位 dB。 */
    std::vector<double> customA3ShadowingDb;

    /** 每条 UE-卫星观测链当前持有的莱斯功率增益样本，线性值。 */
    std::vector<double> customA3RicianPowerLinear;

    /** 上一次更新自定义 A3 扰动的时刻，单位秒。 */
    double customA3LastUpdateTimeSeconds = -1.0;
};

struct UeLayoutConfig
{
    /** UE 部署类型：`line` 或 `seven-cell`。 */
    std::string layoutType = "seven-cell";

    /** 线性部署时相邻 UE 的东西向间距。 */
    double lineSpacingMeters = 0.0;

    /** 中心小区与六个外围小区共用的 hex 半径。 */
    double hexCellRadiusMeters = 20000.0;

    /** 中心小区内 3x3 密集簇的格点间距。 */
    double centerSpacingMeters = 6000.0;

    /** 外围 6 个小区内局部散点相对各自小区中心的偏移尺度。 */
    double ringPointOffsetMeters = 5000.0;
};

struct UePlacement
{
    /** 生成后的地面点。 */
    LeoOrbitCalculator::GroundPoint groundPoint;

    /** 部署角色。 */
    std::string role = "line";

    /** 相对参考中心的东西向偏移，单位米。 */
    double eastOffsetMeters = 0.0;

    /** 相对参考中心的南北向偏移，单位米。 */
    double northOffsetMeters = 0.0;
};

/**
 * 将单个 UE 的运行时状态重置到新的仿真起点。
 */
inline void
ResetUeRuntime(UeRuntime& ue, uint32_t gNbNum)
{
    ue.lastServingCellForLog = 0;
    ue.lastKpiReportTime = -1.0;
    ue.lastRxPackets = 0;
    ue.seenExpectedHandover = false;
    ue.hasPendingHoStart = false;
    ue.lastHoStartSourceCell = 0;
    ue.lastHoStartTargetCell = 0;
    ue.lastHoStartTimeSeconds = -1.0;
    ue.handoverStartCount = 0;
    ue.handoverEndOkCount = 0;
    ue.totalHandoverExecutionDelaySeconds = 0.0;
    ue.pingPongCount = 0;
    ue.lastSuccessfulHoSourceCell = 0;
    ue.lastSuccessfulHoTargetCell = 0;
    ue.lastSuccessfulHoTimeSeconds = -1.0;
    ue.manualHoServingIdx = std::numeric_limits<uint32_t>::max();
    ue.manualHoCandidateIdx = std::numeric_limits<uint32_t>::max();
    ue.manualHoCandidateSince = -1.0;
    ue.manualHoLastTriggerTime = -1.0;
    ue.prevDistances.assign(gNbNum, -1.0);
    ue.customA3ShadowingDb.assign(gNbNum, std::numeric_limits<double>::quiet_NaN());
    ue.customA3RicianPowerLinear.assign(gNbNum, std::numeric_limits<double>::quiet_NaN());
    ue.customA3LastUpdateTimeSeconds = -1.0;
}

/**
 * 根据 IMSI 反查 UE 序号。
 *
 * 说明：
 * 真实 IMSI 仍由底层协议栈维护，这里只做日志和统计用途的映射。
 */
inline std::optional<uint32_t>
ResolveUeIndexFromImsi(const std::map<uint64_t, uint32_t>& imsiToUe, uint64_t imsi)
{
    const auto it = imsiToUe.find(imsi);
    if (it == imsiToUe.end())
    {
        return std::nullopt;
    }
    return it->second;
}

/**
 * 单个 UE 在局部平面中的偏移描述。
 */
struct UePlacementOffsetSpec
{
    double eastOffsetMeters = 0.0;
    double northOffsetMeters = 0.0;
    std::string role = "line";
};

inline void
AppendUeOffsetSpec(std::vector<UePlacementOffsetSpec>& specs,
                   double eastOffsetMeters,
                   double northOffsetMeters,
                   const std::string& role)
{
    UePlacementOffsetSpec spec;
    spec.eastOffsetMeters = eastOffsetMeters;
    spec.northOffsetMeters = northOffsetMeters;
    spec.role = role;
    specs.push_back(spec);
}

inline void
AppendRectGridOffsetSpecs(std::vector<UePlacementOffsetSpec>& specs,
                          double centerEastMeters,
                          double centerNorthMeters,
                          int rowMin,
                          int rowMax,
                          int colMin,
                          int colMax,
                          double spacingMeters,
                          const std::string& role)
{
    for (int row = rowMin; row <= rowMax; ++row)
    {
        for (int col = colMin; col <= colMax; ++col)
        {
            AppendUeOffsetSpec(specs,
                               centerEastMeters + static_cast<double>(col) * spacingMeters,
                               centerNorthMeters + static_cast<double>(row) * spacingMeters,
                               role);
        }
    }
}

inline std::vector<UePlacementOffsetSpec>
BuildLineUeOffsetSpecs(uint32_t ueNum, double ueSpacingMeters)
{
    std::vector<UePlacementOffsetSpec> specs;
    specs.reserve(ueNum);
    const double center = (static_cast<double>(ueNum) - 1.0) / 2.0;
    for (uint32_t i = 0; i < ueNum; ++i)
    {
        AppendUeOffsetSpec(specs, (static_cast<double>(i) - center) * ueSpacingMeters, 0.0, "line");
    }
    return specs;
}

inline std::vector<UePlacementOffsetSpec>
BuildSevenCellUeOffsetSpecs(uint32_t ueNum, const UeLayoutConfig& layout)
{
    std::vector<UePlacementOffsetSpec> specs;
    NS_ABORT_MSG_IF(ueNum != 25, "seven-cell layout currently requires ueNum == 25");
    specs.reserve(ueNum);

    AppendRectGridOffsetSpecs(
        specs, 0.0, 0.0, -1, 1, -1, 1, layout.centerSpacingMeters, "center");

    const double dx = std::sqrt(3.0) * layout.hexCellRadiusMeters;
    const double dy = 1.5 * layout.hexCellRadiusMeters;
    const std::vector<std::pair<double, double>> ringCellCenters = {
        {-dx, 0.0},
        {-0.5 * dx, dy},
        {0.5 * dx, dy},
        {dx, 0.0},
        {0.5 * dx, -dy},
        {-0.5 * dx, -dy},
    };
    const std::vector<uint32_t> ringUeCounts = {3, 3, 3, 3, 2, 2};

    for (uint32_t cellIdx = 0; cellIdx < ringCellCenters.size(); ++cellIdx)
    {
        const auto& [centerEastMeters, centerNorthMeters] = ringCellCenters[cellIdx];
        const uint32_t count = ringUeCounts[cellIdx];
        const double norm = std::hypot(centerEastMeters, centerNorthMeters);
        const double dirEast = centerEastMeters / norm;
        const double dirNorth = centerNorthMeters / norm;
        const double tangentEast = -dirNorth;
        const double tangentNorth = dirEast;

        if (count == 3)
        {
            AppendUeOffsetSpec(specs,
                               centerEastMeters - layout.ringPointOffsetMeters * dirEast,
                               centerNorthMeters - layout.ringPointOffsetMeters * dirNorth,
                               "ring");
            AppendUeOffsetSpec(specs,
                               centerEastMeters + layout.ringPointOffsetMeters * tangentEast,
                               centerNorthMeters + layout.ringPointOffsetMeters * tangentNorth,
                               "ring");
            AppendUeOffsetSpec(specs,
                               centerEastMeters - layout.ringPointOffsetMeters * tangentEast,
                               centerNorthMeters - layout.ringPointOffsetMeters * tangentNorth,
                               "ring");
        }
        else
        {
            const double tangentialOffsetMeters = 0.75 * layout.ringPointOffsetMeters;
            AppendUeOffsetSpec(specs,
                               centerEastMeters + tangentialOffsetMeters * tangentEast,
                               centerNorthMeters + tangentialOffsetMeters * tangentNorth,
                               "ring");
            AppendUeOffsetSpec(specs,
                               centerEastMeters - tangentialOffsetMeters * tangentEast,
                               centerNorthMeters - tangentialOffsetMeters * tangentNorth,
                               "ring");
        }
    }

    return specs;
}

inline std::vector<UePlacement>
BuildUePlacementsFromOffsetSpecs(double baseLatitudeDeg,
                                 double baseLongitudeDeg,
                                 double altitudeMeters,
                                 const std::vector<UePlacementOffsetSpec>& offsetSpecs)
{
    std::vector<UePlacement> placements;
    placements.reserve(offsetSpecs.size());

    const double earthRadiusMeters = LeoOrbitCalculator::kWgs84SemiMajorAxisMeters;
    const double baseLatRad = LeoOrbitCalculator::DegToRad(baseLatitudeDeg);
    const double lonScale = std::max(0.1, std::cos(baseLatRad));
    for (const auto& spec : offsetSpecs)
    {
        const double deltaLatRad = spec.northOffsetMeters / earthRadiusMeters;
        const double deltaLonRad = spec.eastOffsetMeters / (earthRadiusMeters * lonScale);

        UePlacement placement;
        placement.groundPoint =
            LeoOrbitCalculator::CreateGroundPoint(baseLatitudeDeg + LeoOrbitCalculator::RadToDeg(deltaLatRad),
                                                  baseLongitudeDeg + LeoOrbitCalculator::RadToDeg(deltaLonRad),
                                                  altitudeMeters);
        placement.role = spec.role;
        placement.eastOffsetMeters = spec.eastOffsetMeters;
        placement.northOffsetMeters = spec.northOffsetMeters;
        placements.push_back(placement);
    }

    return placements;
}

inline std::vector<UePlacement>
BuildUePlacements(double baseLatitudeDeg,
                  double baseLongitudeDeg,
                  double altitudeMeters,
                  uint32_t ueNum,
                  const UeLayoutConfig& layout)
{
    std::vector<UePlacementOffsetSpec> offsetSpecs;
    if (layout.layoutType == "seven-cell")
    {
        offsetSpecs = BuildSevenCellUeOffsetSpecs(ueNum, layout);
    }
    else
    {
        offsetSpecs = BuildLineUeOffsetSpecs(ueNum, layout.lineSpacingMeters);
    }

    return BuildUePlacementsFromOffsetSpecs(
        baseLatitudeDeg, baseLongitudeDeg, altitudeMeters, offsetSpecs);
}

inline void
DumpUeLayoutCsv(const std::string& path, const std::vector<UePlacement>& placements)
{
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    NS_ABORT_MSG_IF(!out.is_open(), "Failed to open UE layout CSV: " << path);

    out << "ue_id,role,east_m,north_m,latitude_deg,longitude_deg,altitude_m\n";
    for (uint32_t ueIdx = 0; ueIdx < placements.size(); ++ueIdx)
    {
        const auto& placement = placements[ueIdx];
        out << ueIdx << "," << placement.role << ",";
        out << std::fixed << std::setprecision(3) << placement.eastOffsetMeters << ","
            << placement.northOffsetMeters << ",";
        out << std::setprecision(8)
            << LeoOrbitCalculator::RadToDeg(placement.groundPoint.latitudeRad) << ","
            << LeoOrbitCalculator::RadToDeg(placement.groundPoint.longitudeRad) << ",";
        out << std::setprecision(3) << placement.groundPoint.altitudeMeters << "\n";
    }
}

} // namespace ns3

#endif // MYFIRST_RUNTIME_H
