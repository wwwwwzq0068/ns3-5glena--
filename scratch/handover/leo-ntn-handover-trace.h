#ifndef LEO_NTN_HANDOVER_TRACE_H
#define LEO_NTN_HANDOVER_TRACE_H

/*
 * 文件说明：
 * `leo-ntn-handover-trace.h` 负责集中保存主脚本中的 CSV trace 写行逻辑。
 *
 * 设计目标：
 * - 让主脚本只决定何时采样，而不展开每个 trace 的列写入细节；
 * - 保持现有 CSV 字段和输出条件不变；
 * - 后续新增 trace 时优先放到这里，避免继续拉长主流程。
 */

#include "leo-ntn-handover-runtime.h"
#include "leo-orbit-calculator.h"
#include "wgs84-hex-grid.h"
#include "ns3/mobility-module.h"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <unordered_map>
#include <vector>

namespace ns3
{

inline double
TraceDistanceMeters(const Vector& a, const Vector& b)
{
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    const double dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

inline const Wgs84HexGridCell*
LookupTraceHexGridCellById(const std::vector<Wgs84HexGridCell>& cells,
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

inline Vector
EcefDeltaToTraceGridCenterEnu(const Vector& deltaEcef,
                              double gridCenterLatitudeDeg,
                              double gridCenterLongitudeDeg)
{
    const double latRad = LeoOrbitCalculator::DegToRad(gridCenterLatitudeDeg);
    const double lonRad = LeoOrbitCalculator::DegToRad(gridCenterLongitudeDeg);
    const double sinLat = std::sin(latRad);
    const double cosLat = std::cos(latRad);
    const double sinLon = std::sin(lonRad);
    const double cosLon = std::cos(lonRad);

    const double east = -sinLon * deltaEcef.x + cosLon * deltaEcef.y;
    const double north =
        -sinLat * cosLon * deltaEcef.x - sinLat * sinLon * deltaEcef.y + cosLat * deltaEcef.z;
    const double up =
        cosLat * cosLon * deltaEcef.x + cosLat * sinLon * deltaEcef.y + sinLat * deltaEcef.z;
    return Vector(east, north, up);
}

inline LeoOrbitCalculator::GroundPoint
BuildTraceGroundPointFromEcef(const Vector& pointEcef,
                              double gridCenterLatitudeDeg,
                              double gridCenterLongitudeDeg)
{
    const Vector centerEcef = Wgs84GeodeticToEcef(gridCenterLatitudeDeg, gridCenterLongitudeDeg, 0.0);
    const Vector delta(pointEcef.x - centerEcef.x,
                       pointEcef.y - centerEcef.y,
                       pointEcef.z - centerEcef.z);
    const Vector enu =
        EcefDeltaToTraceGridCenterEnu(delta, gridCenterLatitudeDeg, gridCenterLongitudeDeg);
    const auto latLon =
        OffsetMetersToLatLonDeg(gridCenterLatitudeDeg, gridCenterLongitudeDeg, enu.x, enu.y);
    return LeoOrbitCalculator::CreateGroundPoint(latLon.first, latLon.second, 0.0);
}

inline std::optional<uint32_t>
FindNearestTraceGridCellId(const std::vector<Wgs84HexGridCell>& cells, const Vector& pointEcef)
{
    const auto nearestIndices = FindNearestHexCellIndices(cells, pointEcef, 1);
    if (nearestIndices.empty())
    {
        return std::nullopt;
    }
    return cells[nearestIndices.front()].id;
}

template <typename BeamTargetResolver>
inline void
FlushSatelliteAnchorAndGroundTrackTraceRows(
    double nowSeconds,
    const std::vector<SatelliteRuntime>& satellites,
    const std::vector<Wgs84HexGridCell>& hexGridCells,
    const std::unordered_map<uint32_t, size_t>& hexGridCellIndexById,
    double gridCenterLatitudeDeg,
    double gridCenterLongitudeDeg,
    BeamTargetResolver&& resolveSatelliteBeamTargetEcef,
    std::ofstream& satAnchorTrace,
    std::ofstream& satGroundTrackTrace)
{
    if (!satAnchorTrace.is_open() && !satGroundTrackTrace.is_open())
    {
        return;
    }

    const Vector centerEcef = Wgs84GeodeticToEcef(gridCenterLatitudeDeg, gridCenterLongitudeDeg, 0.0);
    for (uint32_t satIdx = 0; satIdx < satellites.size(); ++satIdx)
    {
        const auto& sat = satellites[satIdx];
        uint32_t anchorGridId = sat.currentAnchorGridId;
        double anchorLatDeg = std::numeric_limits<double>::quiet_NaN();
        double anchorLonDeg = std::numeric_limits<double>::quiet_NaN();
        double anchorEastMeters = std::numeric_limits<double>::quiet_NaN();
        double anchorNorthMeters = std::numeric_limits<double>::quiet_NaN();

        Ptr<MobilityModel> satMobility = sat.node ? sat.node->GetObject<MobilityModel>() : nullptr;
        if (satMobility)
        {
            const Vector satEcef = satMobility->GetPosition();
            const Vector beamTargetEcef = resolveSatelliteBeamTargetEcef(satIdx, satEcef);
            const Wgs84HexGridCell* beamGridCell = nullptr;
            if (const auto beamGridId = FindNearestTraceGridCellId(hexGridCells, beamTargetEcef))
            {
                anchorGridId = beamGridId.value();
                beamGridCell =
                    LookupTraceHexGridCellById(hexGridCells, hexGridCellIndexById, anchorGridId);
            }

            if (beamGridCell != nullptr &&
                TraceDistanceMeters(beamTargetEcef, beamGridCell->ecef) <= 1.0)
            {
                anchorLatDeg = beamGridCell->latitudeDeg;
                anchorLonDeg = beamGridCell->longitudeDeg;
                anchorEastMeters = beamGridCell->eastMeters;
                anchorNorthMeters = beamGridCell->northMeters;
            }
            else
            {
                const auto beamGroundPoint = BuildTraceGroundPointFromEcef(
                    beamTargetEcef, gridCenterLatitudeDeg, gridCenterLongitudeDeg);
                anchorLatDeg = LeoOrbitCalculator::RadToDeg(beamGroundPoint.latitudeRad);
                anchorLonDeg = LeoOrbitCalculator::RadToDeg(beamGroundPoint.longitudeRad);

                const Vector delta(beamTargetEcef.x - centerEcef.x,
                                   beamTargetEcef.y - centerEcef.y,
                                   beamTargetEcef.z - centerEcef.z);
                const Vector enu =
                    EcefDeltaToTraceGridCenterEnu(delta, gridCenterLatitudeDeg, gridCenterLongitudeDeg);
                anchorEastMeters = enu.x;
                anchorNorthMeters = enu.y;
            }

            if (satGroundTrackTrace.is_open())
            {
                const auto subpointGround =
                    BuildTraceGroundPointFromEcef(satEcef, gridCenterLatitudeDeg, gridCenterLongitudeDeg);
                const Vector satDelta(satEcef.x - centerEcef.x,
                                      satEcef.y - centerEcef.y,
                                      satEcef.z - centerEcef.z);
                const Vector satEnu =
                    EcefDeltaToTraceGridCenterEnu(satDelta, gridCenterLatitudeDeg, gridCenterLongitudeDeg);
                satGroundTrackTrace << std::fixed << std::setprecision(3) << nowSeconds << ","
                                    << satIdx << "," << sat.orbitPlaneIndex << ","
                                    << sat.orbitSlotIndex << "," << sat.dev->GetCellId() << ","
                                    << std::setprecision(8)
                                    << LeoOrbitCalculator::RadToDeg(subpointGround.latitudeRad) << ","
                                    << LeoOrbitCalculator::RadToDeg(subpointGround.longitudeRad) << ","
                                    << std::setprecision(3) << satEnu.x << "," << satEnu.y << ","
                                    << satEcef.x << "," << satEcef.y << "," << satEcef.z << "\n";
            }
        }

        if (satAnchorTrace.is_open())
        {
            satAnchorTrace << std::fixed << std::setprecision(3) << nowSeconds << "," << satIdx
                           << "," << sat.orbitPlaneIndex << "," << sat.orbitSlotIndex << ","
                           << sat.dev->GetCellId() << "," << anchorGridId << ",";
            satAnchorTrace << std::setprecision(8) << anchorLatDeg << "," << anchorLonDeg << ",";
            satAnchorTrace << std::setprecision(3) << anchorEastMeters << "," << anchorNorthMeters
                           << "\n";
        }
    }
}

inline void
FlushSatelliteStateTraceRows(double nowSeconds,
                             const std::vector<SatelliteRuntime>& satellites,
                             std::ofstream& satelliteStateTrace)
{
    if (!satelliteStateTrace.is_open())
    {
        return;
    }

    for (uint32_t satIdx = 0; satIdx < satellites.size(); ++satIdx)
    {
        const auto& sat = satellites[satIdx];
        const uint16_t cellId = sat.dev ? sat.dev->GetCellId() : 0;
        satelliteStateTrace << std::fixed << std::setprecision(3) << nowSeconds << ","
                            << satIdx << "," << sat.orbitPlaneIndex << ","
                            << sat.orbitSlotIndex << "," << cellId << ","
                            << sat.currentAnchorGridId << "," << sat.attachedUeCount << ","
                            << sat.offeredPacketRate << "," << sat.loadScore << ","
                            << (sat.admissionAllowed ? 1 : 0) << "\n";
    }
}

template <typename SatelliteIndexResolver>
inline void
FlushHandoverDlThroughputTraceRows(double nowSeconds,
                                   double dtSeconds,
                                   uint32_t packetSizeBytes,
                                   std::vector<UeRuntime>& ues,
                                   SatelliteIndexResolver&& resolveSatelliteIndexFromCellId,
                                   std::ofstream& handoverThroughputTrace)
{
    if (!handoverThroughputTrace.is_open())
    {
        return;
    }

    for (uint32_t ueIdx = 0; ueIdx < ues.size(); ++ueIdx)
    {
        auto& ue = ues[ueIdx];
        const uint64_t currentRxPackets = ue.server ? ue.server->GetReceived() : 0;
        const uint64_t deltaPackets = currentRxPackets - ue.lastThroughputTraceRxPackets;
        ue.lastThroughputTraceRxPackets = currentRxPackets;

        uint16_t servingCellId = 0;
        if (ue.dev && ue.dev->GetRrc())
        {
            servingCellId = ue.dev->GetRrc()->GetCellId();
        }

        const double throughputMbps =
            (dtSeconds > 0.0) ? (deltaPackets * packetSizeBytes * 8.0) / dtSeconds / 1e6 : 0.0;

        handoverThroughputTrace << std::fixed << std::setprecision(3) << nowSeconds << ","
                                << ueIdx << "," << servingCellId << ","
                                << resolveSatelliteIndexFromCellId(servingCellId) << ","
                                << throughputMbps << "," << deltaPackets << ","
                                << currentRxPackets << "," << (ue.hasPendingHoStart ? 1 : 0)
                                << "," << ue.activeHandoverTraceId << ","
                                << ue.lastHoStartSourceCell << "," << ue.lastHoStartTargetCell
                                << "\n";
    }
}

} // namespace ns3

#endif // LEO_NTN_HANDOVER_TRACE_H
