#ifndef LEO_NTN_HANDOVER_UPDATE_H
#define LEO_NTN_HANDOVER_UPDATE_H

/*
 * 文件说明：
 * `leo-ntn-handover-update.h` 存放周期更新主循环可复用的辅助结构和函数。
 *
 * 设计目标：
 * - 把 `UpdateConstellation()` 中的观测计算、日志拼装和收口步骤拆开；
 * - 不改变 baseline 的切换判决语义，只减少主脚本中的冗长实现；
 * - 让主脚本更聚焦于调度顺序，而不是细节样板。
 */

#include "beam-link-budget.h"
#include "leo-ntn-handover-runtime.h"
#include "leo-ntn-handover-utils.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace ns3
{

struct BeamTraceRow
{
    double timeSeconds = 0.0;
    uint32_t ueIndex = 0;
    uint32_t satIndex = 0;
    uint16_t cellId = 0;
    bool beamLocked = false;
    double scanLossDb = 0.0;
    double patternLossDb = 0.0;
    double fsplDb = 0.0;
    double atmLossDb = 0.0;
    double rsrpDbm = 0.0;
};

struct UeObservationSnapshot
{
    std::vector<LeoOrbitCalculator::OrbitState> states;
    std::vector<BeamLinkBudget> beamBudgets;
    std::vector<double> distancesMeters;
    uint16_t servingCellId = 0;
    uint32_t servingSatIdx = std::numeric_limits<uint32_t>::max();
    uint32_t bestRsrpIdx = std::numeric_limits<uint32_t>::max();
    double bestRsrp = -std::numeric_limits<double>::infinity();
    uint32_t bestNeighbourIdx = std::numeric_limits<uint32_t>::max();
    double bestNeighbourRsrp = -std::numeric_limits<double>::infinity();
    bool servingUsable = false;
    bool neighbourQualified = false;
};

inline UeObservationSnapshot
BuildUeObservationSnapshot(const UeRuntime& ue,
                           uint32_t ueIdx,
                           double nowSeconds,
                           double carrierFrequencyHz,
                           double minElevationRad,
                           const BeamModelConfig& beamModelConfig,
                           const std::vector<LeoOrbitCalculator::OrbitState>& satelliteStates,
                           const std::vector<SatelliteRuntime>& satellites,
                           const std::map<uint16_t, uint32_t>& cellToSatellite,
                           std::vector<BeamTraceRow>* pendingBeamTraceRows = nullptr)
{
    UeObservationSnapshot out;
    out.states.resize(satellites.size());
    out.beamBudgets.resize(satellites.size());
    out.distancesMeters.assign(satellites.size(), 0.0);

    Vector ueEcef = ue.groundPoint.ecef;
    if (ue.node && ue.node->GetObject<MobilityModel>())
    {
        ueEcef = ue.node->GetObject<MobilityModel>()->GetPosition();
    }

    for (uint32_t satIdx = 0; satIdx < satellites.size(); ++satIdx)
    {
        out.states[satIdx] = LeoOrbitCalculator::CalculateObservation(satelliteStates[satIdx],
                                                                      ue.groundPoint,
                                                                      carrierFrequencyHz,
                                                                      minElevationRad);
        out.beamBudgets[satIdx] = CalculateEarthFixedBeamBudget(out.states[satIdx].ecef,
                                                                ueEcef,
                                                                satellites[satIdx].cellAnchorEcef,
                                                                beamModelConfig);
        out.distancesMeters[satIdx] = out.states[satIdx].slantRangeMeters;

        if (pendingBeamTraceRows != nullptr)
        {
            pendingBeamTraceRows->push_back({nowSeconds,
                                             ueIdx,
                                             satIdx,
                                             satellites[satIdx].dev->GetCellId(),
                                             out.beamBudgets[satIdx].beamLocked,
                                             out.beamBudgets[satIdx].scanLossDb,
                                             out.beamBudgets[satIdx].patternLossDb,
                                             out.beamBudgets[satIdx].fsplDb,
                                             beamModelConfig.atmLossDb,
                                             out.beamBudgets[satIdx].rsrpDbm});
        }
    }

    if (ue.dev && ue.dev->GetRrc())
    {
        out.servingCellId = ue.dev->GetRrc()->GetCellId();
        const auto servingIt = cellToSatellite.find(out.servingCellId);
        if (servingIt != cellToSatellite.end())
        {
            out.servingSatIdx = servingIt->second;
        }
    }

    out.bestRsrpIdx = FindBestRsrpSatellite(nullptr,
                                            out.beamBudgets,
                                            std::numeric_limits<uint32_t>::max(),
                                            false,
                                            true,
                                            &out.bestRsrp);

    if (out.servingSatIdx != std::numeric_limits<uint32_t>::max())
    {
        out.bestNeighbourIdx = FindBestRsrpSatellite(&out.states,
                                                     out.beamBudgets,
                                                     out.servingSatIdx,
                                                     true,
                                                     true,
                                                     &out.bestNeighbourRsrp);
        out.servingUsable = out.states[out.servingSatIdx].visible &&
                            out.beamBudgets[out.servingSatIdx].beamLocked &&
                            std::isfinite(out.beamBudgets[out.servingSatIdx].rsrpDbm);
    }

    return out;
}

inline void
PrintMobilitySnapshot(const UeRuntime& ue,
                      uint32_t ueIdx,
                      double nowSeconds,
                      const std::vector<SatelliteRuntime>& satellites,
                      const UeObservationSnapshot& observation,
                      const BeamModelConfig& beamModelConfig)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    oss << "[Mobility] t=" << nowSeconds << "s ue=" << ueIdx
        << " servingCell=" << observation.servingCellId;
    for (uint32_t satIdx = 0; satIdx < satellites.size(); ++satIdx)
    {
        const double prev = ue.prevDistances[satIdx];
        const double curr = observation.distancesMeters[satIdx];
        const char* trend = "init";
        if (prev >= 0.0)
        {
            trend = (curr < prev) ? "down" : ((curr > prev) ? "up" : "flat");
        }

        const double elevDeg = LeoOrbitCalculator::RadToDeg(observation.states[satIdx].elevationRad);
        const double azDeg = LeoOrbitCalculator::RadToDeg(observation.states[satIdx].azimuthRad);
        const double dopplerKhz = observation.states[satIdx].dopplerHz / 1e3;
        const double alphaDeg = LeoOrbitCalculator::RadToDeg(observation.beamBudgets[satIdx].scanAngleRad);
        const double thetaDeg =
            LeoOrbitCalculator::RadToDeg(observation.beamBudgets[satIdx].offBoresightAngleRad);
        const double drlRewardProxy = observation.beamBudgets[satIdx].beamLocked
                                          ? observation.beamBudgets[satIdx].rsrpDbm
                                          : -beamModelConfig.beamDropPenaltyDb;

        oss << " | sat" << satIdx << "(cell=" << satellites[satIdx].dev->GetCellId() << ")="
            << curr / 1000.0 << "km(" << trend << ")"
            << ",el=" << elevDeg << "deg"
            << ",az=" << azDeg << "deg"
            << ",fd=" << dopplerKhz << "kHz"
            << ",a=" << alphaDeg << "deg"
            << ",th=" << thetaDeg << "deg"
            << ",rsrp="
            << (std::isfinite(observation.beamBudgets[satIdx].rsrpDbm)
                    ? observation.beamBudgets[satIdx].rsrpDbm
                    : -999.0)
            << "dBm"
            << ",rew=" << drlRewardProxy
            << (observation.states[satIdx].visible ? ",VIS" : ",NVS")
            << (observation.beamBudgets[satIdx].beamLocked ? ",LOCK" : ",DROP");
    }
    std::cout << oss.str() << std::endl;
}

