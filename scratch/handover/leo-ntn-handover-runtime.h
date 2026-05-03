#ifndef MYFIRST_RUNTIME_H
#define MYFIRST_RUNTIME_H

/*
 * 文件说明：
 * `leo-ntn-handover-runtime.h` 负责保存 `leo-ntn-handover-baseline.cc`
 * 在仿真运行期间需要维护的核心状态。
 *
 * 这里放的是“运行时上下文”：
 * - 卫星侧对象与邻区状态；
 * - UE 侧对象和切换/业务连续性统计；
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
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ns3
{

enum class HandoverFailureReason
{
    NONE,
    UNKNOWN,
    NO_PREAMBLE,
    MAX_RACH,
    LEAVING_TIMEOUT,
    JOINING_TIMEOUT,
};

inline const char*
ToString(HandoverFailureReason reason)
{
    switch (reason)
    {
    case HandoverFailureReason::NONE:
        return "";
    case HandoverFailureReason::UNKNOWN:
        return "unknown";
    case HandoverFailureReason::NO_PREAMBLE:
        return "no_preamble";
    case HandoverFailureReason::MAX_RACH:
        return "max_rach";
    case HandoverFailureReason::LEAVING_TIMEOUT:
        return "leaving_timeout";
    case HandoverFailureReason::JOINING_TIMEOUT:
        return "joining_timeout";
    }
    return "unknown";
}

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

    /** 正在等待门控确认的新锚点编号。 */
    uint32_t pendingAnchorGridId = 0;

    /** 正在等待门控确认的新锚点位置。 */
    Vector pendingAnchorEcef;

    /** 当前候选锚点首次取得领先的时刻，单位秒。 */
    double pendingAnchorLeadStartTimeSeconds = -1.0;

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

    /** 静态 UE 地面点最近的 hex 小区编号；0 表示当前未缓存。 */
    uint32_t homeGridId = 0;

    /** 当前 UE 下行业务流使用的 UDP 目标端口。 */
    uint16_t dlPort = 0;

    /** UE 在场景中的部署角色，例如线性、中心簇或外围簇。 */
    std::string placementRole = "line";

    /** UE 相对参考中心的东西向偏移，单位米。 */
    double eastOffsetMeters = 0.0;

    /** UE 相对参考中心的南北向偏移，单位米。 */
    double northOffsetMeters = 0.0;

    /** 初始接入卫星的索引。 */
    uint32_t initialAttachIdx = std::numeric_limits<uint32_t>::max();

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

    /** 切换窗口吞吐 trace 上一次采样时看到的累计收包数。 */
    uint64_t lastThroughputTraceRxPackets = 0;

    /** 已观察到的切换开始次数。 */
    uint32_t handoverStartCount = 0;

    /** 已观察到的切换成功次数。 */
    uint32_t handoverEndOkCount = 0;

    /** 已观察到的切换失败次数。 */
    uint32_t handoverEndErrorCount = 0;

    /** 未能分到 non-contention preamble 的失败次数。 */
    uint32_t handoverFailureNoPreambleCount = 0;

    /** 由于达到最大 RACH 尝试次数导致的失败次数。 */
    uint32_t handoverFailureMaxRachCount = 0;

    /** 由于 source 侧 leaving timeout 导致的失败次数。 */
    uint32_t handoverFailureLeavingCount = 0;

    /** 由于 target 侧 joining timeout 导致的失败次数。 */
    uint32_t handoverFailureJoiningCount = 0;

    /** 未拿到更具体 gNB 侧原因的失败次数。 */
    uint32_t handoverFailureUnknownCount = 0;

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

    /** 当前连续领先门控跟踪的源小区 ID。 */
    uint16_t stableLeadSourceCell = 0;

    /** 当前连续领先门控跟踪的目标小区 ID。 */
    uint16_t stableLeadTargetCell = 0;

    /** 当前连续领先门控开始计时的时刻，单位秒。 */
    double stableLeadSinceSeconds = -1.0;

    /** 当前 UE 已分配到的切换事件序号，用于关联事件和吞吐 trace。 */
    uint32_t handoverTraceSequence = 0;

    /** 当前尚未闭合的切换事件序号；没有 pending handover 时为 0。 */
    uint32_t activeHandoverTraceId = 0;

    /** 当前 pending 切换已捕获到的失败原因。 */
    HandoverFailureReason pendingFailureReason = HandoverFailureReason::NONE;

    /** 最近 PHY 下行样本数，用于跨层门控预热。 */
    uint32_t recentPhySampleCount = 0;

    /** 最近 PHY 下行损坏块率的 EWMA。 */
    double recentPhyCorruptRateEwma = 0.0;

    /** 最近 PHY 下行 TBler 的 EWMA。 */
    double recentPhyTblerEwma = 0.0;

    /** 最近 PHY 下行 SINR(dB) 的 EWMA。 */
    double recentPhySinrDbEwma = std::numeric_limits<double>::quiet_NaN();

    // ===== Interference-trap handover diagnosis fields =====

    /** PHY state snapshot at HO_START time (TBler EWMA). */
    double hoStartPhyTblerEwma = std::numeric_limits<double>::quiet_NaN();

    /** PHY state snapshot at HO_START time (SINR EWMA in dB). */
    double hoStartPhySinrDbEwma = std::numeric_limits<double>::quiet_NaN();

    /** PHY state snapshot at HO_START time (corrupt rate EWMA). */
    double hoStartPhyCorruptRateEwma = std::numeric_limits<double>::quiet_NaN();

    /** PHY state snapshot at HO_END_OK time (TBler EWMA, after settling window). */
    double hoEndOkPhyTblerEwma = std::numeric_limits<double>::quiet_NaN();

    /** PHY state snapshot at HO_END_OK time (SINR EWMA in dB). */
    double hoEndOkPhySinrDbEwma = std::numeric_limits<double>::quiet_NaN();

    /** Count of detected interference-trap handovers (target PHY worse than source). */
    uint32_t interferenceTrapHoCount = 0;

    /** Time when last HO_END_OK occurred, for post-HO monitoring window. */
    double lastHoEndOkTimeSeconds = -1.0;

};

