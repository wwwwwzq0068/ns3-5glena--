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
#include "ns3/nr-leo-a3-measurement-handover-algorithm.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <string>
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
    switch (handoverMode)
    {
    case HandoverMode::IMPROVED:
        return "improved";
    case HandoverMode::BASELINE:
    default:
        return "baseline";
    }
}

inline bool
UsesJointScoreSelection(HandoverMode handoverMode)
{
    return handoverMode == HandoverMode::IMPROVED;
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
    double improvedMinCandidateRsrpDbm = -118.0;
    double improvedMinCandidateRsrqDb = -17.0;
    double improvedServingWeakRsrpDbm = -118.0;
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

inline MeasurementDrivenHandoverConfig
BuildMeasurementDrivenHandoverConfig(const BaselineSimulationConfig& config)
{
    MeasurementDrivenHandoverConfig handoverConfig;
    handoverConfig.measurementReportIntervalMs = config.measurementReportIntervalMs;
    handoverConfig.measurementMaxReportCells =
        static_cast<uint8_t>(std::clamp<uint16_t>(config.measurementMaxReportCells, 1, 32));
    handoverConfig.handoverMode = ParseHandoverMode(config.handoverMode);
    handoverConfig.improvedSignalWeight = config.improvedSignalWeight;
    handoverConfig.improvedLoadWeight = config.improvedLoadWeight;
    handoverConfig.improvedRsrqWeight = config.improvedRsrqWeight;
    handoverConfig.improvedVisibilityWeight = config.improvedVisibilityWeight;
    handoverConfig.improvedMinLoadScoreDelta = config.improvedMinLoadScoreDelta;
    handoverConfig.improvedMaxSignalGapDb = config.improvedMaxSignalGapDb;
    handoverConfig.improvedMinStableLeadTimeSeconds = config.improvedMinStableLeadTimeSeconds;
    handoverConfig.improvedMinVisibilitySeconds = config.improvedMinVisibilitySeconds;
    handoverConfig.improvedVisibilityHorizonSeconds = config.improvedVisibilityHorizonSeconds;
    handoverConfig.improvedVisibilityPredictionStepSeconds =
        config.improvedVisibilityPredictionStepSeconds;
    handoverConfig.improvedMinJointScoreMargin = config.improvedMinJointScoreMargin;
    handoverConfig.improvedMinCandidateRsrpDbm = config.improvedMinCandidateRsrpDbm;
    handoverConfig.improvedMinCandidateRsrqDb = config.improvedMinCandidateRsrqDb;
    handoverConfig.improvedServingWeakRsrpDbm = config.improvedServingWeakRsrpDbm;
    handoverConfig.improvedServingWeakRsrqDb = config.improvedServingWeakRsrqDb;
    handoverConfig.improvedMinRsrqAdvantageDb = config.improvedMinRsrqAdvantageDb;
    handoverConfig.improvedEnableCrossLayerPhyAssist =
        config.improvedEnableCrossLayerPhyAssist;
    handoverConfig.improvedCrossLayerPhyAlpha = config.improvedCrossLayerPhyAlpha;
    handoverConfig.improvedCrossLayerTblerThreshold = config.improvedCrossLayerTblerThreshold;
    handoverConfig.improvedCrossLayerSinrThresholdDb =
        config.improvedCrossLayerSinrThresholdDb;
    handoverConfig.improvedCrossLayerMinSamples = config.improvedCrossLayerMinSamples;
    return handoverConfig;
}

inline bool
IsCrossLayerPhyWeak(const MeasurementDrivenHandoverConfig& config, const UeRuntime& ue)
{
    if (!config.improvedEnableCrossLayerPhyAssist ||
        ue.recentPhySampleCount < config.improvedCrossLayerMinSamples)
    {
        return false;
    }

    const bool tblerWeak = ue.recentPhyTblerEwma >= config.improvedCrossLayerTblerThreshold;
    const bool sinrWeak = std::isfinite(ue.recentPhySinrDbEwma) &&
                          ue.recentPhySinrDbEwma <= config.improvedCrossLayerSinrThresholdDb;
    return tblerWeak || sinrWeak;
}

struct MeasurementDrivenDecisionContext
{
    const std::vector<SatelliteRuntime>& satellites;
    const std::map<uint16_t, uint32_t>& cellToSatellite;
    double gmstAtEpochRad = 0.0;
    double carrierFrequencyHz = 0.0;
    double minElevationRad = 0.0;
    double loadCongestionThreshold = 1.0;
    HandoverMode handoverMode = HandoverMode::BASELINE;
    double improvedSignalWeight = 0.0;
    double improvedRsrqWeight = 0.0;
    double improvedLoadWeight = 0.0;
    double improvedVisibilityWeight = 0.0;
    double improvedMinLoadScoreDelta = 0.0;
    double improvedMaxSignalGapDb = 0.0;
    double improvedMinVisibilitySeconds = 0.0;
    double improvedVisibilityHorizonSeconds = 0.0;
    double improvedVisibilityPredictionStepSeconds = 0.2;
    double improvedMinJointScoreMargin = 0.0;
    double improvedMinCandidateRsrpDbm = -std::numeric_limits<double>::infinity();
    double improvedMinCandidateRsrqDb = -std::numeric_limits<double>::infinity();
    double improvedServingWeakRsrpDbm = -std::numeric_limits<double>::infinity();
    double improvedServingWeakRsrqDb = -std::numeric_limits<double>::infinity();
    double improvedMinRsrqAdvantageDb = 0.0;
    bool (*isCandidateAllowed)(uint32_t satIdx, const UeRuntime& ue) = nullptr;
    bool (*isCrossLayerPhyWeak)(const UeRuntime& ue) = nullptr;
    double (*predictRemainingVisibilitySeconds)(const MeasurementDrivenDecisionContext& context,
                                                uint32_t satIdx,
                                                const LeoOrbitCalculator::GroundPoint& ueGroundPoint,
                                                double nowSeconds) = nullptr;
};

inline MeasurementDrivenDecisionContext
BuildMeasurementDrivenDecisionContext(
    const std::vector<SatelliteRuntime>& satellites,
    const std::map<uint16_t, uint32_t>& cellToSatellite,
    const MeasurementDrivenHandoverConfig& handoverConfig,
    double gmstAtEpochRad,
    double carrierFrequencyHz,
    double minElevationRad,
    double loadCongestionThreshold,
    bool (*isCandidateAllowed)(uint32_t satIdx, const UeRuntime& ue),
    bool (*isCrossLayerPhyWeak)(const UeRuntime& ue))
{
    MeasurementDrivenDecisionContext context{satellites, cellToSatellite};
    context.gmstAtEpochRad = gmstAtEpochRad;
    context.carrierFrequencyHz = carrierFrequencyHz;
    context.minElevationRad = minElevationRad;
    context.loadCongestionThreshold = loadCongestionThreshold;
    context.handoverMode = handoverConfig.handoverMode;
    context.improvedSignalWeight = handoverConfig.improvedSignalWeight;
    context.improvedRsrqWeight = handoverConfig.improvedRsrqWeight;
    context.improvedLoadWeight = handoverConfig.improvedLoadWeight;
    context.improvedVisibilityWeight = handoverConfig.improvedVisibilityWeight;
    context.improvedMinLoadScoreDelta = handoverConfig.improvedMinLoadScoreDelta;
    context.improvedMaxSignalGapDb = handoverConfig.improvedMaxSignalGapDb;
    context.improvedMinVisibilitySeconds = handoverConfig.improvedMinVisibilitySeconds;
    context.improvedVisibilityHorizonSeconds = handoverConfig.improvedVisibilityHorizonSeconds;
    context.improvedVisibilityPredictionStepSeconds =
        handoverConfig.improvedVisibilityPredictionStepSeconds;
    context.improvedMinJointScoreMargin = handoverConfig.improvedMinJointScoreMargin;
    context.improvedMinCandidateRsrpDbm = handoverConfig.improvedMinCandidateRsrpDbm;
    context.improvedMinCandidateRsrqDb = handoverConfig.improvedMinCandidateRsrqDb;
    context.improvedServingWeakRsrpDbm = handoverConfig.improvedServingWeakRsrpDbm;
    context.improvedServingWeakRsrqDb = handoverConfig.improvedServingWeakRsrqDb;
    context.improvedMinRsrqAdvantageDb = handoverConfig.improvedMinRsrqAdvantageDb;
    context.isCandidateAllowed = isCandidateAllowed;
    context.isCrossLayerPhyWeak = isCrossLayerPhyWeak;
    return context;
}

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

inline double
DefaultPredictRemainingVisibilitySeconds(const MeasurementDrivenDecisionContext& context,
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

inline double
PredictRemainingVisibilitySeconds(const MeasurementDrivenDecisionContext& context,
                                  uint32_t satIdx,
                                  const LeoOrbitCalculator::GroundPoint& ueGroundPoint,
                                  double nowSeconds)
{
    if (context.predictRemainingVisibilitySeconds != nullptr)
    {
        return context.predictRemainingVisibilitySeconds(context, satIdx, ueGroundPoint, nowSeconds);
    }
    return DefaultPredictRemainingVisibilitySeconds(context, satIdx, ueGroundPoint, nowSeconds);
}

inline double
NormalizeMetric(double value, double minValue, double maxValue)
{
    if (!std::isfinite(value) || !std::isfinite(minValue) || !std::isfinite(maxValue))
    {
        return 0.0;
    }
    const double span = std::max(1e-9, maxValue - minValue);
    return std::clamp((value - minValue) / span, 0.0, 1.0);
}

inline double
ComputeVisibilityScore(const MeasurementDrivenDecisionContext& context,
                       double remainingVisibilitySeconds)
{
    return std::clamp(remainingVisibilitySeconds /
                          std::max(1e-9, context.improvedVisibilityHorizonSeconds),
                      0.0,
                      1.0);
}

inline double
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

inline double
ComputeLoadPressureFromScore(double loadScore, double loadCongestionThreshold)
{
    const double congestionSpan = std::max(1e-9, loadCongestionThreshold);
    return std::clamp(loadScore / congestionSpan, 0.0, 1.0);
}

inline bool
IsServingLinkWeak(const MeasurementDrivenDecisionContext& context,
                  const NrRrcSap::MeasResults& measResults)
{
    const double servingRsrpDbm =
        nr::EutranMeasurementMapping::RsrpRange2Dbm(measResults.measResultPCell.rsrpResult);
    const double servingRsrqDb =
        nr::EutranMeasurementMapping::RsrqRange2Db(measResults.measResultPCell.rsrqResult);
    return servingRsrpDbm <= context.improvedServingWeakRsrpDbm ||
           servingRsrqDb <= context.improvedServingWeakRsrqDb;
}

inline bool
IsCrossLayerPhyWeak(const MeasurementDrivenDecisionContext& context, const UeRuntime& ue)
{
    return context.isCrossLayerPhyWeak != nullptr && context.isCrossLayerPhyWeak(ue);
}

inline std::optional<MeasurementCandidate>
SelectStrongestSignalCandidate(const std::vector<MeasurementCandidate>& candidates)
{
    if (candidates.empty())
    {
        return std::nullopt;
    }
    return *std::max_element(candidates.begin(),
                             candidates.end(),
                             [](const auto& lhs, const auto& rhs) {
                                 return lhs.rsrpDbm < rhs.rsrpDbm;
                             });
}

template <typename Predicate>
inline void
KeepCandidatesIfAny(std::vector<MeasurementCandidate>& candidates, Predicate&& predicate)
{
    std::vector<MeasurementCandidate> qualifiedCandidates;
    qualifiedCandidates.reserve(candidates.size());
    std::copy_if(candidates.begin(),
                 candidates.end(),
                 std::back_inserter(qualifiedCandidates),
                 std::forward<Predicate>(predicate));
    if (!qualifiedCandidates.empty())
    {
        candidates = std::move(qualifiedCandidates);
    }
}

inline std::vector<MeasurementCandidate>
BuildMeasuredCandidates(const MeasurementDrivenDecisionContext& context,
                        uint16_t servingCellId,
                        const UeRuntime& ue,
                        const NrRrcSap::MeasResults& measResults)
{
    std::vector<MeasurementCandidate> candidates;
    candidates.reserve(measResults.measResultListEutra.size());

    const double nowSeconds = Simulator::Now().GetSeconds();
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
        if (context.isCandidateAllowed != nullptr && !context.isCandidateAllowed(satIt->second, ue))
        {
            continue;
        }

        const auto& satellite = context.satellites[satIt->second];
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
        if (UsesJointScoreSelection(context.handoverMode))
        {
            candidate.remainingVisibilitySeconds =
                PredictRemainingVisibilitySeconds(context, candidate.satIdx, ue.groundPoint, nowSeconds);
        }
        candidates.push_back(candidate);
    }

    return candidates;
}

inline MeasurementCandidate
BuildServingMeasurementCandidate(const MeasurementDrivenDecisionContext& context,
                                 uint16_t servingCellId,
                                 uint32_t sourceSatIdx,
                                 const UeRuntime& ue,
                                 const NrRrcSap::MeasResults& measResults,
                                 double sourceLoadScore)
{
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
        PredictRemainingVisibilitySeconds(context,
                                         sourceSatIdx,
                                         ue.groundPoint,
                                         Simulator::Now().GetSeconds());
    return servingCandidate;
}

inline void
ApplyImprovedCandidateFilters(const MeasurementDrivenDecisionContext& context,
                              std::vector<MeasurementCandidate>& candidates)
{
    KeepCandidatesIfAny(candidates, [](const auto& candidate) {
        return candidate.admissionAllowed;
    });

    if (context.improvedMinVisibilitySeconds > 0.0)
    {
        KeepCandidatesIfAny(candidates, [&](const auto& candidate) {
            return candidate.remainingVisibilitySeconds >= context.improvedMinVisibilitySeconds;
        });
    }

    KeepCandidatesIfAny(candidates, [&](const auto& candidate) {
        const bool rsrpOk = candidate.rsrpDbm >= context.improvedMinCandidateRsrpDbm;
        const bool rsrqOk = std::isfinite(candidate.rsrqDb) &&
                            candidate.rsrqDb >= context.improvedMinCandidateRsrqDb;
        return rsrpOk && rsrqOk;
    });
}

inline std::optional<MeasurementCandidate>
ApplyRsrqAdvantageGuard(const MeasurementDrivenDecisionContext& context,
                        const MeasurementCandidate& servingCandidate,
                        bool servingWeak,
                        std::vector<MeasurementCandidate>& candidates)
{
    if (context.improvedMinRsrqAdvantageDb <= 0.0 || servingWeak)
    {
        return std::nullopt;
    }

    const double servingRsrqDb =
        std::isfinite(servingCandidate.rsrqDb) ? servingCandidate.rsrqDb : -19.5;
    std::vector<MeasurementCandidate> rsrqGuardCandidates;
    rsrqGuardCandidates.reserve(candidates.size());
    std::copy_if(candidates.begin(),
                 candidates.end(),
                 std::back_inserter(rsrqGuardCandidates),
                 [&](const auto& candidate) {
                     return std::isfinite(candidate.rsrqDb) &&
                            candidate.rsrqDb >= servingRsrqDb + context.improvedMinRsrqAdvantageDb;
                 });
    if (!rsrqGuardCandidates.empty())
    {
        candidates = std::move(rsrqGuardCandidates);
        return std::nullopt;
    }

    const auto bestSignal = SelectStrongestSignalCandidate(candidates);
    if (bestSignal.has_value() && bestSignal->rsrpDbm > servingCandidate.rsrpDbm)
    {
        return bestSignal;
    }
    return std::nullopt;
}

struct DynamicJointScoreConfig
{
    double maxSignalGapDb = 0.0;
    double minLoadScoreDelta = 0.0;
    double signalWeight = 0.0;
    double rsrqWeight = 0.0;
    double loadWeight = 0.0;
    double visibilityWeight = 0.0;
};

inline DynamicJointScoreConfig
BuildDynamicJointScoreConfig(const MeasurementDrivenDecisionContext& context,
                             double sourceLoadPressure)
{
    DynamicJointScoreConfig config;
    config.maxSignalGapDb = context.improvedMaxSignalGapDb + 2.0 * sourceLoadPressure;
    config.minLoadScoreDelta =
        std::max(0.05, context.improvedMinLoadScoreDelta * (1.0 - 0.5 * sourceLoadPressure));
    config.signalWeight =
        std::max(0.15, context.improvedSignalWeight * (1.0 - 0.4 * sourceLoadPressure));
    config.rsrqWeight = std::max(0.0, context.improvedRsrqWeight * (1.0 - 0.2 * sourceLoadPressure));
    config.loadWeight = context.improvedLoadWeight + 0.6 * sourceLoadPressure;
    config.visibilityWeight = context.improvedVisibilityWeight * (1.0 - 0.2 * sourceLoadPressure);

    const double totalWeight = std::max(
        1e-9, config.signalWeight + config.rsrqWeight + config.loadWeight + config.visibilityWeight);
    config.signalWeight /= totalWeight;
    config.rsrqWeight /= totalWeight;
    config.loadWeight /= totalWeight;
    config.visibilityWeight /= totalWeight;
    return config;
}

inline std::vector<MeasurementCandidate>
BuildJointScoreCandidatePool(const std::vector<MeasurementCandidate>& candidates,
                             const MeasurementCandidate& bestSignalCandidate,
                             const DynamicJointScoreConfig& scoreConfig)
{
    std::vector<MeasurementCandidate> scoredCandidates;
    scoredCandidates.reserve(candidates.size());
    for (const auto& candidate : candidates)
    {
        const double signalGapDb = bestSignalCandidate.rsrpDbm - candidate.rsrpDbm;
        const double loadAdvantage = bestSignalCandidate.loadScore - candidate.loadScore;
        const bool keepCandidate = candidate.cellId == bestSignalCandidate.cellId ||
                                   signalGapDb <= scoreConfig.maxSignalGapDb ||
                                   loadAdvantage >= scoreConfig.minLoadScoreDelta;
        if (keepCandidate)
        {
            scoredCandidates.push_back(candidate);
        }
    }
    return scoredCandidates;
}

inline void
ComputeJointScoreRanges(const MeasurementCandidate& servingCandidate,
                        const std::vector<MeasurementCandidate>& candidates,
                        double& minRsrpDbm,
                        double& maxRsrpDbm,
                        double& minRsrqDb,
                        double& maxRsrqDb)
{
    minRsrpDbm = std::numeric_limits<double>::infinity();
    maxRsrpDbm = -std::numeric_limits<double>::infinity();
    minRsrqDb = std::numeric_limits<double>::infinity();
    maxRsrqDb = -std::numeric_limits<double>::infinity();

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
    for (const auto& candidate : candidates)
    {
        minRsrpDbm = std::min(minRsrpDbm, candidate.rsrpDbm);
        maxRsrpDbm = std::max(maxRsrpDbm, candidate.rsrpDbm);
        if (std::isfinite(candidate.rsrqDb))
        {
            minRsrqDb = std::min(minRsrqDb, candidate.rsrqDb);
            maxRsrqDb = std::max(maxRsrqDb, candidate.rsrqDb);
        }
    }
}

inline void
AssignJointScores(const MeasurementDrivenDecisionContext& context,
                  const DynamicJointScoreConfig& scoreConfig,
                  double sourceLoadScore,
                  double sourceLoadPressure,
                  double minRsrpDbm,
                  double maxRsrpDbm,
                  double minRsrqDb,
                  double maxRsrqDb,
                  std::vector<MeasurementCandidate>& candidates)
{
    for (auto& candidate : candidates)
    {
        candidate.visibilityScore = ComputeVisibilityScore(context, candidate.remainingVisibilitySeconds);
        candidate.jointScore = ComputeJointScore(candidate,
                                                minRsrpDbm,
                                                maxRsrpDbm,
                                                minRsrqDb,
                                                maxRsrqDb,
                                                scoreConfig.signalWeight,
                                                scoreConfig.rsrqWeight,
                                                scoreConfig.loadWeight,
                                                scoreConfig.visibilityWeight,
                                                sourceLoadScore,
                                                sourceLoadPressure);
    }
}

inline std::optional<MeasurementCandidate>
SelectMeasurementDrivenTarget(const MeasurementDrivenDecisionContext& context,
                              uint16_t servingCellId,
                              uint32_t sourceSatIdx,
                              const UeRuntime& ue,
                              const NrRrcSap::MeasResults& measResults)
{
    auto candidates = BuildMeasuredCandidates(context, servingCellId, ue, measResults);

    if (candidates.empty())
    {
        return std::nullopt;
    }

    if (context.handoverMode == HandoverMode::BASELINE)
    {
        return SelectStrongestSignalCandidate(candidates);
    }

    const double sourceLoadScore =
        (sourceSatIdx < context.satellites.size()) ? context.satellites[sourceSatIdx].loadScore : 0.0;
    const double sourceLoadPressure =
        ComputeLoadPressureFromScore(sourceLoadScore, context.loadCongestionThreshold);
    MeasurementCandidate servingCandidate = BuildServingMeasurementCandidate(context,
                                                                            servingCellId,
                                                                            sourceSatIdx,
                                                                            ue,
                                                                            measResults,
                                                                            sourceLoadScore);
    const bool servingWeak = IsServingLinkWeak(context, measResults);
    const bool crossLayerWeak = IsCrossLayerPhyWeak(context, ue);

    std::vector<MeasurementCandidate> filteredCandidates = candidates;
    ApplyImprovedCandidateFilters(context, filteredCandidates);

    const auto guardedTarget =
        ApplyRsrqAdvantageGuard(context, servingCandidate, servingWeak, filteredCandidates);
    if (guardedTarget.has_value())
    {
        return guardedTarget;
    }

    const auto bestSignalTarget = SelectStrongestSignalCandidate(filteredCandidates);
    if (!bestSignalTarget.has_value())
    {
        return std::nullopt;
    }
    const MeasurementCandidate bestSignalCandidate = *bestSignalTarget;
    if (servingWeak || crossLayerWeak)
    {
        return bestSignalCandidate;
    }

    const auto scoreConfig = BuildDynamicJointScoreConfig(context, sourceLoadPressure);
    std::vector<MeasurementCandidate> scoredCandidates =
        BuildJointScoreCandidatePool(filteredCandidates, bestSignalCandidate, scoreConfig);
    if (scoredCandidates.empty())
    {
        return bestSignalCandidate;
    }

    double minRsrpDbm;
    double maxRsrpDbm;
    double minRsrqDb;
    double maxRsrqDb;
    ComputeJointScoreRanges(servingCandidate,
                            scoredCandidates,
                            minRsrpDbm,
                            maxRsrpDbm,
                            minRsrqDb,
                            maxRsrqDb);
    AssignJointScores(context,
                      scoreConfig,
                      sourceLoadScore,
                      sourceLoadPressure,
                      minRsrpDbm,
                      maxRsrpDbm,
                      minRsrqDb,
                      maxRsrqDb,
                      scoredCandidates);

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
            ComputeVisibilityScore(context, servingCandidate.remainingVisibilitySeconds);
        servingCandidate.jointScore = ComputeJointScore(servingCandidate,
                                                        minRsrpDbm,
                                                        maxRsrpDbm,
                                                        minRsrqDb,
                                                        maxRsrqDb,
                                                        scoreConfig.signalWeight,
                                                        scoreConfig.rsrqWeight,
                                                        scoreConfig.loadWeight,
                                                        scoreConfig.visibilityWeight,
                                                        sourceLoadScore,
                                                        sourceLoadPressure);
        if (bestJointIt->jointScore <
            servingCandidate.jointScore + context.improvedMinJointScoreMargin)
        {
            return std::nullopt;
        }
    }

    return *bestJointIt;
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
