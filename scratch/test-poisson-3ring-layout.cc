#include "handover/leo-ntn-handover-runtime.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
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

int
RingFromRole(const std::string& role)
{
    if (role == "p3-center")
    {
        return 0;
    }
    if (role == "p3-ring1")
    {
        return 1;
    }
    if (role == "p3-ring2")
    {
        return 2;
    }
    return -1;
}

} // namespace

int
main()
{
    UeLayoutConfig layout;
    layout.layoutType = "poisson-3ring";
    layout.hexCellRadiusMeters = 20000.0;
    layout.poissonLambda = 1.5;
    layout.maxUePerCell = 5;
    layout.randomSeed = 7;

    const auto placements = BuildUePlacements(45.6, 84.9, 0.0, 25, layout);
    Require(placements.size() == 25, "poisson-3ring should build exactly ueNum placements");

    std::map<std::string, uint32_t> roleCounts;
    for (const auto& placement : placements)
    {
        ++roleCounts[placement.role];
        Require(RingFromRole(placement.role) >= 0, "poisson-3ring should label each UE by ring");

        const double distance = std::hypot(placement.eastOffsetMeters, placement.northOffsetMeters);
        Require(distance <= 5.0 * layout.hexCellRadiusMeters,
                "poisson-3ring UE should stay inside the local two-ring footprint");
    }

    Require(roleCounts["p3-center"] > 0, "poisson-3ring should keep demand in the center cell");
    Require(roleCounts["p3-ring1"] > 0, "poisson-3ring should place demand in first-ring cells");
    Require(roleCounts["p3-ring2"] > 0, "poisson-3ring should place demand in second-ring cells");

    const auto repeated = BuildUePlacements(45.6, 84.9, 0.0, 25, layout);
    Require(repeated.size() == placements.size(), "poisson-3ring should be reproducible by seed");
    for (uint32_t i = 0; i < placements.size(); ++i)
    {
        Require(std::abs(repeated[i].eastOffsetMeters - placements[i].eastOffsetMeters) < 1e-9,
                "poisson-3ring east offset should be reproducible by seed");
        Require(std::abs(repeated[i].northOffsetMeters - placements[i].northOffsetMeters) < 1e-9,
                "poisson-3ring north offset should be reproducible by seed");
    }

    std::cout << "[TEST-PASS] poisson-3ring layout generation is correct" << std::endl;
    return 0;
}