struct UeLayoutConfig
{
    /** UE 部署类型；当前主线只支持 `poisson-3ring`。 */
    std::string layoutType = "poisson-3ring";

    /** 中心小区与六个外围小区共用的 hex 半径。 */
    double hexCellRadiusMeters = 20000.0;

    /** poisson-3ring 布局中每个 hex cell 的泊松均值。 */
    double poissonLambda = 1.5;

    /** poisson-3ring 布局中单个 hex cell 的 UE 数上限。 */
    uint32_t maxUePerCell = 5;

    /** poisson-3ring 布局的确定性随机种子。 */
    uint32_t randomSeed = 42;
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
    ue.lastThroughputTraceRxPackets = 0;
    ue.hasPendingHoStart = false;
    ue.lastHoStartSourceCell = 0;
    ue.lastHoStartTargetCell = 0;
    ue.lastHoStartTimeSeconds = -1.0;
    ue.handoverStartCount = 0;
    ue.handoverEndOkCount = 0;
    ue.handoverEndErrorCount = 0;
    ue.handoverFailureNoPreambleCount = 0;
    ue.handoverFailureMaxRachCount = 0;
    ue.handoverFailureLeavingCount = 0;
    ue.handoverFailureJoiningCount = 0;
    ue.handoverFailureUnknownCount = 0;
    ue.totalHandoverExecutionDelaySeconds = 0.0;
    ue.pingPongCount = 0;
    ue.lastSuccessfulHoSourceCell = 0;
    ue.lastSuccessfulHoTargetCell = 0;
    ue.lastSuccessfulHoTimeSeconds = -1.0;
    ue.stableLeadSourceCell = 0;
    ue.stableLeadTargetCell = 0;
    ue.stableLeadSinceSeconds = -1.0;
    ue.handoverTraceSequence = 0;
    ue.activeHandoverTraceId = 0;
    ue.pendingFailureReason = HandoverFailureReason::NONE;
    ue.recentPhySampleCount = 0;
    ue.recentPhyCorruptRateEwma = 0.0;
    ue.recentPhyTblerEwma = 0.0;
    ue.recentPhySinrDbEwma = std::numeric_limits<double>::quiet_NaN();
    // Interference-trap diagnosis reset
    ue.hoStartPhyTblerEwma = std::numeric_limits<double>::quiet_NaN();
    ue.hoStartPhySinrDbEwma = std::numeric_limits<double>::quiet_NaN();
    ue.hoStartPhyCorruptRateEwma = std::numeric_limits<double>::quiet_NaN();
    ue.hoEndOkPhyTblerEwma = std::numeric_limits<double>::quiet_NaN();
    ue.hoEndOkPhySinrDbEwma = std::numeric_limits<double>::quiet_NaN();
    ue.interferenceTrapHoCount = 0;
    ue.lastHoEndOkTimeSeconds = -1.0;
}