inline void
MaybePrintServingChangeAndKpi(UeRuntime& ue,
                              uint32_t ueIdx,
                              double nowSeconds,
                              double kpiIntervalSeconds,
                              bool printKpiReports,
                              const std::vector<SatelliteRuntime>& satellites,
                              const UeObservationSnapshot& observation)
{
    const bool servingChanged = (ue.lastServingCellForLog != 0 && observation.servingCellId != 0 &&
                                 observation.servingCellId != ue.lastServingCellForLog);
    if (servingChanged)
    {
        std::cout << "[SERVING] t=" << std::fixed << std::setprecision(3) << nowSeconds << "s ue="
                  << ueIdx << " cell " << ue.lastServingCellForLog << " -> "
                  << observation.servingCellId << std::endl;
    }

    const bool periodic = printKpiReports &&
                          ((ue.lastKpiReportTime < 0.0) ||
                           (nowSeconds - ue.lastKpiReportTime >= kpiIntervalSeconds - 1e-9));
    if (periodic)
    {
        std::ostringstream kpi;
        kpi << std::fixed << std::setprecision(3);
        kpi << "[KPI] t=" << nowSeconds << "s"
            << " ue=" << ueIdx
            << " servingCell=" << observation.servingCellId;
        if (observation.servingSatIdx != std::numeric_limits<uint32_t>::max())
        {
            kpi << " servingSat=sat" << observation.servingSatIdx
                << " rsrp=" << observation.beamBudgets[observation.servingSatIdx].rsrpDbm
                << "dBm alpha="
                << LeoOrbitCalculator::RadToDeg(
                       observation.beamBudgets[observation.servingSatIdx].scanAngleRad)
                << "deg state="
                << (observation.beamBudgets[observation.servingSatIdx].beamLocked ? "LOCK" : "DROP");
        }
        if (observation.bestRsrpIdx != std::numeric_limits<uint32_t>::max())
        {
            kpi << " bestSat=sat" << observation.bestRsrpIdx
                << "(cell=" << satellites[observation.bestRsrpIdx].dev->GetCellId()
                << ") bestRsrp=" << observation.bestRsrp << "dBm";
        }
        std::cout << kpi.str() << std::endl;
        ue.lastKpiReportTime = nowSeconds;
    }

    ue.lastServingCellForLog = observation.servingCellId;
    ue.prevDistances = observation.distancesMeters;
}

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

