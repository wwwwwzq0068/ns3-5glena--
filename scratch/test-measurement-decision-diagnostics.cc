#include "handover/leo-ntn-handover-config.h"
#include "handover/leo-ntn-handover-decision.h"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace ns3;

namespace
{

void
Require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "[TEST-FAIL] " << message << std::endl;
        std::exit(1);
    }
}

} // namespace

int
main()
{
    BaselineSimulationConfig config;
    Require(!config.printMeasurementDecisionDiagnostics,
            "measurement decision diagnostics should stay disabled by default");

    MeasurementDecisionDebugInfo info;
    info.rawCandidateCount = 2;
    info.admissionAllowedCandidateCount = 2;
    info.visibilityQualifiedCandidateCount = 2;
    info.qualityQualifiedCandidateCount = 2;
    info.scoredCandidateCount = 1;
    info.servingWeak = true;
    info.crossLayerWeak = false;
    info.servingRsrpDbm = -109.0;
    info.servingRsrqDb = -16.0;
    info.bestSignalCellId = 11;
    info.bestSignalRsrpDbm = -105.0;
    info.bestSignalLoadScore = 0.64;
    info.bestSignalRemainingVisibilitySeconds = 1.5;
    info.bestJointCellId = 11;
    info.bestJointScore = 0.81;
    info.selectedCellId = 11;
    info.resolution = MeasurementDecisionResolution::SERVING_WEAK_BEST_SIGNAL;

    const std::string summary = FormatMeasurementDecisionDebugInfo(info);
    Require(summary.find("servingWeak=YES") != std::string::npos,
            "formatted diagnostics should include the serving-weak state");
    Require(summary.find("resolution=serving-weak-best-signal") != std::string::npos,
            "formatted diagnostics should include the decision resolution");
    Require(summary.find("selected=11") != std::string::npos,
            "formatted diagnostics should include the selected cell");

    std::cout << "[TEST-PASS] measurement decision diagnostics behave as expected" << std::endl;
    return 0;
}