inline void
ClearStableLeadTracking(UeRuntime& ue)
{
    ue.stableLeadSourceCell = 0;
    ue.stableLeadTargetCell = 0;
    ue.stableLeadSinceSeconds = -1.0;
}

inline void
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

inline void
ApplyDlPhyTbSampleToUe(UeRuntime& ue,
                       bool corrupt,
                       double tbler,
                       double sinr,
                       double alpha)
{
    const double boundedAlpha = std::clamp(alpha, 1e-6, 1.0);
    const double corruptIndicator = corrupt ? 1.0 : 0.0;
    if (ue.recentPhySampleCount == 0)
    {
        ue.recentPhyCorruptRateEwma = corruptIndicator;
        ue.recentPhyTblerEwma = tbler;
    }
    else
    {
        ue.recentPhyCorruptRateEwma =
            (1.0 - boundedAlpha) * ue.recentPhyCorruptRateEwma + boundedAlpha * corruptIndicator;
        ue.recentPhyTblerEwma = (1.0 - boundedAlpha) * ue.recentPhyTblerEwma + boundedAlpha * tbler;
    }
    ue.recentPhySampleCount++;

    if (sinr > 0.0)
    {
        const double sinrDb = 10.0 * std::log10(sinr);
        if (!std::isfinite(ue.recentPhySinrDbEwma) || ue.recentPhySampleCount <= 1)
        {
            ue.recentPhySinrDbEwma = sinrDb;
        }
        else
        {
            ue.recentPhySinrDbEwma =
                (1.0 - boundedAlpha) * ue.recentPhySinrDbEwma + boundedAlpha * sinrDb;
        }
    }
}

