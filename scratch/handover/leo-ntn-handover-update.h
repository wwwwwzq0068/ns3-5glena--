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
        sat.loadScore = std::min(1.0, static_cast<double>(sat.attachedUeCount) / safeMaxSupportedUes);
        sat.admissionAllowed = (sat.loadScore + 1e-9 < loadCongestionThreshold);
    }
}

} // namespace ns3

#endif // LEO_NTN_HANDOVER_UPDATE_H
