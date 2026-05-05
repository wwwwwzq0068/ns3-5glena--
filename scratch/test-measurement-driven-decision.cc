#include "handover/leo-ntn-handover-decision.h"

#include <cstdlib>
#include <iostream>

using namespace ns3;

namespace
{

uint32_t g_visibilityPredictionCallCount = 0;

void
Require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "[TEST-FAIL] " << message << std::endl;
        std::exit(1);
    }
}

double
CountVisibilityPrediction(const MeasurementDrivenDecisionContext&,
                          uint32_t,
                          const LeoOrbitCalculator::GroundPoint&,
                          double)
{
    g_visibilityPredictionCallCount++;
    return 5.0;
}

double
ImprovedVisibilityPrediction(const MeasurementDrivenDecisionContext&,
                             uint32_t satIdx,
                             const LeoOrbitCalculator::GroundPoint&,
                             double)
{
    return satIdx == 1 ? 8.0 : 1.0;
}

NrRrcSap::MeasResults
BuildMeasurementResults()
{
    NrRrcSap::MeasResults measResults;
    measResults.measResultPCell.rsrpResult = 40;
    measResults.measResultPCell.rsrqResult = 30;
    measResults.haveMeasResultNeighCells = true;

    NrRrcSap::MeasResultEutra weakerCandidate;
    weakerCandidate.physCellId = 20;
    weakerCandidate.haveRsrpResult = true;
    weakerCandidate.rsrpResult = 55;
    weakerCandidate.haveRsrqResult = true;
    weakerCandidate.rsrqResult = 30;
    measResults.measResultListEutra.push_back(weakerCandidate);

    NrRrcSap::MeasResultEutra strongerCandidate;
    strongerCandidate.physCellId = 30;
    strongerCandidate.haveRsrpResult = true;
    strongerCandidate.rsrpResult = 75;
    strongerCandidate.haveRsrqResult = true;
    strongerCandidate.rsrqResult = 32;
    measResults.measResultListEutra.push_back(strongerCandidate);

    return measResults;
}

MeasurementDrivenDecisionContext
BuildDecisionContext(std::vector<SatelliteRuntime>& satellites,
                     std::map<uint16_t, uint32_t>& cellToSatellite,
                     HandoverMode handoverMode)
{
    MeasurementDrivenDecisionContext context{satellites, cellToSatellite};
    context.handoverMode = handoverMode;
    context.loadCongestionThreshold = 1.0;
    context.improvedSignalWeight = 1.0;
    context.improvedRsrqWeight = 0.0;
    context.improvedLoadWeight = 0.0;
    context.improvedVisibilityWeight = 0.0;
    context.improvedMinVisibilitySeconds = 0.0;
    context.improvedVisibilityHorizonSeconds = 8.0;
    context.improvedVisibilityPredictionStepSeconds = 0.5;
    context.improvedMinJointScoreMargin = 0.0;
    context.improvedMinCandidateRsrpDbm = -200.0;
    context.improvedMinCandidateRsrqDb = -100.0;
    context.improvedServingWeakRsrpDbm = -200.0;
    context.improvedServingWeakRsrqDb = -100.0;
    context.predictRemainingVisibilitySeconds = &CountVisibilityPrediction;
    return context;
}

} // namespace

int
main()
{
    Require(ParseHandoverMode("improved") == HandoverMode::IMPROVED,
            "handover mode parser should accept the improved mode");
    Require(ParseHandoverMode("improved-score-only") == HandoverMode::BASELINE,
            "score-only diagnostic mode should not be accepted in the final reproduction package");
    Require(ParseHandoverMode("unknown") == HandoverMode::BASELINE,
            "unknown handover modes should fall back to baseline");

    std::vector<SatelliteRuntime> satellites(3);
    for (auto& satellite : satellites)
    {
        satellite.loadScore = 0.1;
        satellite.admissionAllowed = true;
    }

    std::map<uint16_t, uint32_t> cellToSatellite;
    cellToSatellite[10] = 0;
    cellToSatellite[20] = 1;
    cellToSatellite[30] = 2;

    UeRuntime ue;
    ue.groundPoint = LeoOrbitCalculator::CreateGroundPoint(45.6, 84.9, 0.0);

    const auto measResults = BuildMeasurementResults();

    {
        auto context =
            BuildDecisionContext(satellites, cellToSatellite, HandoverMode::BASELINE);
        g_visibilityPredictionCallCount = 0;
        const auto selectedTarget =
            SelectMeasurementDrivenTarget(context, 10, 0, ue, measResults);

        Require(selectedTarget.has_value(),
                "baseline target selection should return the strongest measured neighbor");
        Require(selectedTarget->cellId == 30,
                "baseline target selection should keep the strongest-RSRP candidate");
        Require(g_visibilityPredictionCallCount == 0,
                "baseline target selection should not call visibility prediction");
    }

    {
        auto context =
            BuildDecisionContext(satellites, cellToSatellite, HandoverMode::IMPROVED);
        g_visibilityPredictionCallCount = 0;
        const auto selectedTarget =
            SelectMeasurementDrivenTarget(context, 10, 0, ue, measResults);

        Require(selectedTarget.has_value(),
                "improved target selection should still produce a candidate in the simple test case");
        Require(g_visibilityPredictionCallCount > 0,
                "improved target selection should continue to use visibility prediction");
    }

    {
        satellites[1].loadScore = 0.15;
        satellites[2].loadScore = 0.75;
        auto context = BuildDecisionContext(satellites, cellToSatellite, HandoverMode::IMPROVED);
        context.improvedSignalWeight = 0.4;
        context.improvedLoadWeight = 0.3;
        context.improvedVisibilityWeight = 0.3;
        context.improvedMinVisibilitySeconds = 0.0;
        context.improvedMinJointScoreMargin = 0.0;
        context.predictRemainingVisibilitySeconds = &ImprovedVisibilityPrediction;
        g_visibilityPredictionCallCount = 0;
        const auto selectedTarget =
            SelectMeasurementDrivenTarget(context, 10, 0, ue, measResults);

        Require(selectedTarget.has_value(),
                "improved target selection should still produce a candidate");
        Require(selectedTarget->cellId == 20,
                "improved target selection should be able to prefer the better joint candidate");
    }

    std::cout << "[TEST-PASS] measurement-driven decision behavior is correct" << std::endl;
    return 0;
}
