#ifndef LEO_NTN_HANDOVER_GRID_ANCHOR_H
#define LEO_NTN_HANDOVER_GRID_ANCHOR_H

/*
 * 文件说明：
 * `leo-ntn-handover-grid-anchor.h` 负责保存 hex grid 缓存、demand snapshot
 * 和卫星波束锚点更新逻辑。
 *
 * 设计目标：
 * - 让主脚本只调度何时刷新 demand 与 anchor；
 * - 保持当前 `poisson-3ring + overlap-only + beam-only` 锚点语义不变；
 * - 将运行时网格查表和需求权重计算集中管理，避免主流程继续变长。
 */

#include "beam-link-budget.h"
#include "grid-anchor-landing-rule.h"
#include "leo-ntn-handover-config.h"
#include "leo-ntn-handover-runtime.h"
#include "leo-ntn-handover-scenario.h"
#include "leo-ntn-handover-utils.h"
#include "wgs84-hex-grid.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace ns3
{

struct DemandGridSnapshot
{
    std::set<uint32_t> gridIds;
    std::map<uint32_t, uint32_t> ueCounts;
    std::map<uint32_t, uint32_t> priorityWeights;
};

inline std::string
FormatUintList(const std::vector<uint32_t>& values)
{
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < values.size(); ++i)
    {
        if (i > 0)
        {
            oss << ",";
        }
        oss << values[i];
    }
    oss << "]";
    return oss.str();
}

