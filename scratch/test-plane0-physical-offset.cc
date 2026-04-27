#include "handover/beam-link-budget.h"
#include "handover/leo-ntn-handover-utils.h"

#include <cmath>
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

LeoOrbitCalculator::KeplerElements
BuildOrbit(double semiMajorAxisMeters,
           double inclinationRad,
           double raanRad,
           double argPerigeeRad,
           double baseTrueAnomalyRad,
           double meanMotionRadPerSec,
           double overpassTimeSeconds)
{
    LeoOrbitCalculator::KeplerElements orbit;
    orbit.semiMajorAxisMeters = semiMajorAxisMeters;
    orbit.eccentricity = 0.0;
    orbit.inclinationRad = inclinationRad;
    orbit.raanRad = raanRad;
    orbit.argPerigeeRad = argPerigeeRad;
    orbit.trueAnomalyAtEpochRad = baseTrueAnomalyRad - meanMotionRadPerSec * overpassTimeSeconds;
    return orbit;
}

struct GroundTrackPoint
{
    double eastMeters = 0.0;
    double northMeters = 0.0;
};

std::vector<GroundTrackPoint>
BuildPlaneGroundTrack(uint32_t planeIdx,
                      double alignmentReferenceTimeSeconds,
                      double overpassGapSeconds,
                      double interPlaneTimeOffsetSeconds,
                      uint32_t satsInPlane,
                      double semiMajorAxisMeters,
                      double inclinationRad,
                      double baseRaanRad,
                      double interPlaneRaanSpacingDeg,
                      double plane0RaanOffsetDeg,
                      double argPerigeeRad,
                      double baseTrueAnomalyRad,
                      double meanMotionRadPerSec,
                      double centerLatDeg,
                      double centerLonDeg)
{
    std::vector<GroundTrackPoint> points;
    const double planeCenterIndex = (static_cast<double>(satsInPlane) - 1.0) / 2.0;
    const double planeRaanRad = ComputePlaneRaanRad(baseRaanRad,
                                                    interPlaneRaanSpacingDeg,
                                                    plane0RaanOffsetDeg,
                                                    planeIdx);
    const double planeTimeOffset = interPlaneTimeOffsetSeconds * planeIdx;
    for (uint32_t slotIdx = 0; slotIdx < satsInPlane; ++slotIdx)
    {
        const double overpassTime =
            alignmentReferenceTimeSeconds +
            (static_cast<double>(slotIdx) - planeCenterIndex) * overpassGapSeconds +
            planeTimeOffset;
        const auto orbit = BuildOrbit(semiMajorAxisMeters,
                                      inclinationRad,
                                      planeRaanRad,
                                      argPerigeeRad,
                                      baseTrueAnomalyRad,
                                      meanMotionRadPerSec,
                                      overpassTime);
        const auto state = LeoOrbitCalculator::CalculateSatelliteState(0.0, orbit, 0.0);
        const Vector subpoint = ProjectNadirToWgs84(state.ecef);
        const Vector enu = Wgs84EcefToEnu(subpoint, centerLatDeg, centerLonDeg, 0.0);
        points.push_back({enu.x, enu.y});
    }
    return points;
}

double
MeanTrackNormalMeters(const std::vector<GroundTrackPoint>& planePoints,
                      const std::vector<GroundTrackPoint>& plane0ReferencePoints,
                      const std::vector<GroundTrackPoint>& plane1ReferencePoints)
{
    Require(planePoints.size() >= 2, "plane track must contain at least two points");
    Require(plane0ReferencePoints.size() >= 2,
            "plane0 reference track must contain at least two points");
    Require(plane1ReferencePoints.size() >= 2,
            "plane1 reference track must contain at least two points");

    const auto& start = plane0ReferencePoints.front();
    const auto& end = plane0ReferencePoints.back();
    const double dx = end.eastMeters - start.eastMeters;
    const double dy = end.northMeters - start.northMeters;
    const double length = std::hypot(dx, dy);
    Require(length > 0.0, "reference plane track direction must be non-zero");

    double normalX = -dy / length;
    double normalY = dx / length;

    const double plane0MeanEast =
        (plane0ReferencePoints.front().eastMeters + plane0ReferencePoints.back().eastMeters) / 2.0;
    const double plane0MeanNorth =
        (plane0ReferencePoints.front().northMeters + plane0ReferencePoints.back().northMeters) / 2.0;
    const double plane1MeanEast =
        (plane1ReferencePoints.front().eastMeters + plane1ReferencePoints.back().eastMeters) / 2.0;
    const double plane1MeanNorth =
        (plane1ReferencePoints.front().northMeters + plane1ReferencePoints.back().northMeters) / 2.0;
    if ((plane1MeanEast - plane0MeanEast) * normalX + (plane1MeanNorth - plane0MeanNorth) * normalY <
        0.0)
    {
        normalX = -normalX;
        normalY = -normalY;
    }

    double sum = 0.0;
    for (const auto& p : planePoints)
    {
        sum += p.eastMeters * normalX + p.northMeters * normalY;
    }
    return sum / static_cast<double>(planePoints.size());
}

} // namespace

