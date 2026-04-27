#include "handover/beam-link-budget.h"
#include "handover/leo-ntn-handover-config.h"

#include <cstdlib>
#include <iostream>

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
    const BaselineSimulationConfig config;

    Require(config.ueLayoutType == "seven-cell", "baseline default should keep seven-cell layout");
    Require(config.ueNum == 25, "baseline default should keep 25 UE");
    Require(config.handoverMode == "baseline", "baseline default should keep A3 baseline mode");

    Require(config.earthFixedBeamTargetMode == "grid-anchor",
            "baseline default should enable grid-anchor beam targeting");
    Require(config.beamExclusionCandidateK == 64,
            "baseline default should use K=64 beam exclusion search");
    Require(config.gnbAntennaRows == 12 && config.gnbAntennaColumns == 12,
            "baseline default should use a 12x12 gNB array");

    std::cout << "[TEST-PASS] baseline defaults are aligned with the current best reuse1 profile"
              << std::endl;
    return 0;
}