inline void
InstallStaticNeighbourRelations(std::vector<SatelliteRuntime>& satellites)
{
    for (uint32_t servingSatIdx = 0; servingSatIdx < satellites.size(); ++servingSatIdx)
    {
        auto& servingSat = satellites[servingSatIdx];
        servingSat.activeNeighbours.clear();
        for (uint32_t neighbourSatIdx = 0; neighbourSatIdx < satellites.size(); ++neighbourSatIdx)
        {
            if (neighbourSatIdx == servingSatIdx)
            {
                continue;
            }

            const uint16_t neighbourCellId = satellites[neighbourSatIdx].dev->GetCellId();
            servingSat.activeNeighbours.insert(neighbourCellId);
            servingSat.rrc->AddX2Neighbour(neighbourCellId);
        }
    }
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

struct HexOffsetCenter
{
    double eastMeters = 0.0;
    double northMeters = 0.0;
    uint32_t ringDistance = 0;
};

inline std::vector<HexOffsetCenter>
BuildTwoRingHexOffsetCenters(double hexRadiusMeters)
{
    std::vector<HexOffsetCenter> centers;
    centers.reserve(19);

    const double dx = std::sqrt(3.0) * hexRadiusMeters;
    const double dy = 1.5 * hexRadiusMeters;

    for (int ring = 0; ring <= 2; ++ring)
    {
        for (int r = -2; r <= 2; ++r)
        {
            for (int q = -2; q <= 2; ++q)
            {
                const int s = -q - r;
                const int ringDistance = std::max({std::abs(q), std::abs(r), std::abs(s)});
                if (ringDistance != ring)
                {
                    continue;
                }

                HexOffsetCenter center;
                center.eastMeters =
                    dx * (static_cast<double>(q) + 0.5 * static_cast<double>(r));
                center.northMeters = dy * static_cast<double>(r);
                center.ringDistance = static_cast<uint32_t>(ringDistance);
                centers.push_back(center);
            }
        }
    }

    return centers;
}

inline uint32_t
SampleTruncatedPoisson(double lambda, uint32_t maxUePerCell, std::mt19937& rng)
{
    std::poisson_distribution<uint32_t> distribution(lambda);
    return std::min(distribution(rng), maxUePerCell);
}

inline std::pair<double, double>
SamplePointInPointyHex(double hexRadiusMeters, std::mt19937& rng)
{
    const double halfWidth = 0.5 * std::sqrt(3.0) * hexRadiusMeters;
    std::uniform_real_distribution<double> eastDistribution(-halfWidth, halfWidth);
    std::uniform_real_distribution<double> northDistribution(-hexRadiusMeters, hexRadiusMeters);

    while (true)
    {
        const double east = eastDistribution(rng);
        const double north = northDistribution(rng);
        const double absEast = std::abs(east);
        const double absNorth = std::abs(north);

        const double maxEast =
            absNorth <= 0.5 * hexRadiusMeters
                ? halfWidth
                : std::sqrt(3.0) * (hexRadiusMeters - absNorth);
        if (absEast <= maxEast)
        {
            return {east, north};
        }
    }
}

inline std::string
PoissonThreeRingRole(uint32_t ringDistance)
{
    return ringDistance == 0 ? "p3-center" : ringDistance == 1 ? "p3-ring1" : "p3-ring2";
}

inline std::vector<uint32_t>
BuildPoissonThreeRingCellCounts(uint32_t ueNum, const UeLayoutConfig& layout, std::mt19937& rng)
{
    const uint32_t cellCount = 19;
    std::vector<uint32_t> counts(cellCount, 0);
    uint32_t total = 0;

    for (uint32_t cellIdx = 0; cellIdx < cellCount; ++cellIdx)
    {
        counts[cellIdx] =
            SampleTruncatedPoisson(layout.poissonLambda, layout.maxUePerCell, rng);
        total += counts[cellIdx];
    }

    std::uniform_int_distribution<uint32_t> cellDistribution(0, cellCount - 1);
    while (total < ueNum)
    {
        const uint32_t cellIdx = cellDistribution(rng);
        if (counts[cellIdx] >= layout.maxUePerCell)
        {
            continue;
        }
        ++counts[cellIdx];
        ++total;
    }

    while (total > ueNum)
    {
        const uint32_t cellIdx = cellDistribution(rng);
        if (counts[cellIdx] == 0)
        {
            continue;
        }
        --counts[cellIdx];
        --total;
    }

    return counts;
}

inline std::vector<UePlacementOffsetSpec>
BuildPoissonThreeRingUeOffsetSpecs(uint32_t ueNum, const UeLayoutConfig& layout)
{
    NS_ABORT_MSG_IF(layout.layoutType != "poisson-3ring",
                    "Only poisson-3ring UE layout is supported");
    NS_ABORT_MSG_IF(layout.poissonLambda <= 0.0, "poissonLambda must be > 0");
    NS_ABORT_MSG_IF(layout.maxUePerCell == 0, "maxUePerCell must be >= 1");
    NS_ABORT_MSG_IF(ueNum > 19 * layout.maxUePerCell,
                    "poisson-3ring requires ueNum <= 19 * maxUePerCell");

    std::mt19937 rng(layout.randomSeed);
    const auto hexCenters = BuildTwoRingHexOffsetCenters(layout.hexCellRadiusMeters);
    const auto cellCounts = BuildPoissonThreeRingCellCounts(ueNum, layout, rng);

    std::vector<UePlacementOffsetSpec> specs;
    specs.reserve(ueNum);
    for (uint32_t cellIdx = 0; cellIdx < hexCenters.size(); ++cellIdx)
    {
        const auto& center = hexCenters[cellIdx];
        for (uint32_t ueIdx = 0; ueIdx < cellCounts[cellIdx]; ++ueIdx)
        {
            const auto [localEastMeters, localNorthMeters] =
                SamplePointInPointyHex(layout.hexCellRadiusMeters, rng);
            AppendUeOffsetSpec(specs,
                               center.eastMeters + localEastMeters,
                               center.northMeters + localNorthMeters,
                               PoissonThreeRingRole(center.ringDistance));
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
    const std::vector<UePlacementOffsetSpec> offsetSpecs =
        BuildPoissonThreeRingUeOffsetSpecs(ueNum, layout);

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
