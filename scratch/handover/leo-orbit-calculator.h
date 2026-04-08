#ifndef LEO_ORBIT_CALCULATOR_H
#define LEO_ORBIT_CALCULATOR_H

/*
 * 文件说明：
 * `leo-orbit-calculator.h` 提供一个轻量的 LEO 轨道与观测几何计算器。
 *
 * 主要职责：
 * - 根据开普勒根数计算卫星在给定时刻的位置与速度；
 * - 在 ECI 与 ECEF 之间进行转换；
 * - 计算卫星相对地面 UE 的斜距、方位角、仰角和多普勒。
 *
 * 这个类是当前基础组物理场景构造的核心几何工具。
 */

#include "ns3/vector.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ns3
{

class LeoOrbitCalculator
{
  public:
    /** 用于描述卫星轨道的开普勒根数。 */
    struct KeplerElements
    {
        /** 轨道半长轴，单位米。 */
        double semiMajorAxisMeters;

        /** 轨道偏心率。 */
        double eccentricity;

        /** 轨道倾角，单位弧度。 */
        double inclinationRad;

        /** 升交点赤经，单位弧度。 */
        double raanRad;

        /** 近地点幅角，单位弧度。 */
        double argPerigeeRad;

        /** 仿真纪元时刻对应的真近点角，单位弧度。 */
        double trueAnomalyAtEpochRad;
    };

    /** 地面点的地理与 ECEF 表示。 */
    struct GroundPoint
    {
        /** 纬度，单位弧度。 */
        double latitudeRad;

        /** 经度，单位弧度。 */
        double longitudeRad;

        /** 海拔高度，单位米。 */
        double altitudeMeters;

        /** 对应的 ECEF 坐标。 */
        Vector ecef;
    };

    /** 卫星相对 UE 的完整几何状态。 */
    struct OrbitState
    {
        /** 卫星在 ECEF 坐标系下的位置。 */
        Vector ecef;

        /** 卫星在 ECEF 坐标系下的速度。 */
        Vector vEcef;

        /** 斜距长度，单位米。 */
        double slantRangeMeters;

        /** 方位角，单位弧度。 */
        double azimuthRad;

        /** 仰角，单位弧度。 */
        double elevationRad;

        /** 多普勒频移，单位 Hz。 */
        double dopplerHz;

        /** 是否高于最小可见仰角门限。 */
        bool visible;
    };

    /** 圆周率。 */
    static constexpr double kPi = 3.14159265358979323846;
    /** 地球自转角速度，单位 rad/s。 */
    static constexpr double kEarthRotationRateRadPerSec = 7.2921150e-5;
    /** 地球引力常数 GM。 */
    static constexpr double kEarthGravitationalMu = 3.986004418e14;
    /** WGS84 长半轴。 */
    static constexpr double kWgs84SemiMajorAxisMeters = 6378137.0;
    /** WGS84 扁率。 */
    static constexpr double kWgs84Flattening = 1.0 / 298.257223563;
    /** WGS84 第一偏心率平方。 */
    static constexpr double kWgs84FirstEccentricitySquared =
        kWgs84Flattening * (2.0 - kWgs84Flattening);
    /** 光速。 */
    static constexpr double kSpeedOfLight = 299792458.0;

    /** 角度转弧度。 */
    static double
    DegToRad(double deg)
    {
        return deg * kPi / 180.0;
    }

    /** 弧度转角度。 */
    static double
    RadToDeg(double rad)
    {
        return rad * 180.0 / kPi;
    }

    /** 将角度归一化到 `[-pi, pi]`。 */
    static double
    NormalizeAngle(double angleRad)
    {
        while (angleRad > kPi)
        {
            angleRad -= 2.0 * kPi;
        }
        while (angleRad < -kPi)
        {
            angleRad += 2.0 * kPi;
        }
        return angleRad;
    }

    /** 根据经纬高构造一个地面点对象。 */
    static GroundPoint
    CreateGroundPoint(double latitudeDeg, double longitudeDeg, double altitudeMeters)
    {
        GroundPoint point;
        point.latitudeRad = DegToRad(latitudeDeg);
        point.longitudeRad = DegToRad(longitudeDeg);
        point.altitudeMeters = altitudeMeters;
        point.ecef = GeographicToEcef(point.latitudeRad, point.longitudeRad, altitudeMeters);
        return point;
    }

    /**
     * 只计算给定时刻的卫星轨道状态，不引入任何 UE 观测几何量。
     */
    static OrbitState
    CalculateSatelliteState(double tSeconds, const KeplerElements& elements, double gmstAtEpochRad)
    {
        const double e = std::clamp(elements.eccentricity, 0.0, 0.999999);
        const double n = std::sqrt(kEarthGravitationalMu / std::pow(elements.semiMajorAxisMeters, 3.0));

        const double eccentricAnomaly0 = TrueToEccentricAnomaly(elements.trueAnomalyAtEpochRad, e);
        const double meanAnomaly0 = eccentricAnomaly0 - e * std::sin(eccentricAnomaly0);
        const double meanAnomaly = meanAnomaly0 + n * tSeconds;
        const double eccentricAnomaly = SolveKeplerEquation(meanAnomaly, e);
        const double trueAnomaly = EccentricToTrueAnomaly(eccentricAnomaly, e);

        const double radiusMeters =
            elements.semiMajorAxisMeters * (1.0 - e * std::cos(eccentricAnomaly));
        const Vector rPqw(radiusMeters * std::cos(trueAnomaly), radiusMeters * std::sin(trueAnomaly), 0.0);

        const double semiLatusRectum = elements.semiMajorAxisMeters * (1.0 - e * e);
        const double velocityScale = std::sqrt(kEarthGravitationalMu / semiLatusRectum);
        const Vector vPqw(-velocityScale * std::sin(trueAnomaly),
                          velocityScale * (e + std::cos(trueAnomaly)),
                          0.0);

        const Vector rEci = PerifocalToEci(rPqw, elements);
        const Vector vEci = PerifocalToEci(vPqw, elements);

        const double thetaG = gmstAtEpochRad + kEarthRotationRateRadPerSec * tSeconds;
        const double cosG = std::cos(thetaG);
        const double sinG = std::sin(thetaG);

        const Vector rEcef(cosG * rEci.x + sinG * rEci.y, -sinG * rEci.x + cosG * rEci.y, rEci.z);

        const Vector rotatedVEcef(cosG * vEci.x + sinG * vEci.y,
                                  -sinG * vEci.x + cosG * vEci.y,
                                  vEci.z);
        const Vector vEcef(rotatedVEcef.x + kEarthRotationRateRadPerSec * rEcef.y,
                           rotatedVEcef.y - kEarthRotationRateRadPerSec * rEcef.x,
                           rotatedVEcef.z);

        OrbitState state;
        state.ecef = rEcef;
        state.vEcef = vEcef;
        state.slantRangeMeters = 0.0;
        state.azimuthRad = 0.0;
        state.elevationRad = -kPi / 2.0;
        state.dopplerHz = 0.0;
        state.visible = false;
        return state;
    }

    /**
     * 在已知卫星公共运动状态的前提下，补充相对某个 UE 的观测几何量。
     */
    static OrbitState
    CalculateObservation(const OrbitState& satelliteState,
                         const GroundPoint& uePoint,
                         double carrierFrequencyHz,
                         double minElevationRad)
    {
        const Vector slant(satelliteState.ecef.x - uePoint.ecef.x,
                           satelliteState.ecef.y - uePoint.ecef.y,
                           satelliteState.ecef.z - uePoint.ecef.z);
        const double slantRangeMeters = Norm(slant);
        const Vector losUnit =
            (slantRangeMeters > 0.0)
                ? Vector(slant.x / slantRangeMeters, slant.y / slantRangeMeters, slant.z / slantRangeMeters)
                : Vector(0.0, 0.0, 0.0);

        const Vector enu = EcefDeltaToEnu(slant, uePoint.latitudeRad, uePoint.longitudeRad);
        const double elevationRad =
            std::asin(std::clamp(enu.z / std::max(slantRangeMeters, 1e-9), -1.0, 1.0));
        double azimuthRad = std::atan2(enu.x, enu.y);
        if (azimuthRad < 0.0)
        {
            azimuthRad += 2.0 * kPi;
        }

        const double rangeRate = Dot(satelliteState.vEcef, losUnit);
        const double dopplerHz = -(carrierFrequencyHz / kSpeedOfLight) * rangeRate;

        OrbitState state = satelliteState;
        state.slantRangeMeters = slantRangeMeters;
        state.azimuthRad = azimuthRad;
        state.elevationRad = elevationRad;
        state.dopplerHz = dopplerHz;
        state.visible = (elevationRad >= minElevationRad);
        return state;
    }

    /**
     * 计算给定时刻的卫星轨道状态以及相对 UE 的观测几何量。
     */
    static OrbitState
    Calculate(double tSeconds,
              const KeplerElements& elements,
              double gmstAtEpochRad,
              const GroundPoint& uePoint,
              double carrierFrequencyHz,
              double minElevationRad)
    {
        return CalculateObservation(
            CalculateSatelliteState(tSeconds, elements, gmstAtEpochRad),
            uePoint,
            carrierFrequencyHz,
            minElevationRad);
    }

  private:
    /** 地理坐标转 ECEF。 */
    static Vector
    GeographicToEcef(double latitudeRad, double longitudeRad, double altitudeMeters)
    {
        const double sinLat = std::sin(latitudeRad);
        const double cosLat = std::cos(latitudeRad);
        const double sinLon = std::sin(longitudeRad);
        const double cosLon = std::cos(longitudeRad);

        const double primeVerticalRadius =
            kWgs84SemiMajorAxisMeters / std::sqrt(1.0 - kWgs84FirstEccentricitySquared * sinLat * sinLat);

        const double x = (primeVerticalRadius + altitudeMeters) * cosLat * cosLon;
        const double y = (primeVerticalRadius + altitudeMeters) * cosLat * sinLon;
        const double z =
            (primeVerticalRadius * (1.0 - kWgs84FirstEccentricitySquared) + altitudeMeters) * sinLat;
        return Vector(x, y, z);
    }

    /** 将轨道平面坐标系向量旋转到 ECI 坐标系。 */
    static Vector
    PerifocalToEci(const Vector& pqw, const KeplerElements& elements)
    {
        const Vector afterArg = RotateZ(pqw, -elements.argPerigeeRad);
        const Vector afterInc = RotateX(afterArg, -elements.inclinationRad);
        return RotateZ(afterInc, -elements.raanRad);
    }

    /** 将 ECEF 差分向量转换到 ENU 本地坐标系。 */
    static Vector
    EcefDeltaToEnu(const Vector& deltaEcef, double latitudeRad, double longitudeRad)
    {
        const double sinLat = std::sin(latitudeRad);
        const double cosLat = std::cos(latitudeRad);
        const double sinLon = std::sin(longitudeRad);
        const double cosLon = std::cos(longitudeRad);

        const double east = -sinLon * deltaEcef.x + cosLon * deltaEcef.y;
        const double north =
            -sinLat * cosLon * deltaEcef.x - sinLat * sinLon * deltaEcef.y + cosLat * deltaEcef.z;
        const double up =
            cosLat * cosLon * deltaEcef.x + cosLat * sinLon * deltaEcef.y + sinLat * deltaEcef.z;
        return Vector(east, north, up);
    }

    /** 绕 X 轴旋转。 */
    static Vector
    RotateX(const Vector& v, double angleRad)
    {
        const double c = std::cos(angleRad);
        const double s = std::sin(angleRad);
        return Vector(v.x, c * v.y - s * v.z, s * v.y + c * v.z);
    }

    /** 绕 Z 轴旋转。 */
    static Vector
    RotateZ(const Vector& v, double angleRad)
    {
        const double c = std::cos(angleRad);
        const double s = std::sin(angleRad);
        return Vector(c * v.x - s * v.y, s * v.x + c * v.y, v.z);
    }

    /** 真近点角转偏近点角。 */
    static double
    TrueToEccentricAnomaly(double trueAnomalyRad, double eccentricity)
    {
        const double sinHalf = std::sin(trueAnomalyRad / 2.0);
        const double cosHalf = std::cos(trueAnomalyRad / 2.0);
        return 2.0 * std::atan2(std::sqrt(1.0 - eccentricity) * sinHalf,
                                std::sqrt(1.0 + eccentricity) * cosHalf);
    }

    /** 偏近点角转真近点角。 */
    static double
    EccentricToTrueAnomaly(double eccentricAnomalyRad, double eccentricity)
    {
        const double sinHalf = std::sin(eccentricAnomalyRad / 2.0);
        const double cosHalf = std::cos(eccentricAnomalyRad / 2.0);
        return 2.0 * std::atan2(std::sqrt(1.0 + eccentricity) * sinHalf,
                                std::sqrt(1.0 - eccentricity) * cosHalf);
    }

    /** 使用牛顿迭代求解开普勒方程。 */
    static double
    SolveKeplerEquation(double meanAnomalyRad, double eccentricity)
    {
        double eAnomaly = meanAnomalyRad;
        for (uint32_t iter = 0; iter < 12; ++iter)
        {
            const double f = eAnomaly - eccentricity * std::sin(eAnomaly) - meanAnomalyRad;
            const double fp = 1.0 - eccentricity * std::cos(eAnomaly);
            eAnomaly -= f / fp;
        }
        return eAnomaly;
    }

    /** 计算点积。 */
    static double
    Dot(const Vector& a, const Vector& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    /** 计算模长。 */
    static double
    Norm(const Vector& v)
    {
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }
};

} // namespace ns3

#endif // LEO_ORBIT_CALCULATOR_H
