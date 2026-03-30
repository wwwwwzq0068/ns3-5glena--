#ifndef WGS84_HEX_GRID_H
#define WGS84_HEX_GRID_H

/*
 * 文件说明：
 * `wgs84-hex-grid.h` 用于在 WGS84 地球模型上构建一个局部六边形网格。
 *
 * 当前场景中，这个网格主要用于：
 * - 给卫星波束提供稳定的地面锚点；
 * - 分析卫星星下点与地面小区的对应关系；
 * - 为后续可视化和更复杂的小区/负载建模做准备。
 */

#include "ns3/vector.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace ns3
{

constexpr double kWgs84HexPi = 3.14159265358979323846;

struct Wgs84HexGridCell
{
    /** 网格编号，从 1 开始递增。 */
    uint32_t id = 0;

    /** 网格中心纬度，单位度。 */
    double latitudeDeg = 0.0;

    /** 网格中心经度，单位度。 */
    double longitudeDeg = 0.0;

    /** 网格中心海拔，单位米。 */
    double altitudeMeters = 0.0;

    /** 相对网格中心原点的东向偏移，单位米。 */
    double eastMeters = 0.0;

    /** 相对网格中心原点的北向偏移，单位米。 */
    double northMeters = 0.0;

    /** 网格中心的 ECEF 坐标。 */
    Vector ecef;
};

/** 角度转弧度。 */
inline double
Wgs84DegToRad(double deg)
{
    return deg * kWgs84HexPi / 180.0;
}

/** 弧度转角度。 */
inline double
Wgs84RadToDeg(double rad)
{
    return rad * 180.0 / kWgs84HexPi;
}

/** WGS84 地理坐标转 ECEF 坐标。 */
inline Vector
Wgs84GeodeticToEcef(double latitudeDeg, double longitudeDeg, double altitudeMeters)
{
    constexpr double a = 6378137.0;
    constexpr double f = 1.0 / 298.257223563;
    constexpr double e2 = f * (2.0 - f);

    const double lat = Wgs84DegToRad(latitudeDeg);
    const double lon = Wgs84DegToRad(longitudeDeg);
    const double sinLat = std::sin(lat);
    const double cosLat = std::cos(lat);
    const double sinLon = std::sin(lon);
    const double cosLon = std::cos(lon);

    const double n = a / std::sqrt(1.0 - e2 * sinLat * sinLat);
    const double x = (n + altitudeMeters) * cosLat * cosLon;
    const double y = (n + altitudeMeters) * cosLat * sinLon;
    const double z = (n * (1.0 - e2) + altitudeMeters) * sinLat;
    return Vector(x, y, z);
}

/**
 * 将卫星的径向方向投影到 WGS84 地球表面，得到星下点。
 */
inline Vector
ProjectNadirToWgs84(const Vector& satEcef)
{
    constexpr double a = 6378137.0;
    constexpr double f = 1.0 / 298.257223563;
    constexpr double b = a * (1.0 - f);

    const double satNorm = std::sqrt(satEcef.x * satEcef.x + satEcef.y * satEcef.y + satEcef.z * satEcef.z);
    if (satNorm <= 0.0)
    {
        return Vector(0.0, 0.0, 0.0);
    }

    const double ux = satEcef.x / satNorm;
    const double uy = satEcef.y / satNorm;
    const double uz = satEcef.z / satNorm;

    const double denom = (ux * ux + uy * uy) / (a * a) + (uz * uz) / (b * b);
    if (denom <= 0.0)
    {
        return Vector(0.0, 0.0, 0.0);
    }

    const double t = 1.0 / std::sqrt(denom);
    return Vector(t * ux, t * uy, t * uz);
}

/**
 * 将局部平面中的东向/北向偏移近似映射为经纬度偏移。
 */
inline std::pair<double, double>
OffsetMetersToLatLonDeg(double centerLatDeg, double centerLonDeg, double eastMeters, double northMeters)
{
    constexpr double a = 6378137.0;
    constexpr double f = 1.0 / 298.257223563;
    constexpr double e2 = f * (2.0 - f);

    const double lat0 = Wgs84DegToRad(centerLatDeg);
    const double lon0 = Wgs84DegToRad(centerLonDeg);
    const double sinLat0 = std::sin(lat0);
    const double cosLat0 = std::max(1e-9, std::cos(lat0));

    const double denom = std::max(1e-9, 1.0 - e2 * sinLat0 * sinLat0);
    const double n = a / std::sqrt(denom);
    const double m = a * (1.0 - e2) / std::pow(denom, 1.5);

    const double lat = lat0 + northMeters / std::max(1e-9, m);
    double lon = lon0 + eastMeters / std::max(1e-9, n * cosLat0);

    while (lon > kWgs84HexPi)
    {
        lon -= 2.0 * kWgs84HexPi;
    }
    while (lon < -kWgs84HexPi)
    {
        lon += 2.0 * kWgs84HexPi;
    }

    return {Wgs84RadToDeg(lat), Wgs84RadToDeg(lon)};
}

/**
 * 在给定区域内构建平顶六边形网格。
 */
inline std::vector<Wgs84HexGridCell>
BuildWgs84HexGrid(double centerLatDeg,
                  double centerLonDeg,
                  double regionWidthMeters,
                  double regionHeightMeters,
                  double hexRadiusMeters)
{
    std::vector<Wgs84HexGridCell> cells;
    if (regionWidthMeters <= 0.0 || regionHeightMeters <= 0.0 || hexRadiusMeters <= 0.0)
    {
        return cells;
    }

    const double dx = std::sqrt(3.0) * hexRadiusMeters;
    const double dy = 1.5 * hexRadiusMeters;
    const double halfW = 0.5 * regionWidthMeters;
    const double halfH = 0.5 * regionHeightMeters;

    const int rowMax = static_cast<int>(std::ceil(halfH / dy)) + 1;
    const int colMax = static_cast<int>(std::ceil(halfW / dx)) + 1;

    uint32_t id = 1;
    for (int row = -rowMax; row <= rowMax; ++row)
    {
        const double north = row * dy;
        if (std::abs(north) > halfH)
        {
            continue;
        }

        const bool oddRow = (std::abs(row) % 2) == 1;
        const double rowOffset = oddRow ? (0.5 * dx) : 0.0;

        for (int col = -colMax; col <= colMax; ++col)
        {
            const double east = col * dx + rowOffset;
            if (std::abs(east) > halfW)
            {
                continue;
            }

            const auto latLon = OffsetMetersToLatLonDeg(centerLatDeg, centerLonDeg, east, north);
            Wgs84HexGridCell cell;
            cell.id = id++;
            cell.latitudeDeg = latLon.first;
            cell.longitudeDeg = latLon.second;
            cell.altitudeMeters = 0.0;
            cell.eastMeters = east;
            cell.northMeters = north;
            cell.ecef = Wgs84GeodeticToEcef(cell.latitudeDeg, cell.longitudeDeg, 0.0);
            cells.push_back(cell);
        }
    }

    return cells;
}

/**
 * 在已有网格中搜索与给定 ECEF 点最近的 `k` 个网格单元。
 */
inline std::vector<uint32_t>
FindNearestHexCellIndices(const std::vector<Wgs84HexGridCell>& cells, const Vector& pointEcef, uint32_t k)
{
    std::vector<uint32_t> indices;
    if (cells.empty() || k == 0)
    {
        return indices;
    }

    std::vector<std::pair<double, uint32_t>> ranked;
    ranked.reserve(cells.size());
    for (uint32_t i = 0; i < cells.size(); ++i)
    {
        const double dx = cells[i].ecef.x - pointEcef.x;
        const double dy = cells[i].ecef.y - pointEcef.y;
        const double dz = cells[i].ecef.z - pointEcef.z;
        ranked.emplace_back(dx * dx + dy * dy + dz * dz, i);
    }

    const uint32_t take = std::min<uint32_t>(k, static_cast<uint32_t>(ranked.size()));
    if (take < ranked.size())
    {
        std::nth_element(ranked.begin(), ranked.begin() + take, ranked.end());
        std::sort(ranked.begin(), ranked.begin() + take);
    }
    else
    {
        std::sort(ranked.begin(), ranked.end());
    }

    indices.reserve(take);
    for (uint32_t i = 0; i < take; ++i)
    {
        indices.push_back(ranked[i].second);
    }
    return indices;
}

} // namespace ns3

#endif // WGS84_HEX_GRID_H
