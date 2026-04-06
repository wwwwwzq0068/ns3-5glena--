#ifndef MYFIRST_UTILS_H
#define MYFIRST_UTILS_H

/*
 * 文件说明：
 * `leo-ntn-handover-utils.h` 存放
 * `leo-ntn-handover-baseline.cc` 使用的通用辅助工具函数。
 *
 * 这些内容的特点是：
 * - 偏计算或格式化；
 * - 与运行时对象生命周期弱耦合；
 * - 可以被场景初始化、预测分析和日志辅助逻辑复用。
 *
 * 本文件不负责保存运行时状态，也不直接承担最终统计输出。
 */

#include "beam-link-budget.h"
#include "leo-orbit-calculator.h"
#include "wgs84-hex-grid.h"
#include "ns3/core-module.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace ns3
{

struct PredictedHandover
{
    /** 预测切换的源卫星索引。 */
    uint32_t sourceIdx = 0;

    /** 预测切换的目标卫星索引。 */
    uint32_t targetIdx = 0;

    /** 预测触发时刻，单位秒。 */
    double triggerTimeSeconds = 0.0;
};

struct GridAnchorSelection
{
    /** 是否成功找到可用网格锚点。 */
    bool found = false;

    /** 当前选择的主锚点网格编号。 */
    uint32_t anchorGridId = 0;

    /** 主锚点网格中心的 ECEF 坐标。 */
    Vector anchorEcef;

    /** 与卫星星下点最近的 K 个网格编号。 */
    std::vector<uint32_t> nearestGridIds;
};

struct AutoAlignOrbitResult
{
    /** 自动对准后选出的升交点赤经。 */
    double raanRad = 0.0;

    /** 自动对准后选出的中心参考真近点角。 */
    double baseTrueAnomalyRad = 0.0;

    /** 搜索过程中得到的最佳峰值仰角。 */
    double peakElevationRad = -std::numeric_limits<double>::infinity();
};

/**
 * 估计从当前时刻起，某颗卫星还能为给定 UE 提供服务的剩余时间。
 *
 * 这里采用“先粗步长扫描，再二分逼近”的方式：
 * - 如果当前时刻已经不可见，结果直接为 0；
 * - 如果在给定搜索窗口内一直可见，返回搜索窗口上限；
 * - 否则返回首次跌出最小仰角门限之前的近似剩余时间。
 */
inline double
EstimateRemainingVisibleSeconds(double nowSeconds,
                                const LeoOrbitCalculator::KeplerElements& orbit,
                                const LeoOrbitCalculator::GroundPoint& ueGroundPoint,
                                double carrierFrequencyHz,
                                double minElevationRad,
                                double gmstAtEpochRad,
                                double maxLookaheadSeconds,
                                double coarseStepSeconds = 0.5)
{
    if (maxLookaheadSeconds <= 0.0)
    {
        return 0.0;
    }

    const double lookaheadLimit = std::max(0.0, maxLookaheadSeconds);
    const double stepSeconds = std::max(0.1, coarseStepSeconds);
    const auto isVisibleAt = [&](double deltaSeconds) {
        return LeoOrbitCalculator::Calculate(nowSeconds + deltaSeconds,
                                             orbit,
                                             gmstAtEpochRad,
                                             ueGroundPoint,
                                             carrierFrequencyHz,
                                             minElevationRad)
            .visible;
    };

    if (!isVisibleAt(0.0))
    {
        return 0.0;
    }

    double lowerBoundSeconds = 0.0;
    double upperBoundSeconds = std::min(stepSeconds, lookaheadLimit);

    while (upperBoundSeconds < lookaheadLimit - 1e-9 && isVisibleAt(upperBoundSeconds))
    {
        lowerBoundSeconds = upperBoundSeconds;
        upperBoundSeconds = std::min(upperBoundSeconds + stepSeconds, lookaheadLimit);
    }

    if (upperBoundSeconds >= lookaheadLimit - 1e-9 && isVisibleAt(upperBoundSeconds))
    {
        return lookaheadLimit;
    }

    for (uint32_t i = 0; i < 16; ++i)
    {
        const double middleSeconds = 0.5 * (lowerBoundSeconds + upperBoundSeconds);
        if (isVisibleAt(middleSeconds))
        {
            lowerBoundSeconds = middleSeconds;
        }
        else
        {
            upperBoundSeconds = middleSeconds;
        }
    }

    return lowerBoundSeconds;
}

/**
 * 将剩余可见时间映射为 [0,1] 的可见性效用。
 */
inline double
ComputeVisibilityUtility(double remainingVisibleSeconds, double scoreWindowSeconds)
{
    if (scoreWindowSeconds <= 0.0)
    {
        return 0.0;
    }
    return std::clamp(remainingVisibleSeconds / scoreWindowSeconds, 0.0, 1.0);
}

/**
 * 根据卫星当前位置，在六边形网格中选择用于波束锚定的网格单元。
 */
inline GridAnchorSelection
ComputeGridAnchorSelection(const std::vector<Wgs84HexGridCell>& cells,
                           const Vector& satEcef,
                           uint32_t nearestK)
{
    GridAnchorSelection out;
    if (cells.empty())
    {
        return out;
    }

    const Vector nadirPoint = ProjectNadirToWgs84(satEcef);
    const auto nearestIndices = FindNearestHexCellIndices(cells, nadirPoint, std::max<uint32_t>(1, nearestK));
    if (nearestIndices.empty())
    {
        return out;
    }

    out.found = true;
    out.anchorGridId = cells[nearestIndices.front()].id;
    out.anchorEcef = cells[nearestIndices.front()].ecef;
    out.nearestGridIds.reserve(nearestIndices.size());
    for (const auto idx : nearestIndices)
    {
        out.nearestGridIds.push_back(cells[idx].id);
    }
    return out;
}

/**
 * 将整数列表格式化成紧凑的 `[1,2,3]` 形式，便于日志输出。
 */
inline std::string
FormatUintList(const std::vector<uint32_t>& values)
{
    std::ostringstream oss;
    oss << "[";
    for (uint32_t i = 0; i < values.size(); ++i)
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

/**
 * 自动搜索一组更合适的轨道相位参数，使中心时刻过境更接近给定 UE。
 *
 * 该函数用于基础组场景快速对齐轨道几何，避免手工调整 RAAN 和初始真近点角。
 */
inline AutoAlignOrbitResult
AutoAlignOrbitToUe(double centerOverpassSeconds,
                   bool descendingPass,
                   double semiMajorAxisMeters,
                   double orbitEccentricity,
                   double inclinationRad,
                   double argPerigeeRad,
                   double initialRaanRad,
                   double initialBaseTrueAnomalyRad,
                   double meanMotionRadPerSec,
                   double gmstAtEpochRad,
                   const LeoOrbitCalculator::GroundPoint& ueGroundPoint,
                   double carrierFrequencyHz,
                   double minElevationRad)
{
    AutoAlignOrbitResult result;
    double bestScore = -std::numeric_limits<double>::infinity();
    result.raanRad = initialRaanRad;
    result.baseTrueAnomalyRad = initialBaseTrueAnomalyRad;

    const double sinLat = std::sin(ueGroundPoint.latitudeRad);
    const double cosLat = std::cos(ueGroundPoint.latitudeRad);
    const double sinLon = std::sin(ueGroundPoint.longitudeRad);
    const double cosLon = std::cos(ueGroundPoint.longitudeRad);
    const Vector northUnit(-sinLat * cosLon, -sinLat * sinLon, cosLat);
    const double preferredNorthSign = descendingPass ? -1.0 : 1.0;

    auto tryCandidate = [&](double candidateRaan, double candidateBaseTrueAnomaly) {
        LeoOrbitCalculator::KeplerElements trial;
        trial.semiMajorAxisMeters = semiMajorAxisMeters;
        trial.eccentricity = orbitEccentricity;
        trial.inclinationRad = inclinationRad;
        trial.raanRad = candidateRaan;
        trial.argPerigeeRad = argPerigeeRad;
        trial.trueAnomalyAtEpochRad =
            candidateBaseTrueAnomaly - meanMotionRadPerSec * centerOverpassSeconds;

        const auto state = LeoOrbitCalculator::Calculate(centerOverpassSeconds,
                                                         trial,
                                                         gmstAtEpochRad,
                                                         ueGroundPoint,
                                                         carrierFrequencyHz,
                                                         minElevationRad);

        const double northRate =
            northUnit.x * state.vEcef.x + northUnit.y * state.vEcef.y + northUnit.z * state.vEcef.z;
        const double score = state.elevationRad + preferredNorthSign * northRate * 1e-6;
        if (score > bestScore)
        {
            bestScore = score;
            result.peakElevationRad = state.elevationRad;
            result.raanRad = candidateRaan;
            result.baseTrueAnomalyRad = candidateBaseTrueAnomaly;
        }
    };

    for (double raanDeg = -180.0; raanDeg <= 180.0; raanDeg += 5.0)
    {
        for (double baseNuDeg = -180.0; baseNuDeg <= 180.0; baseNuDeg += 5.0)
        {
            tryCandidate(LeoOrbitCalculator::DegToRad(raanDeg), LeoOrbitCalculator::DegToRad(baseNuDeg));
        }
    }
    for (double deltaRaanDeg = -5.0; deltaRaanDeg <= 5.0; deltaRaanDeg += 0.5)
    {
        for (double deltaNuDeg = -5.0; deltaNuDeg <= 5.0; deltaNuDeg += 0.5)
        {
            tryCandidate(result.raanRad + LeoOrbitCalculator::DegToRad(deltaRaanDeg),
                         result.baseTrueAnomalyRad + LeoOrbitCalculator::DegToRad(deltaNuDeg));
        }
    }

    result.raanRad = LeoOrbitCalculator::NormalizeAngle(result.raanRad);
    result.baseTrueAnomalyRad = LeoOrbitCalculator::NormalizeAngle(result.baseTrueAnomalyRad);
    return result;
}

/**
 * 将生成的六边形网格目录导出为 CSV，便于后续可视化和核对。
 */
inline void
DumpHexGridCatalog(const std::string& path, const std::vector<Wgs84HexGridCell>& cells)
{
    std::ofstream out(path);
    if (!out.is_open())
    {
        std::cout << "[Grid] warning: failed to open catalog file: " << path << std::endl;
        return;
    }

    out << "id,latitude_deg,longitude_deg,altitude_m,east_m,north_m,ecef_x,ecef_y,ecef_z\n";
    out << std::fixed << std::setprecision(8);
    for (const auto& c : cells)
    {
        out << c.id << "," << c.latitudeDeg << "," << c.longitudeDeg << "," << c.altitudeMeters << ","
            << c.eastMeters << "," << c.northMeters << "," << c.ecef.x << "," << c.ecef.y << ","
            << c.ecef.z << "\n";
    }
}

/**
 * 拼接输出目录和文件名，避免主脚本重复处理路径细节。
 */
inline std::string
JoinOutputPath(const std::string& directory, const std::string& filename)
{
    return (std::filesystem::path(directory) / filename).string();
}

/**
 * 根据网格编号反查六边形网格单元。
 */
inline const Wgs84HexGridCell*
FindHexGridCellById(const std::vector<Wgs84HexGridCell>& cells, uint32_t id)
{
    for (const auto& cell : cells)
    {
        if (cell.id == id)
        {
            return &cell;
        }
    }
    return nullptr;
}

/**
 * 为输出文件创建父目录。
 *
 * 返回 `false` 表示目录创建失败，调用方应终止仿真并提示路径问题。
 */
inline bool
EnsureParentDirectoryForFile(const std::string& filePath)
{
    const std::filesystem::path path(filePath);
    const std::filesystem::path parent = path.parent_path();
    if (parent.empty())
    {
        return true;
    }

    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    return !ec;
}

/**
 * 在一组卫星候选中选择当前 RSRP 最优的卫星索引。
 */
inline uint32_t
FindBestRsrpSatellite(const std::vector<LeoOrbitCalculator::OrbitState>* states,
                      const std::vector<BeamLinkBudget>& budgets,
                      uint32_t skipSatIdx,
                      bool requireVisible,
                      bool requireFinite,
                      double* bestRsrpOut = nullptr)
{
    if (requireVisible && states == nullptr)
    {
        return std::numeric_limits<uint32_t>::max();
    }

    uint32_t bestIdx = std::numeric_limits<uint32_t>::max();
    double bestRsrp = -std::numeric_limits<double>::infinity();
    for (uint32_t i = 0; i < budgets.size(); ++i)
    {
        if (i == skipSatIdx)
        {
            continue;
        }
        const bool visibleOk = !requireVisible || (*states)[i].visible;
        const bool finiteOk = !requireFinite || std::isfinite(budgets[i].rsrpDbm);
        if (visibleOk && budgets[i].beamLocked && finiteOk && budgets[i].rsrpDbm > bestRsrp)
        {
            bestRsrp = budgets[i].rsrpDbm;
            bestIdx = i;
        }
    }

    if (bestRsrpOut != nullptr)
    {
        *bestRsrpOut = bestRsrp;
    }
    return bestIdx;
}

/**
 * 为预测切换逻辑解析一个稳定的波束锚点坐标。
 *
 * 如果启用了六边形网格，则优先使用网格中心；
 * 否则退回到调用方提供的后备锚点。
 */
inline Vector
ResolvePredictiveAnchorEcef(const Vector& satEcef,
                            const Vector& fallbackAnchorEcef,
                            bool useWgs84HexGrid,
                            const std::vector<Wgs84HexGridCell>& hexGridCells)
{
    if (!useWgs84HexGrid || hexGridCells.empty())
    {
        return fallbackAnchorEcef;
    }

    const Vector nadirPoint = ProjectNadirToWgs84(satEcef);
    const auto nearest = FindNearestHexCellIndices(hexGridCells, nadirPoint, 1);
    if (nearest.empty())
    {
        return fallbackAnchorEcef;
    }
    return hexGridCells[nearest.front()].ecef;
}

} // namespace ns3

#endif // MYFIRST_UTILS_H
