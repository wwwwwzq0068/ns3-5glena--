#ifndef LEO_NTN_HANDOVER_UPDATE_H
#define LEO_NTN_HANDOVER_UPDATE_H

/*
 * 文件说明：
 * `leo-ntn-handover-update.h` 现在只保留周期更新主循环仍会复用的最小辅助函数。
 *
 * 当前 handover 判决已经统一迁到标准 PHY/RRC `MeasurementReport` 路径，
 * 因此原先那套几何 `beam budget/custom A3` 观测与手工触发逻辑已从主链移除。
 */

#include "leo-ntn-handover-runtime.h"

#include <algorithm>
#include <vector>

namespace ns3
{

inline void
UpdateSatelliteLoadStats(std::vector<SatelliteRuntime>& satellites,
                         double offeredPacketRatePerUe,
                         double maxSupportedUesPerSatellite,
                         double loadCongestionThreshold)
{
    const double safeMaxSupportedUes = std::max(1.0, maxSupportedUesPerSatellite);
    for (auto& sat : satellites)
    {
        sat.offeredPacketRate = sat.attachedUeCount * offeredPacketRatePerUe;
        const double linearPressure = std::clamp(static_cast<double>(sat.attachedUeCount) / safeMaxSupportedUes,
                                                 0.0,
                                                 1.0);
        const double smoothPressure = static_cast<double>(sat.attachedUeCount) /
                                      (static_cast<double>(sat.attachedUeCount) + safeMaxSupportedUes);
        // 当前业务模型里 offeredPacketRate 仍与接入 UE 数正相关，因此这里采用“线性容量比 + 平滑饱和”
        // 的双分量负载分数，避免 3 UE 左右就过早打满，保留 2/3/4 UE 之间的区分度。
        sat.loadScore = std::clamp(0.55 * linearPressure + 0.45 * smoothPressure, 0.0, 1.0);
        sat.admissionAllowed = (sat.loadScore + 1e-9 < loadCongestionThreshold);
    }
}

} // namespace ns3

#endif // LEO_NTN_HANDOVER_UPDATE_H