int
main()
{
    constexpr double kAlignmentReferenceTimeSeconds = 6.5;
    constexpr double kOverpassGapSeconds = 3.0;
    constexpr double kScenarioOverpassGapSeconds = 6.0;
    constexpr double kPlane0TimeOffsetSeconds = -3.5;
    constexpr double kPlane1LegacyOverpassGapSeconds = 3.0;
    constexpr double kInterPlaneTimeOffsetSeconds = 7.5;
    constexpr double kInterPlaneRaanSpacingDeg = -1.0;
    constexpr double kPlane0RaanOffsetDeg = 1.0;
    constexpr uint32_t kSatsInPlane = 4;
    constexpr double kAltitudeMeters = 600000.0;
    constexpr double kCenterLatDeg = 45.6;
    constexpr double kCenterLonDeg = 84.9;

    const double baseRaanRad = LeoOrbitCalculator::DegToRad(84.9);

    const double semiMajorAxisMeters = LeoOrbitCalculator::kWgs84SemiMajorAxisMeters + kAltitudeMeters;
    const double inclinationRad = LeoOrbitCalculator::DegToRad(53.0);
    const double argPerigeeRad = 0.0;
    const double meanMotionRadPerSec =
        std::sqrt(LeoOrbitCalculator::kEarthGravitationalMu / std::pow(semiMajorAxisMeters, 3.0));

    for (uint32_t slotIdx = 0; slotIdx < kSatsInPlane; ++slotIdx)
    {
        const double plane1LegacyOverpassTime =
            kAlignmentReferenceTimeSeconds +
            (static_cast<double>(slotIdx) - 1.5) * kOverpassGapSeconds +
            kInterPlaneTimeOffsetSeconds;
        const double plane1CompensatedOverpassTime =
            ComputeSatelliteOverpassTime(kAlignmentReferenceTimeSeconds,
                                         kScenarioOverpassGapSeconds,
                                         0.0,
                                         kInterPlaneTimeOffsetSeconds,
                                         kPlane0TimeOffsetSeconds,
                                         kPlane1LegacyOverpassGapSeconds,
                                         1,
                                         slotIdx,
                                         kSatsInPlane);
        Require(std::abs(plane1CompensatedOverpassTime - plane1LegacyOverpassTime) < 1e-9,
                "plane1 legacy gap should preserve the old initial phase under scenario gap 6s");
    }
    Require(std::abs(ComputeSatelliteOverpassTime(kAlignmentReferenceTimeSeconds,
                                                  kScenarioOverpassGapSeconds,
                                                  0.0,
                                                  kInterPlaneTimeOffsetSeconds,
                                                  kPlane0TimeOffsetSeconds,
                                                  kPlane1LegacyOverpassGapSeconds,
                                                  0,
                                                  1,
                                                  kSatsInPlane) -
                     ComputeSatelliteOverpassTime(kAlignmentReferenceTimeSeconds,
                                                  kScenarioOverpassGapSeconds,
                                                  0.0,
                                                  kInterPlaneTimeOffsetSeconds,
                                                  kPlane0TimeOffsetSeconds,
                                                  kPlane1LegacyOverpassGapSeconds,
                                                  0,
                                                  0,
                                                  kSatsInPlane) -
                     kScenarioOverpassGapSeconds) < 1e-9,
            "plane0 should use the scenario 6s same-plane spacing");

    const auto ueGroundPoint = LeoOrbitCalculator::CreateGroundPoint(kCenterLatDeg, kCenterLonDeg, 0.0);
    const auto aligned = AutoAlignOrbitToUe(kAlignmentReferenceTimeSeconds,
                                            false,
                                            semiMajorAxisMeters,
                                            0.0,
                                            inclinationRad,
                                            argPerigeeRad,
                                            baseRaanRad,
                                            0.0,
                                            meanMotionRadPerSec,
                                            0.0,
                                            ueGroundPoint,
                                            2e9,
                                            LeoOrbitCalculator::DegToRad(10.0));
    const auto plane0BaseTrack = BuildPlaneGroundTrack(0,
                                                       kAlignmentReferenceTimeSeconds,
                                                       kOverpassGapSeconds,
                                                       kInterPlaneTimeOffsetSeconds,
                                                       kSatsInPlane,
                                                       semiMajorAxisMeters,
                                                       inclinationRad,
                                                       aligned.raanRad,
                                                       kInterPlaneRaanSpacingDeg,
                                                       0.0,
                                                       argPerigeeRad,
                                                       aligned.baseTrueAnomalyRad,
                                                       meanMotionRadPerSec,
                                                       kCenterLatDeg,
                                                       kCenterLonDeg);
    const auto plane0ShiftedTrack = BuildPlaneGroundTrack(0,
                                                          kAlignmentReferenceTimeSeconds,
                                                          kOverpassGapSeconds,
                                                          kInterPlaneTimeOffsetSeconds,
                                                          kSatsInPlane,
                                                          semiMajorAxisMeters,
                                                          inclinationRad,
                                                          aligned.raanRad,
                                                          kInterPlaneRaanSpacingDeg,
                                                          kPlane0RaanOffsetDeg,
                                                          argPerigeeRad,
                                                          aligned.baseTrueAnomalyRad,
                                                          meanMotionRadPerSec,
                                                          kCenterLatDeg,
                                                          kCenterLonDeg);
    const auto plane1BaseTrack = BuildPlaneGroundTrack(1,
                                                       kAlignmentReferenceTimeSeconds,
                                                       kOverpassGapSeconds,
                                                       kInterPlaneTimeOffsetSeconds,
                                                       kSatsInPlane,
                                                       semiMajorAxisMeters,
                                                       inclinationRad,
                                                       aligned.raanRad,
                                                       kInterPlaneRaanSpacingDeg,
                                                       0.0,
                                                       argPerigeeRad,
                                                       aligned.baseTrueAnomalyRad,
                                                       meanMotionRadPerSec,
                                                       kCenterLatDeg,
                                                       kCenterLonDeg);
    const auto plane1ShiftedTrack = BuildPlaneGroundTrack(1,
                                                          kAlignmentReferenceTimeSeconds,
                                                          kOverpassGapSeconds,
                                                          kInterPlaneTimeOffsetSeconds,
                                                          kSatsInPlane,
                                                          semiMajorAxisMeters,
                                                          inclinationRad,
                                                          aligned.raanRad,
                                                          kInterPlaneRaanSpacingDeg,
                                                          kPlane0RaanOffsetDeg,
                                                          argPerigeeRad,
                                                          aligned.baseTrueAnomalyRad,
                                                          meanMotionRadPerSec,
                                                          kCenterLatDeg,
                                                          kCenterLonDeg);

    const double plane0BaseNormal =
        MeanTrackNormalMeters(plane0BaseTrack, plane0BaseTrack, plane1BaseTrack);
    const double plane0ShiftedNormal =
        MeanTrackNormalMeters(plane0ShiftedTrack, plane0BaseTrack, plane1BaseTrack);
    Require(plane0ShiftedNormal < plane0BaseNormal - 10000.0,
            "positive plane0 RAAN offset should move the whole plane0 track south");

    const double plane1BaseNormal =
        MeanTrackNormalMeters(plane1BaseTrack, plane0BaseTrack, plane1BaseTrack);
    const double plane1ShiftedNormal =
        MeanTrackNormalMeters(plane1ShiftedTrack, plane0BaseTrack, plane1BaseTrack);
    Require(std::abs(plane1ShiftedNormal - plane1BaseNormal) < 1e-6,
            "plane1 track should remain unchanged");

    std::cout << "[TEST-PASS] plane0 dedicated RAAN offset shifts only the plane0 ground-track line"
              << std::endl;
    return 0;
}
