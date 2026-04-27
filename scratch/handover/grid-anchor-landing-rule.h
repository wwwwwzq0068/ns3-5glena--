#ifndef GRID_ANCHOR_LANDING_RULE_H
#define GRID_ANCHOR_LANDING_RULE_H

#include <cstdint>
#include <limits>
#include <vector>

namespace ns3
{

enum class GridAnchorLandingDecisionReason
{
    NONE,
    PRIMARY_WITH_UE,
    FIRST_RING_WITH_UE,
    EMPTY_PRIMARY_FALLBACK,
};

struct GridAnchorLandingCandidate
{
    uint32_t gridId = 0;
    bool legal = false;
    uint32_t ueCount = 0;
    uint32_t priorityWeight = 0;
    double score = std::numeric_limits<double>::infinity();
};

struct GridAnchorLandingDecision
{
    bool found = false;
    uint32_t anchorGridId = 0;
    GridAnchorLandingDecisionReason reason = GridAnchorLandingDecisionReason::NONE;
};

inline bool
PreferGridAnchorLandingCandidate(const GridAnchorLandingCandidate& candidate,
                                 const GridAnchorLandingCandidate* best)
{
    if (!candidate.legal || candidate.ueCount == 0)
    {
        return false;
    }
    if (best == nullptr)
    {
        return true;
    }
    if (candidate.priorityWeight != best->priorityWeight)
    {
        return candidate.priorityWeight > best->priorityWeight;
    }
    if (candidate.ueCount != best->ueCount)
    {
        return candidate.ueCount > best->ueCount;
    }
    if (candidate.score != best->score)
    {
        return candidate.score < best->score;
    }
    return candidate.gridId < best->gridId;
}

inline GridAnchorLandingDecision
SelectPreferredGridAnchorLanding(const GridAnchorLandingCandidate& primary,
                                 const std::vector<GridAnchorLandingCandidate>& firstRing)
{
    if (primary.legal && primary.ueCount > 0)
    {
        return {true, primary.gridId, GridAnchorLandingDecisionReason::PRIMARY_WITH_UE};
    }

    const GridAnchorLandingCandidate* bestRing = nullptr;
    for (const auto& candidate : firstRing)
    {
        if (PreferGridAnchorLandingCandidate(candidate, bestRing))
        {
            bestRing = &candidate;
        }
    }
    if (bestRing != nullptr)
    {
        return {true, bestRing->gridId, GridAnchorLandingDecisionReason::FIRST_RING_WITH_UE};
    }

    bool localRegionHasDemand = primary.ueCount > 0;
    for (const auto& candidate : firstRing)
    {
        localRegionHasDemand = localRegionHasDemand || candidate.ueCount > 0;
    }
    if (!localRegionHasDemand && primary.legal)
    {
        return {true, primary.gridId, GridAnchorLandingDecisionReason::EMPTY_PRIMARY_FALLBACK};
    }

    return {};
}

} // namespace ns3

#endif // GRID_ANCHOR_LANDING_RULE_H
