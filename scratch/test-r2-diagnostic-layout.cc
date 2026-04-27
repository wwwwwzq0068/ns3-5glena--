#include "handover/leo-ntn-handover-runtime.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>
#include <utility>

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

std::pair<int, int>
QuantizeOffsetMm(const UePlacement& placement)
{
    return {static_cast<int>(std::lround(placement.eastOffsetMeters * 1000.0)),
            static_cast<int>(std::lround(placement.northOffsetMeters * 1000.0))};
}

} // namespace

int
main()
{
    UeLayoutConfig layout;
    layout.layoutType = "r2-diagnostic";
    layout.hexCellRadiusMeters = 20000.0;

    const auto placements = BuildUePlacements(45.6, 84.9, 0.0, 19, layout);
    Require(placements.size() == 19, "r2-diagnostic should build exactly 19 placements");

    uint32_t centerCount = 0;
    uint32_t ring1Count = 0;
    uint32_t ring2Count = 0;
    std::set<std::pair<int, int>> uniqueOffsetsMm;

    for (const auto& placement : placements)
    {
        uniqueOffsetsMm.insert(QuantizeOffsetMm(placement));
        if (placement.role == "r2-center")
        {
            ++centerCount;
        }
        else if (placement.role == "r2-ring1")
        {
            ++ring1Count;
        }
        else if (placement.role == "r2-ring2")
        {
            ++ring2Count;
        }
    }

    Require(uniqueOffsetsMm.size() == 19, "r2-diagnostic offsets should all be unique");
    Require(centerCount == 1, "r2-diagnostic should contain exactly one center UE");
    Require(ring1Count == 6, "r2-diagnostic should contain exactly six first-ring UE");
    Require(ring2Count == 12, "r2-diagnostic should contain exactly twelve second-ring UE");

    const double dx = std::sqrt(3.0) * layout.hexCellRadiusMeters;
    const double dy = 1.5 * layout.hexCellRadiusMeters;
    Require(uniqueOffsetsMm.count({0, 0}) == 1, "center offset should exist");
    Require(uniqueOffsetsMm.count({static_cast<int>(std::lround(2.0 * dx * 1000.0)), 0}) == 1,
            "east two-ring offset should exist");
    Require(uniqueOffsetsMm.count({static_cast<int>(std::lround(-2.0 * dx * 1000.0)), 0}) == 1,
            "west two-ring offset should exist");
    Require(uniqueOffsetsMm.count({0, static_cast<int>(std::lround(2.0 * dy * 1000.0))}) == 1,
            "north two-ring offset should exist");
    Require(uniqueOffsetsMm.count({0, static_cast<int>(std::lround(-2.0 * dy * 1000.0))}) == 1,
            "south two-ring offset should exist");

    std::cout << "[TEST-PASS] r2-diagnostic layout geometry is correct" << std::endl;
    return 0;
}