inline double
GridAnchorDistanceMeters(const Vector& a, const Vector& b)
{
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    const double dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

inline const Wgs84HexGridCell*
LookupHexGridCellById(const std::vector<Wgs84HexGridCell>& cells,
                      const std::unordered_map<uint32_t, size_t>& cellIndexById,
                      uint32_t gridId)
{
    const auto it = cellIndexById.find(gridId);
    if (it == cellIndexById.end() || it->second >= cells.size())
    {
        return nullptr;
    }
    return &cells[it->second];
}

inline std::optional<uint32_t>
FindNearestGridCellId(const std::vector<Wgs84HexGridCell>& cells, const Vector& pointEcef)
{
    if (cells.empty())
    {
        return std::nullopt;
    }

    const auto nearestIndices = FindNearestHexCellIndices(cells, pointEcef, 1);
    if (nearestIndices.empty())
    {
        return std::nullopt;
    }
    return cells[nearestIndices.front()].id;
}

inline void
RebuildHexGridRuntimeCaches(
    const std::vector<Wgs84HexGridCell>& cells,
    double anchorGridHexRadiusKm,
    std::unordered_map<uint32_t, size_t>& cellIndexById,
    std::unordered_map<uint32_t, std::vector<uint32_t>>& firstRingNeighborGridIdsById)
{
    cellIndexById.clear();
    firstRingNeighborGridIdsById.clear();

    if (cells.empty())
    {
        return;
    }

    cellIndexById.reserve(cells.size());
    for (size_t i = 0; i < cells.size(); ++i)
    {
        cellIndexById[cells[i].id] = i;
    }

    firstRingNeighborGridIdsById.reserve(cells.size());
    for (const auto& centerCell : cells)
    {
        auto& neighbors = firstRingNeighborGridIdsById[centerCell.id];
        for (const auto& candidateCell : cells)
        {
            if (candidateCell.id == centerCell.id)
            {
                continue;
            }

            const double eastDelta = candidateCell.eastMeters - centerCell.eastMeters;
            const double northDelta = candidateCell.northMeters - centerCell.northMeters;
            const double neighborDistanceMeters = std::sqrt(3.0) * anchorGridHexRadiusKm * 1000.0;
            const double guardDistanceMeters = neighborDistanceMeters * 1.01;
            if (eastDelta * eastDelta + northDelta * northDelta <=
                guardDistanceMeters * guardDistanceMeters)
            {
                neighbors.push_back(candidateCell.id);
            }
        }
    }
}

inline void
InitializeUeHomeGridIds(std::vector<UeRuntime>& ues, const std::vector<Wgs84HexGridCell>& cells)
{
    for (auto& ue : ues)
    {
        ue.homeGridId = 0;
        if (const auto gridId = FindNearestGridCellId(cells, ue.groundPoint.ecef))
        {
            ue.homeGridId = gridId.value();
        }
    }
}

inline const std::vector<uint32_t>&
FindFirstRingNeighborGridIds(
    const std::unordered_map<uint32_t, std::vector<uint32_t>>& firstRingNeighborGridIdsById,
    uint32_t centerGridId)
{
    static const std::vector<uint32_t> kEmptyNeighborGridIds;
    const auto it = firstRingNeighborGridIdsById.find(centerGridId);
    return (it != firstRingNeighborGridIdsById.end()) ? it->second : kEmptyNeighborGridIds;
}

inline void
ClearDemandGridSnapshot(DemandGridSnapshot& snapshot)
{
    snapshot.gridIds.clear();
    snapshot.ueCounts.clear();
    snapshot.priorityWeights.clear();
}

inline void
AccumulateDemandGridSnapshot(DemandGridSnapshot& snapshot,
                             uint32_t gridId,
                             uint32_t residentUeIncrement,
                             uint32_t priorityWeightIncrement)
{
    if (residentUeIncrement == 0 && priorityWeightIncrement == 0)
    {
        return;
    }
    snapshot.gridIds.insert(gridId);
    snapshot.ueCounts[gridId] += residentUeIncrement;
    snapshot.priorityWeights[gridId] += priorityWeightIncrement;
}

inline void
RefreshDemandGridSnapshotFromPlacements(DemandGridSnapshot& snapshot,
                                        bool useWgs84HexGrid,
                                        const std::vector<Wgs84HexGridCell>& cells,
                                        const std::vector<UePlacement>& uePlacements)
{
    ClearDemandGridSnapshot(snapshot);
    if (!useWgs84HexGrid || cells.empty())
    {
        return;
    }

    for (const auto& placement : uePlacements)
    {
        if (const auto gridId = FindNearestGridCellId(cells, placement.groundPoint.ecef))
        {
            AccumulateDemandGridSnapshot(snapshot, gridId.value(), 1, 1);
        }
    }
}

template <typename SatelliteIndexResolver, typename BeamMembershipChecker, typename PhyWeakChecker>
inline void
RefreshDemandGridSnapshotFromRuntime(DemandGridSnapshot& snapshot,
                                     bool useWgs84HexGrid,
                                     const std::vector<Wgs84HexGridCell>& cells,
                                     const std::vector<UeRuntime>& ues,
                                     const std::vector<SatelliteRuntime>& satellites,
                                     DemandSnapshotMode demandSnapshotMode,
                                     SatelliteIndexResolver&& resolveSatelliteIndexFromCellId,
                                     BeamMembershipChecker&& isUeInsideAssignedBeam,
                                     PhyWeakChecker&& isCrossLayerPhyWeak)
{
    ClearDemandGridSnapshot(snapshot);
    if (!useWgs84HexGrid || cells.empty())
    {
        return;
    }

    for (const auto& ue : ues)
    {
        if (ue.homeGridId == 0)
        {
            continue;
        }

        uint32_t priorityWeight = 1;
        uint16_t servingCellId = 0;
        if (ue.dev && ue.dev->GetRrc())
        {
            servingCellId = ue.dev->GetRrc()->GetCellId();
        }
        const int32_t servingSatIdx = resolveSatelliteIndexFromCellId(servingCellId);
        if (demandSnapshotMode == DemandSnapshotMode::RUNTIME_UNDERSERVED_UE)
        {
            if (servingSatIdx < 0 || static_cast<uint32_t>(servingSatIdx) >= satellites.size())
            {
                priorityWeight += 2;
            }
            else
            {
                const auto& servingSat = satellites[static_cast<uint32_t>(servingSatIdx)];
                if (servingSat.currentAnchorGridId != 0 &&
                    servingSat.currentAnchorGridId != ue.homeGridId)
                {
                    priorityWeight += 1;
                }
                if (!isUeInsideAssignedBeam(static_cast<uint32_t>(servingSatIdx), ue))
                {
                    priorityWeight += 1;
                }
                if (!servingSat.admissionAllowed)
                {
                    priorityWeight += 1;
                }
            }
            if (isCrossLayerPhyWeak(ue))
            {
                priorityWeight += 1;
            }
        }

        AccumulateDemandGridSnapshot(snapshot, ue.homeGridId, 1, priorityWeight);
    }
}

inline uint32_t
GetDemandGridPriorityWeight(const DemandGridSnapshot& snapshot, uint32_t gridId)
{
    const auto it = snapshot.priorityWeights.find(gridId);
    return (it != snapshot.priorityWeights.end()) ? it->second : 0;
}

inline uint32_t
GetDemandGridUeCount(const DemandGridSnapshot& snapshot, uint32_t gridId)
{
    const auto it = snapshot.ueCounts.find(gridId);
    return (it != snapshot.ueCounts.end()) ? it->second : 0;
}

inline std::set<uint32_t>
BuildBeamAnchorExclusionSet(const std::vector<SatelliteRuntime>& satellites,
                            const std::vector<Wgs84HexGridCell>& cells,
                            const std::unordered_map<uint32_t, size_t>& cellIndexById,
                            uint32_t selfSatIndex)
{
    std::set<uint32_t> blockedGridIds;
    if (cells.empty())
    {
        return blockedGridIds;
    }

    for (uint32_t satIdx = 0; satIdx < satellites.size(); ++satIdx)
    {
        if (satIdx == selfSatIndex || satellites[satIdx].currentAnchorGridId == 0)
        {
            continue;
        }

        const auto* occupiedCell =
            LookupHexGridCellById(cells, cellIndexById, satellites[satIdx].currentAnchorGridId);
        if (occupiedCell == nullptr)
        {
            continue;
        }

        blockedGridIds.insert(occupiedCell->id);
    }
    return blockedGridIds;
}

inline uint32_t
GetGridAnchorCandidateCount(uint32_t gridNearestK, uint32_t beamExclusionCandidateK)
{
    return std::max<uint32_t>({1, gridNearestK, beamExclusionCandidateK});
}

inline GridAnchorSelection
ComputeDemandAwareGridAnchorSelection(
    const std::vector<Wgs84HexGridCell>& cells,
    const std::unordered_map<uint32_t, size_t>& cellIndexById,
    const std::unordered_map<uint32_t, std::vector<uint32_t>>& firstRingNeighborGridIdsById,
    const DemandGridSnapshot& snapshot,
    const BeamModelConfig& beamModelConfig,
    bool preferDemandAnchorCells,
    AnchorSelectionMode anchorSelectionMode,
    const Vector& satEcef,
    uint32_t candidateCount,
    const std::set<uint32_t>& blockedGridIds)
{
    auto fallback = ComputeGridAnchorSelection(cells, satEcef, candidateCount, blockedGridIds);
    if (!preferDemandAnchorCells || snapshot.gridIds.empty())
    {
        return fallback;
    }

    const Vector nadirPoint = ProjectNadirToWgs84(satEcef);
    const auto computeDemandCandidateScore =
        [&](const Wgs84HexGridCell& cell) -> std::optional<double> {
        const auto anchorBudget =
            CalculateEarthFixedBeamBudget(satEcef, cell.ecef, cell.ecef, beamModelConfig);
        if (!anchorBudget.valid || !anchorBudget.beamLocked)
        {
            return std::nullopt;
        }

        const double distanceMeters = GridAnchorDistanceMeters(nadirPoint, cell.ecef);
        return distanceMeters + anchorBudget.scanLossDb * 1000.0;
    };

    const auto makeLandingCandidate = [&](uint32_t gridId) -> GridAnchorLandingCandidate {
        GridAnchorLandingCandidate candidate;
        candidate.gridId = gridId;

        const auto* cell = LookupHexGridCellById(cells, cellIndexById, gridId);
        if (cell == nullptr)
        {
            return candidate;
        }

        candidate.ueCount = GetDemandGridUeCount(snapshot, gridId);
        candidate.priorityWeight = GetDemandGridPriorityWeight(snapshot, gridId);
        const auto score = computeDemandCandidateScore(*cell);
        candidate.legal = blockedGridIds.find(gridId) == blockedGridIds.end() && score.has_value();
        if (score.has_value())
        {
            candidate.score = score.value();
        }
        return candidate;
    };

    if (!fallback.nearestGridIds.empty())
    {
        const uint32_t primaryGridId = fallback.nearestGridIds.front();
        std::vector<GridAnchorLandingCandidate> firstRingCandidates;
        for (const auto neighborGridId :
             FindFirstRingNeighborGridIds(firstRingNeighborGridIdsById, primaryGridId))
        {
            firstRingCandidates.push_back(makeLandingCandidate(neighborGridId));
        }

        const auto landingDecision =
            SelectPreferredGridAnchorLanding(makeLandingCandidate(primaryGridId), firstRingCandidates);
        if (landingDecision.found)
        {
            if (const auto* chosenCell =
                    LookupHexGridCellById(cells, cellIndexById, landingDecision.anchorGridId))
            {
                fallback.found = true;
                fallback.anchorGridId = chosenCell->id;
                fallback.anchorEcef = chosenCell->ecef;
                return fallback;
            }
        }
    }

    if (anchorSelectionMode == AnchorSelectionMode::DEMAND_MAX_UE_NEAR_NADIR)
    {
        uint32_t bestPriorityWeight = 0;
        uint32_t bestUeCount = 0;
        double bestScore = std::numeric_limits<double>::infinity();
        const Wgs84HexGridCell* bestCell = nullptr;

        for (const auto gridId : fallback.nearestGridIds)
        {
            const auto* cell = LookupHexGridCellById(cells, cellIndexById, gridId);
            if (cell == nullptr || snapshot.gridIds.find(cell->id) == snapshot.gridIds.end() ||
                blockedGridIds.find(cell->id) != blockedGridIds.end())
            {
                continue;
            }

            const uint32_t priorityWeight = GetDemandGridPriorityWeight(snapshot, cell->id);
            const uint32_t ueCount = GetDemandGridUeCount(snapshot, cell->id);
            if (ueCount == 0)
            {
                continue;
            }

            const auto score = computeDemandCandidateScore(*cell);
            if (!score.has_value())
            {
                continue;
            }

            if (bestCell == nullptr || priorityWeight > bestPriorityWeight ||
                (priorityWeight == bestPriorityWeight && ueCount > bestUeCount) ||
                (priorityWeight == bestPriorityWeight && ueCount == bestUeCount &&
                 score.value() < bestScore))
            {
                bestCell = cell;
                bestPriorityWeight = priorityWeight;
                bestUeCount = ueCount;
                bestScore = score.value();
            }
        }

        if (bestCell != nullptr)
        {
            fallback.found = true;
            fallback.anchorGridId = bestCell->id;
            fallback.anchorEcef = bestCell->ecef;
            return fallback;
        }
    }

    double bestScore = std::numeric_limits<double>::infinity();
    const Wgs84HexGridCell* bestCell = nullptr;

    for (const auto& cell : cells)
    {
        if (snapshot.gridIds.find(cell.id) == snapshot.gridIds.end() ||
            blockedGridIds.find(cell.id) != blockedGridIds.end())
        {
            continue;
        }

        const auto score = computeDemandCandidateScore(cell);
        if (!score.has_value())
        {
            continue;
        }

        if (score.value() < bestScore)
        {
            bestScore = score.value();
            bestCell = &cell;
        }
    }

    if (bestCell == nullptr)
    {
        return fallback;
    }

    fallback.found = true;
    fallback.anchorGridId = bestCell->id;
    fallback.anchorEcef = bestCell->ecef;
    return fallback;
}

inline void
UpdateSatelliteAnchorFromGrid(
    std::vector<SatelliteRuntime>& satellites,
    const std::vector<Wgs84HexGridCell>& cells,
    const std::unordered_map<uint32_t, size_t>& cellIndexById,
    const std::unordered_map<uint32_t, std::vector<uint32_t>>& firstRingNeighborGridIdsById,
    const DemandGridSnapshot& snapshot,
    const BeamModelConfig& beamModelConfig,
    bool useWgs84HexGrid,
    bool preferDemandAnchorCells,
    AnchorSelectionMode anchorSelectionMode,
    double anchorGridHexRadiusKm,
    uint32_t gridNearestK,
    uint32_t beamExclusionCandidateK,
    double anchorGridSwitchGuardMeters,
    double anchorGridHysteresisSeconds,
    bool printGridAnchorEvents,
    uint32_t satIndex,
    const Vector& satEcef,
    double nowSeconds,
    bool forceLog)
{
    if (!useWgs84HexGrid || cells.empty() || satIndex >= satellites.size())
    {
        return;
    }

    auto& sat = satellites[satIndex];
    const auto blockedGridIds =
        BuildBeamAnchorExclusionSet(satellites, cells, cellIndexById, satIndex);
    const auto anchor = ComputeDemandAwareGridAnchorSelection(
        cells,
        cellIndexById,
        firstRingNeighborGridIdsById,
        snapshot,
        beamModelConfig,
        preferDemandAnchorCells,
        anchorSelectionMode,
        satEcef,
        GetGridAnchorCandidateCount(gridNearestK, beamExclusionCandidateK),
        blockedGridIds);
    if (!anchor.found)
    {
        return;
    }

    sat.nearestGridIds = anchor.nearestGridIds;
    const auto logAnchor = [&](uint32_t anchorGridId) {
        std::cout << "[GRID-ANCHOR] t=" << std::fixed << std::setprecision(3) << nowSeconds << "s"
                  << " sat" << satIndex
                  << " cell=" << sat.dev->GetCellId()
                  << " anchorGrid=" << anchorGridId
                  << " nearestK=" << FormatUintList(sat.nearestGridIds) << std::endl;
    };
    const auto clearPendingAnchor = [&]() {
        sat.pendingAnchorGridId = 0;
        sat.pendingAnchorEcef = Vector(0.0, 0.0, 0.0);
        sat.pendingAnchorLeadStartTimeSeconds = -1.0;
    };
    const auto commitAnchor = [&](uint32_t anchorGridId, const Vector& anchorEcef) {
        sat.currentAnchorGridId = anchorGridId;
        sat.cellAnchorEcef = anchorEcef;
        clearPendingAnchor();
    };

    if (sat.currentAnchorGridId == 0)
    {
        commitAnchor(anchor.anchorGridId, anchor.anchorEcef);
        if (forceLog || printGridAnchorEvents)
        {
            logAnchor(sat.currentAnchorGridId);
        }
        return;
    }

    if (anchor.anchorGridId == sat.currentAnchorGridId)
    {
        sat.cellAnchorEcef = anchor.anchorEcef;
        clearPendingAnchor();
        if (forceLog)
        {
            logAnchor(sat.currentAnchorGridId);
        }
        return;
    }

    const bool currentAnchorBlocked =
        (blockedGridIds.find(sat.currentAnchorGridId) != blockedGridIds.end());
    const Vector nadirPoint = ProjectNadirToWgs84(satEcef);
    const double candidateDistanceMeters = GridAnchorDistanceMeters(nadirPoint, anchor.anchorEcef);
    double currentDistanceMeters = std::numeric_limits<double>::infinity();
    if (const auto* currentCell = LookupHexGridCellById(cells, cellIndexById, sat.currentAnchorGridId))
    {
        currentDistanceMeters = GridAnchorDistanceMeters(nadirPoint, currentCell->ecef);
    }
    else
    {
        commitAnchor(anchor.anchorGridId, anchor.anchorEcef);
        if (forceLog || printGridAnchorEvents)
        {
            logAnchor(sat.currentAnchorGridId);
        }
        return;
    }

    if (currentAnchorBlocked)
    {
        commitAnchor(anchor.anchorGridId, anchor.anchorEcef);
        if (forceLog || printGridAnchorEvents)
        {
            logAnchor(sat.currentAnchorGridId);
        }
        return;
    }

    const bool candidateHasGuardAdvantage =
        (currentDistanceMeters - candidateDistanceMeters >= anchorGridSwitchGuardMeters - 1e-9);
    if (!candidateHasGuardAdvantage)
    {
        clearPendingAnchor();
        if (forceLog)
        {
            logAnchor(sat.currentAnchorGridId);
        }
        return;
    }

    if (anchorGridHysteresisSeconds <= 0.0)
    {
        commitAnchor(anchor.anchorGridId, anchor.anchorEcef);
        if (forceLog || printGridAnchorEvents)
        {
            logAnchor(sat.currentAnchorGridId);
        }
        return;
    }

    if (sat.pendingAnchorGridId != anchor.anchorGridId)
    {
        sat.pendingAnchorGridId = anchor.anchorGridId;
        sat.pendingAnchorEcef = anchor.anchorEcef;
        sat.pendingAnchorLeadStartTimeSeconds = nowSeconds;
        if (forceLog)
        {
            logAnchor(sat.currentAnchorGridId);
        }
        return;
    }

    sat.pendingAnchorEcef = anchor.anchorEcef;
    if (nowSeconds - sat.pendingAnchorLeadStartTimeSeconds + 1e-9 >= anchorGridHysteresisSeconds)
    {
        commitAnchor(anchor.anchorGridId, anchor.anchorEcef);
        if (forceLog || printGridAnchorEvents)
        {
            logAnchor(sat.currentAnchorGridId);
        }
    }
    else if (forceLog)
    {
        logAnchor(sat.currentAnchorGridId);
    }
}

} // namespace ns3

#endif // LEO_NTN_HANDOVER_GRID_ANCHOR_H
