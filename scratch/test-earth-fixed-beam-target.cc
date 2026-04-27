#include "handover/earth-fixed-beam-target.h"
#include "handover/wgs84-hex-grid.h"

#include <cmath>
#include <iostream>

using namespace ns3;

namespace
{

Vector
LiftFromSurface(const Vector& surfaceEcef, double altitudeMeters)
{
    const double norm = std::sqrt(surfaceEcef.x * surfaceEcef.x + surfaceEcef.y * surfaceEcef.y +
                                  surfaceEcef.z * surfaceEcef.z);
    const double scale = (norm + altitudeMeters) / std::max(1e-9, norm);
    return Vector(surfaceEcef.x * scale, surfaceEcef.y * scale, surfaceEcef.z * scale);
}

bool
AlmostEqual(const Vector& a, const Vector& b, double toleranceMeters)
{
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    const double dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz) <= toleranceMeters;
}

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
    const double gridCenterLatDeg = 45.6;
    const double gridCenterLonDeg = 84.9;
    const double hexRadiusMeters = 20000.0;
    const auto gridCells =
        BuildWgs84HexGrid(gridCenterLatDeg, gridCenterLonDeg, 200000.0, 200000.0, hexRadiusMeters);
    const Vector surfaceA = Wgs84GeodeticToEcef(45.6, 84.9, 0.0);
    const Vector surfaceB = Wgs84GeodeticToEcef(45.6, 85.2, 0.0);
    const Vector satA = LiftFromSurface(surfaceA, 600000.0);
    const Vector satB = LiftFromSurface(surfaceB, 600000.0);
    const Vector gridAnchor = Wgs84GeodeticToEcef(45.6, 84.9, 0.0);

    const Vector legacyTargetA =
        ResolveEarthFixedBeamTarget(satA, gridAnchor, EarthFixedBeamTargetMode::GRID_ANCHOR);
    const Vector legacyTargetB =
        ResolveEarthFixedBeamTarget(satB, gridAnchor, EarthFixedBeamTargetMode::GRID_ANCHOR);
    Require(AlmostEqual(legacyTargetA, gridAnchor, 1e-6),
            "grid-anchor mode should preserve the configured grid anchor");
    Require(AlmostEqual(legacyTargetB, gridAnchor, 1e-6),
            "grid-anchor mode should stay fixed even if the satellite moves");

    const Vector nadirTargetA =
        ResolveEarthFixedBeamTarget(satA, gridAnchor, EarthFixedBeamTargetMode::NADIR_CONTINUOUS);
    const Vector nadirTargetB =
        ResolveEarthFixedBeamTarget(satB, gridAnchor, EarthFixedBeamTargetMode::NADIR_CONTINUOUS);
    Require(AlmostEqual(nadirTargetA, surfaceA, 1.0),
            "nadir-continuous mode should point to the current sub-satellite point");
    Require(AlmostEqual(nadirTargetB, surfaceB, 1.0),
            "nadir-continuous mode should update when the satellite moves");
    Require(!AlmostEqual(nadirTargetA, nadirTargetB, 1000.0),
            "nadir-continuous mode must not remain fixed to one cell");

    Require(RequiresDiscreteAnchorCellGate(EarthFixedBeamTargetMode::GRID_ANCHOR),
            "grid-anchor mode should keep the anchor-cell gate");
    Require(!RequiresDiscreteAnchorCellGate(EarthFixedBeamTargetMode::NADIR_CONTINUOUS),
            "nadir-continuous mode should disable the anchor-cell gate");

    const auto naturalNearestA = FindNearestHexCellIndices(gridCells, nadirTargetA, 1);
    const auto naturalNearestB = FindNearestHexCellIndices(gridCells, nadirTargetB, 1);
    Require(!naturalNearestA.empty() && !naturalNearestB.empty(),
            "test grid should provide nearest cells for the nadir points");
    Require(gridCells[naturalNearestA.front()].id != gridCells[naturalNearestB.front()].id,
            "the two nadir points should naturally map to different hex cells");

    const Vector constrainedTargetB = ResolveDiscreteSafeNadirBeamTarget(satB,
                                                                         gridCells,
                                                                         gridCells[naturalNearestA.front()].id,
                                                                         gridCenterLatDeg,
                                                                         gridCenterLonDeg,
                                                                         hexRadiusMeters);
    const auto constrainedNearestB = FindNearestHexCellIndices(gridCells, constrainedTargetB, 1);
    Require(!constrainedNearestB.empty(),
            "constrained beam target should still map to a nearest grid cell");
    Require(gridCells[constrainedNearestB.front()].id == gridCells[naturalNearestA.front()].id,
            "discrete-safe nadir target should resolve to the assigned unique hex");
    Require(AlmostEqual(constrainedTargetB, gridCells[naturalNearestA.front()].ecef, 1.0),
            "assigned hex beam target should land on the hex center, not the boundary");

    const auto centerLatLon = OffsetMetersToLatLonDeg(gridCenterLatDeg, gridCenterLonDeg, 0.0, 0.0);
    const auto sameHexLatLon =
        OffsetMetersToLatLonDeg(gridCenterLatDeg, gridCenterLonDeg, 5000.0, 0.0);
    const auto eastNeighborLatLon =
        OffsetMetersToLatLonDeg(gridCenterLatDeg, gridCenterLonDeg,
                                std::sqrt(3.0) * hexRadiusMeters, 0.0);
    const Vector satSameHexA =
        LiftFromSurface(Wgs84GeodeticToEcef(centerLatLon.first, centerLatLon.second, 0.0), 600000.0);
    const Vector satSameHexB =
        LiftFromSurface(Wgs84GeodeticToEcef(sameHexLatLon.first, sameHexLatLon.second, 0.0), 600000.0);
    const Vector eastNeighborSurface =
        Wgs84GeodeticToEcef(eastNeighborLatLon.first, eastNeighborLatLon.second, 0.0);

    const auto naturalSharedA =
        FindNearestHexCellIndices(gridCells, ProjectNadirToWgs84(satSameHexA), 1);
    const auto naturalSharedB =
        FindNearestHexCellIndices(gridCells, ProjectNadirToWgs84(satSameHexB), 1);
    const auto eastNeighborIndices = FindNearestHexCellIndices(gridCells, eastNeighborSurface, 1);
    Require(!naturalSharedA.empty() && !naturalSharedB.empty() && !eastNeighborIndices.empty(),
            "shared-hex discrete-safety test should find all required grid cells");

    const uint32_t sharedGridId = gridCells[naturalSharedA.front()].id;
    const uint32_t eastNeighborGridId = gridCells[eastNeighborIndices.front()].id;
    Require(sharedGridId == gridCells[naturalSharedB.front()].id,
            "two nearby nadir targets should naturally quantize to the same shared hex");
    Require(sharedGridId != eastNeighborGridId,
            "the assigned fallback hex for the second beam should differ from the shared hex");

    const Vector constrainedSharedA = ResolveDiscreteSafeNadirBeamTarget(satSameHexA,
                                                                         gridCells,
                                                                         sharedGridId,
                                                                         gridCenterLatDeg,
                                                                         gridCenterLonDeg,
                                                                         hexRadiusMeters);
    const Vector constrainedSharedB = ResolveDiscreteSafeNadirBeamTarget(satSameHexB,
                                                                         gridCells,
                                                                         eastNeighborGridId,
                                                                         gridCenterLatDeg,
                                                                         gridCenterLonDeg,
                                                                         hexRadiusMeters);
    const auto constrainedSharedAIndices =
        FindNearestHexCellIndices(gridCells, constrainedSharedA, 1);
    const auto constrainedSharedBIndices =
        FindNearestHexCellIndices(gridCells, constrainedSharedB, 1);
    Require(!constrainedSharedAIndices.empty() && !constrainedSharedBIndices.empty(),
            "constrained shared-hex targets should still resolve to discrete hex cells");
    Require(gridCells[constrainedSharedAIndices.front()].id == sharedGridId,
            "first constrained beam should stay on its assigned shared hex");
    Require(gridCells[constrainedSharedBIndices.front()].id == eastNeighborGridId,
            "second constrained beam should move into its assigned non-overlapping hex");
    Require(AlmostEqual(constrainedSharedB, gridCells[eastNeighborIndices.front()].ecef, 1.0),
            "non-overlapping assigned beam target should use the assigned hex center");
    Require(gridCells[constrainedSharedAIndices.front()].id !=
                gridCells[constrainedSharedBIndices.front()].id,
            "final discrete beam results must not collapse onto the same hex");

    std::cout << "[TEST-PASS] earth-fixed beam target mode behavior is correct" << std::endl;
    return 0;
}
