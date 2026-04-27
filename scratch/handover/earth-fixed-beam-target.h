#ifndef EARTH_FIXED_BEAM_TARGET_H
#define EARTH_FIXED_BEAM_TARGET_H

#include "ns3/vector.h"
#include "wgs84-hex-grid.h"

#include <cstdint>
#include <cmath>
#include <string>
#include <vector>

namespace ns3
{

enum class EarthFixedBeamTargetMode
{
    GRID_ANCHOR,
    NADIR_CONTINUOUS,
};

inline EarthFixedBeamTargetMode
ParseEarthFixedBeamTargetMode(const std::string& mode)
{
    if (mode == "grid-anchor")
    {
        return EarthFixedBeamTargetMode::GRID_ANCHOR;
    }
    return EarthFixedBeamTargetMode::NADIR_CONTINUOUS;
}

inline const char*
ToString(EarthFixedBeamTargetMode mode)
{
    return mode == EarthFixedBeamTargetMode::GRID_ANCHOR ? "grid-anchor" : "nadir-continuous";
}

inline bool
RequiresDiscreteAnchorCellGate(EarthFixedBeamTargetMode mode)
{
    return mode == EarthFixedBeamTargetMode::GRID_ANCHOR;
}

inline Vector
ResolveEarthFixedBeamTarget(const Vector& satEcef,
                            const Vector& gridAnchorEcef,
                            EarthFixedBeamTargetMode mode)
{
    if (mode == EarthFixedBeamTargetMode::NADIR_CONTINUOUS)
    {
        return ProjectNadirToWgs84(satEcef);
    }

    const double gridAnchorNorm = std::sqrt(gridAnchorEcef.x * gridAnchorEcef.x +
                                            gridAnchorEcef.y * gridAnchorEcef.y +
                                            gridAnchorEcef.z * gridAnchorEcef.z);
    if (gridAnchorNorm > 0.0)
    {
        return gridAnchorEcef;
    }
    return ProjectNadirToWgs84(satEcef);
}

inline Vector
ResolveDiscreteSafeNadirBeamTarget(const Vector& satEcef,
                                   const std::vector<Wgs84HexGridCell>& cells,
                                   uint32_t assignedGridId,
                                   double gridCenterLatDeg,
                                   double gridCenterLonDeg,
                                   double hexRadiusMeters)
{
    const Vector nadirPoint = ProjectNadirToWgs84(satEcef);
    if (assignedGridId == 0 || cells.empty() || hexRadiusMeters <= 0.0)
    {
        return nadirPoint;
    }

    const Wgs84HexGridCell* assignedCell = nullptr;
    for (const auto& cell : cells)
    {
        if (cell.id == assignedGridId)
        {
            assignedCell = &cell;
            break;
        }
    }
    if (assignedCell == nullptr)
    {
        return nadirPoint;
    }

    return assignedCell->ecef;
}

} // namespace ns3

#endif // EARTH_FIXED_BEAM_TARGET_H
