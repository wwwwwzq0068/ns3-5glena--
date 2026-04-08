#ifndef LEO_NTN_HANDOVER_DECISION_H
#define LEO_NTN_HANDOVER_DECISION_H

/*
 * 文件说明：
 * `leo-ntn-handover-decision.h` 负责集中保存 measurement-driven handover
 * 目标选择与算法安装相关的辅助逻辑。
 *
 * 设计目标：
 * - 将 baseline / improved 共用的候选构建、可见性预测和联合评分从主脚本抽离；
 * - 保持 `MeasurementReport -> target selection -> TriggerHandover` 主链不变；
 * - 让主脚本更聚焦于场景装配、回调注册和仿真运行。
 */

#include "leo-ntn-handover-config.h"
#include "leo-ntn-handover-runtime.h"
#include "leo-orbit-calculator.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-leo-a3-measurement-handover-algorithm.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace ns3
{

enum class HandoverMode
{
    BASELINE,
    IMPROVED,
};

inline HandoverMode
ParseHandoverMode(const std::string& handoverMode)
{
    if (handoverMode == "improved")
    {
        return HandoverMode::IMPROVED;
    }
    return HandoverMode::BASELINE;
}

inline const char*
ToString(HandoverMode handoverMode)
{
    return handoverMode == HandoverMode::IMPROVED ? "improved" : "baseline";
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

struct MeasurementDrivenDecisionContext
{
    const std::vector<SatelliteRuntime>& satellites;
    const std::vector<UeRuntime>& ues;
    const std::map<uint16_t, uint32_t>& cellToSatellite;
    double gmstAtEpochRad = 0.0;
    double carrierFrequencyHz = 0.0;
    double minElevationRad = 0.0;
    double improvedVisibilityHorizonSeconds = 0.0;
    double improvedVisibilityPredictionStepSeconds = 0.2;
    bool enableDynamicHoPreparation = true;
    double x2ProcessingDelayMs = 0.0;
    double x2MinLinkDelayMs = 0.0;
    double x2PropagationSpeedMetersPerSecond = LeoOrbitCalculator::kSpeedOfLight;
    double hoPreparationBaseDelayMs = 0.0;
    double hoPreparationLoadPenaltyMs = 0.0;
    double hoPreparationLowElevationPenaltyMs = 0.0;
    double loadCongestionThreshold = 1.0;
    HandoverMode handoverMode = HandoverMode::BASELINE;
    double improvedSignalWeight = 0.0;
    double improvedLoadWeight = 0.0;
    double improvedVisibilityWeight = 0.0;
    double improvedMinLoadScoreDelta = 0.0;
    double improvedMaxSignalGapDb = 0.0;
    double improvedReturnGuardSeconds = 0.0;
    double improvedMinVisibilitySeconds = 0.0;
};

inline std::optional<uint32_t>
ResolveUeIndexFromServingCellAndRnti(const std::vector<UeRuntime>& ues,
                                     uint16_t servingCellId,
                                     uint16_t rnti)
{
    for (uint32_t ueIdx = 0; ueIdx < ues.size(); ++ueIdx)
    {
        const auto& ue = ues[ueIdx];
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

inline Time
ComputeSatellitePairX2Delay(const MeasurementDrivenDecisionContext& context,
                            Ptr<Node> lhsNode,
                            Ptr<Node> rhsNode)
{
    NS_ABORT_MSG_IF(context.x2PropagationSpeedMetersPerSecond <= 0.0,
                    "x2PropagationSpeedMetersPerSecond must be > 0");

    const auto lhsMobility = lhsNode->GetObject<MobilityModel>();
    const auto rhsMobility = rhsNode->GetObject<MobilityModel>();
    NS_ABORT_MSG_IF(!lhsMobility || !rhsMobility, "X2 delay computation requires MobilityModel");

    const Vector lhs = lhsMobility->GetPosition();
    const Vector rhs = rhsMobility->GetPosition();
    const double distanceMeters = CalculateDistance(lhs, rhs);
    const double propagationDelayMs =
        1000.0 * distanceMeters / context.x2PropagationSpeedMetersPerSecond;
    const double totalDelayMs =
        std::max(context.x2MinLinkDelayMs, context.x2ProcessingDelayMs + propagationDelayMs);
    return MilliSeconds(totalDelayMs);
}

inline double
PredictRemainingVisibilitySeconds(const MeasurementDrivenDecisionContext& context,
                                  uint32_t satIdx,
                                  const LeoOrbitCalculator::GroundPoint& ueGroundPoint,
                                  double nowSeconds)
{
    if (satIdx >= context.satellites.size())
    {
        return 0.0;
    }

    const double horizonSeconds = std::max(0.0, context.improvedVisibilityHorizonSeconds);
    if (horizonSeconds <= 0.0)
    {
        return 0.0;
    }

    const auto computeVisible = [&](double sampleTimeSeconds) {
        return LeoOrbitCalculator::Calculate(sampleTimeSeconds,
                                             context.satellites[satIdx].orbit,
                                             context.gmstAtEpochRad,
                                             ueGroundPoint,
                                             context.carrierFrequencyHz,
                                             context.minElevationRad)
            .visible;
    };

    if (!computeVisible(nowSeconds))
    {
        return 0.0;
    }

    const double stepSeconds = std::max(0.05, context.improvedVisibilityPredictionStepSeconds);
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

inline LeoOrbitCalculator::OrbitState
ComputeCurrentObservation(const MeasurementDrivenDecisionContext& context,
                          uint32_t satIdx,
                          const LeoOrbitCalculator::GroundPoint& ueGroundPoint,
                          double nowSeconds)
{
    NS_ABORT_MSG_IF(satIdx >= context.satellites.size(),
                    "satIdx out of range in ComputeCurrentObservation");
    return LeoOrbitCalculator::Calculate(nowSeconds,
                                         context.satellites[satIdx].orbit,
                                         context.gmstAtEpochRad,
                                         ueGroundPoint,
                                         context.carrierFrequencyHz,
                                         context.minElevationRad);
}

inline double
ComputeDynamicHandoverPreparationDelayMs(const MeasurementDrivenDecisionContext& context,
                                         uint32_t sourceSatIdx,
                                         const MeasurementCandidate& targetCandidate,
                                         const UeRuntime& ue,
                                         double nowSeconds)
{
    if (!context.enableDynamicHoPreparation)
    {
        return 0.0;
    }

    const auto targetObservation =
        ComputeCurrentObservation(context, targetCandidate.satIdx, ue.groundPoint, nowSeconds);
    const double x2DelayMs =
        (sourceSatIdx < context.satellites.size())
            ? ComputeSatellitePairX2Delay(context,
                                          context.satellites[sourceSatIdx].node,
                                          context.satellites[targetCandidate.satIdx].node)
                  .GetMilliSeconds()
            : 0.0;
    const double accessPropagationMs =
        1000.0 * targetObservation.slantRangeMeters / LeoOrbitCalculator::kSpeedOfLight;
    const double loadPenaltyMs =
        context.hoPreparationLoadPenaltyMs *
        std::clamp(context.satellites[targetCandidate.satIdx].loadScore, 0.0, 1.0);
    const double elevationMarginDeg =
        std::max(0.0,
                 LeoOrbitCalculator::RadToDeg(targetObservation.elevationRad - context.minElevationRad));
    const double elevationReadiness = std::clamp(elevationMarginDeg / 25.0, 0.0, 1.0);
    const double lowElevationPenaltyMs =
        context.hoPreparationLowElevationPenaltyMs * (1.0 - elevationReadiness);

    return context.hoPreparationBaseDelayMs + x2DelayMs + accessPropagationMs + loadPenaltyMs +
           lowElevationPenaltyMs;
}

inline double
ComputeLoadPressureFromScore(double loadScore, double loadCongestionThreshold)
{
    const double congestionSpan = std::max(1e-9, loadCongestionThreshold);
    return std::clamp(loadScore / congestionSpan, 0.0, 1.0);
}

inline std::optional<MeasurementCandidate>
SelectMeasurementDrivenTarget(const MeasurementDrivenDecisionContext& context,
                              uint16_t servingCellId,
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

        const auto satIt = context.cellToSatellite.find(neighbour.physCellId);
        if (satIt == context.cellToSatellite.end() || satIt->second >= context.satellites.size())
        {
            continue;
        }

        const auto& satellite = context.satellites[satIt->second];
        MeasurementCandidate candidate;
        candidate.satIdx = satIt->second;
        candidate.cellId = neighbour.physCellId;
        candidate.rsrpDbm = nr::EutranMeasurementMapping::RsrpRange2Dbm(neighbour.rsrpResult);
        candidate.loadScore = satellite.loadScore;
        candidate.admissionAllowed = satellite.admissionAllowed;
        candidate.remainingVisibilitySeconds =
            PredictRemainingVisibilitySeconds(context,
                                             candidate.satIdx,
                                             ue.groundPoint,
                                             Simulator::Now().GetSeconds());
        candidates.push_back(candidate);
    }

    if (candidates.empty())
    {
        return std::nullopt;
    }

    const double sourceLoadScore =
        (sourceSatIdx < context.satellites.size()) ? context.satellites[sourceSatIdx].loadScore : 0.0;
    const double sourceLoadPressure =
        ComputeLoadPressureFromScore(sourceLoadScore, context.loadCongestionThreshold);

    if (context.handoverMode == HandoverMode::BASELINE)
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
        return std::nullopt;
    }

    const double nowSeconds = Simulator::Now().GetSeconds();
    const bool withinReturnGuard =
        context.improvedReturnGuardSeconds > 0.0 && ue.lastSuccessfulHoTimeSeconds >= 0.0 &&
        (nowSeconds - ue.lastSuccessfulHoTimeSeconds <= context.improvedReturnGuardSeconds) &&
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

    if (context.improvedMinVisibilitySeconds > 0.0)
    {
        std::vector<MeasurementCandidate> visibilityQualifiedCandidates;
        visibilityQualifiedCandidates.reserve(filteredCandidates.size());
        std::copy_if(filteredCandidates.begin(),
                     filteredCandidates.end(),
                     std::back_inserter(visibilityQualifiedCandidates),
                     [&](const auto& candidate) {
                         return candidate.remainingVisibilitySeconds >=
                                context.improvedMinVisibilitySeconds;
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
        context.improvedMaxSignalGapDb + 2.0 * sourceLoadPressure;
    const double dynamicMinLoadScoreDelta =
        std::max(0.05, context.improvedMinLoadScoreDelta * (1.0 - 0.5 * sourceLoadPressure));
    const double effectiveSignalWeight =
        std::max(0.15, context.improvedSignalWeight * (1.0 - 0.4 * sourceLoadPressure));
    const double effectiveLoadWeight = context.improvedLoadWeight + 0.6 * sourceLoadPressure;
    const double effectiveVisibilityWeight =
        context.improvedVisibilityWeight * (1.0 - 0.2 * sourceLoadPressure);

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
    const double totalWeight =
        std::max(1e-9,
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
        candidate.visibilityScore =
            std::clamp(candidate.remainingVisibilitySeconds /
                           std::max(1e-9, context.improvedVisibilityHorizonSeconds),
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

template <typename MeasurementReportHandler>
inline void
InstallMeasurementDrivenHandoverAlgorithms(
    std::vector<SatelliteRuntime>& satellites,
    std::vector<Ptr<NrLeoA3MeasurementHandoverAlgorithm>>& algorithms,
    const BaselineSimulationConfig& config,
    MeasurementReportHandler&& measurementReportHandler)
{
    algorithms.clear();
    algorithms.reserve(satellites.size());

    const uint8_t maxReportCells =
        static_cast<uint8_t>(std::clamp<uint16_t>(config.measurementMaxReportCells, 1, 32));

    for (auto& sat : satellites)
    {
        auto algorithm = CreateObject<NrLeoA3MeasurementHandoverAlgorithm>();
        algorithm->SetAttribute("Hysteresis", DoubleValue(config.hoHysteresisDb));
        algorithm->SetAttribute("TimeToTrigger", TimeValue(MilliSeconds(config.hoTttMs)));
        algorithm->SetAttribute("ReportIntervalMs", UintegerValue(config.measurementReportIntervalMs));
        algorithm->SetAttribute("MaxReportCells", UintegerValue(maxReportCells));
        algorithm->SetAttribute("TriggerHandover", BooleanValue(false));
        sat.rrc->SetNrHandoverManagementSapProvider(algorithm->GetNrHandoverManagementSapProvider());
        algorithm->SetNrHandoverManagementSapUser(sat.rrc->GetNrHandoverManagementSapUser());
        algorithm->TraceConnectWithoutContext(
            "MeasurementReport",
            MakeBoundCallback(std::forward<MeasurementReportHandler>(measurementReportHandler),
                              sat.dev->GetCellId()));
        sat.rrc->AggregateObject(algorithm);
        algorithm->Initialize();
        algorithms.push_back(algorithm);
    }
}

} // namespace ns3

#endif // LEO_NTN_HANDOVER_DECISION_H