inline void
FlushBeamTraceRows(std::ofstream& satBeamTrace,
                   const std::vector<BeamTraceRow>& rows,
                   const std::vector<SatelliteRuntime>& satellites)
{
    if (!satBeamTrace.is_open())
    {
        return;
    }

    for (const auto& row : rows)
    {
        const auto& sat = satellites[row.satIndex];
        satBeamTrace << std::fixed << std::setprecision(3)
                     << row.timeSeconds << ","
                     << row.ueIndex << ","
                     << row.satIndex << ","
                     << row.cellId << ","
                     << (row.beamLocked ? 1 : 0) << ","
                     << row.scanLossDb << ","
                     << row.patternLossDb << ","
                     << row.fsplDb << ","
                     << row.atmLossDb << ","
                     << row.rsrpDbm << ","
                     << sat.attachedUeCount << ","
                     << sat.offeredPacketRate << ","
                     << sat.loadScore << ","
                     << (sat.admissionAllowed ? 1 : 0) << "\n";
    }
}

inline void
ApplyDesiredActiveNeighbours(std::vector<SatelliteRuntime>& satellites,
                             const std::map<uint32_t, std::set<uint16_t>>& desiredActiveNeighbours,
                             double nowSeconds,
                             bool compactReport,
                             bool printNrtEvents)
{
    for (uint32_t servingSatIdx = 0; servingSatIdx < satellites.size(); ++servingSatIdx)
    {
        auto& servingSat = satellites[servingSatIdx];
        for (uint32_t neighbourSatIdx = 0; neighbourSatIdx < satellites.size(); ++neighbourSatIdx)
        {
            if (neighbourSatIdx == servingSatIdx)
            {
                continue;
            }

            const uint16_t neighbourCell = satellites[neighbourSatIdx].dev->GetCellId();
            const bool shouldBeActive =
                desiredActiveNeighbours.count(servingSatIdx) > 0 &&
                desiredActiveNeighbours.at(servingSatIdx).count(neighbourCell) > 0;
            if (shouldBeActive)
            {
                if (servingSat.activeNeighbours.insert(neighbourCell).second)
                {
                    servingSat.rrc->AddX2Neighbour(neighbourCell);
                    if (!compactReport || printNrtEvents)
                    {
                        std::cout << "[NRT] t=" << std::fixed << std::setprecision(3) << nowSeconds
                                  << "s servingCell=" << servingSat.dev->GetCellId()
                                  << " ADD neighbourCell=" << neighbourCell << std::endl;
                    }
                }
            }
            else if (servingSat.activeNeighbours.erase(neighbourCell) > 0)
            {
                if (!compactReport || printNrtEvents)
                {
                    std::cout << "[NRT] t=" << std::fixed << std::setprecision(3) << nowSeconds
                              << "s servingCell=" << servingSat.dev->GetCellId()
                              << " REMOVE neighbourCell=" << neighbourCell << std::endl;
                }
            }
        }
    }
}

} // namespace ns3

#endif // LEO_NTN_HANDOVER_UPDATE_H
