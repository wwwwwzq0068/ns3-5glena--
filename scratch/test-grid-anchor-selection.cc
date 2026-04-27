#include "handover/grid-anchor-landing-rule.h"

#include <cstdlib>
#include <iostream>
#include <vector>

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

GridAnchorLandingCandidate
MakeCandidate(uint32_t gridId,
              bool legal,
              uint32_t ueCount,
              uint32_t priorityWeight,
              double score)
{
    GridAnchorLandingCandidate candidate;
    candidate.gridId = gridId;
    candidate.legal = legal;
    candidate.ueCount = ueCount;
    candidate.priorityWeight = priorityWeight;
    candidate.score = score;
    return candidate;
}

} // namespace

int
main()
{
    {
        const auto decision = SelectPreferredGridAnchorLanding(
            MakeCandidate(74, true, 2, 4, 10.0),
            {MakeCandidate(75, true, 3, 5, 8.0), MakeCandidate(76, true, 1, 1, 6.0)});
        Require(decision.found, "primary-with-demand case should produce a decision");
        Require(decision.anchorGridId == 74, "primary cell with UE should stay preferred");
        Require(decision.reason == GridAnchorLandingDecisionReason::PRIMARY_WITH_UE,
                "primary demand case should report PRIMARY_WITH_UE");
    }

    {
        const auto decision = SelectPreferredGridAnchorLanding(
            MakeCandidate(74, true, 0, 0, 10.0),
            {MakeCandidate(75, true, 2, 3, 8.0),
             MakeCandidate(76, true, 2, 5, 9.0),
             MakeCandidate(77, true, 5, 5, 11.0)});
        Require(decision.found, "first-ring demand case should produce a decision");
        Require(decision.anchorGridId == 77,
                "first-ring selection should prefer higher priority, then higher UE count");
        Require(decision.reason == GridAnchorLandingDecisionReason::FIRST_RING_WITH_UE,
                "first-ring demand case should report FIRST_RING_WITH_UE");
    }

    {
        const auto decision = SelectPreferredGridAnchorLanding(
            MakeCandidate(74, true, 0, 0, 10.0),
            {MakeCandidate(75, true, 0, 0, 8.0), MakeCandidate(76, true, 0, 0, 9.0)});
        Require(decision.found, "empty primary fallback should produce a decision");
        Require(decision.anchorGridId == 74,
                "empty local region should fall back to the legal primary cell");
        Require(decision.reason == GridAnchorLandingDecisionReason::EMPTY_PRIMARY_FALLBACK,
                "empty local region should report EMPTY_PRIMARY_FALLBACK");
    }

    {
        const auto decision = SelectPreferredGridAnchorLanding(
            MakeCandidate(74, false, 3, 6, 10.0),
            {MakeCandidate(75, false, 4, 7, 8.0), MakeCandidate(76, true, 1, 2, 9.0)});
        Require(decision.found, "legal first-ring candidate should replace illegal primary candidate");
        Require(decision.anchorGridId == 76,
                "illegal primary or blocked neighbors must be skipped");
    }

    {
        const auto decision = SelectPreferredGridAnchorLanding(
            MakeCandidate(74, false, 0, 0, 10.0),
            {MakeCandidate(75, false, 2, 3, 8.0), MakeCandidate(76, false, 0, 0, 9.0)});
        Require(!decision.found,
                "when local legal candidates are unavailable the helper should defer to wider fallback");
    }

    std::cout << "[TEST-PASS] grid-anchor landing rule behavior is correct" << std::endl;
    return 0;
}
