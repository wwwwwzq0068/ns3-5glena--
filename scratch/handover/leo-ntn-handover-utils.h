#ifndef LEO_NTN_HANDOVER_UTILS_H
#define LEO_NTN_HANDOVER_UTILS_H

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
#include "leo-ntn-handover-output-lifecycle.h"
#include "leo-orbit-calculator.h"
#include "ns3/core-module.h"
#include "wgs84-hex-grid.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace ns3
{

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
 * 计算指定轨道面的升交点赤经。
 *
 * `plane0RaanOffsetDeg` 只作用在 plane 0，用来整体平移该轨道面的地面投影线。
 */
inline double
ComputePlaneRaanRad(double baseRaanRad,
                    double interPlaneRaanSpacingDeg,
                    double plane0RaanOffsetDeg,
                    uint32_t planeIdx)
{
    const double plane0OffsetRad =
        (planeIdx == 0) ? LeoOrbitCalculator::DegToRad(plane0RaanOffsetDeg) : 0.0;
    return LeoOrbitCalculator::NormalizeAngle(
        baseRaanRad + plane0OffsetRad +
        LeoOrbitCalculator::DegToRad(interPlaneRaanSpacingDeg) * static_cast<double>(planeIdx));
}

/**
 * 计算单颗卫星在初始轨道相位中使用的目标过境时间。
 *
 * `overpassGapSeconds` 是主场景的同轨相邻卫星时间间隔。`plane1OverpassGapSeconds`
 * 用于在主场景间隔调整后保持 plane 1 的初始诊断几何可复现。
 */
inline double
ComputeSatelliteOverpassTime(double alignmentReferenceTimeSeconds,
                             double overpassGapSeconds,
                             double overpassTimeOffsetSeconds,
                             double interPlaneTimeOffsetSeconds,
                             double plane0TimeOffsetSeconds,
                             double plane1OverpassGapSeconds,
                             uint32_t planeIdx,
                             uint32_t slotIdx,
                             uint32_t satsInPlane)
{
    const double planeCenterIndex = (static_cast<double>(satsInPlane) - 1.0) / 2.0;
    const double effectiveOverpassGapSeconds =
        (planeIdx == 1) ? plane1OverpassGapSeconds : overpassGapSeconds;
    const double planeTimeOffset =
        interPlaneTimeOffsetSeconds * static_cast<double>(planeIdx) +
        ((planeIdx == 0) ? plane0TimeOffsetSeconds : 0.0);

    return alignmentReferenceTimeSeconds +
           (static_cast<double>(slotIdx) - planeCenterIndex) * effectiveOverpassGapSeconds +
           overpassTimeOffsetSeconds + planeTimeOffset;
}

/**
 * 根据卫星当前位置，在六边形网格中选择用于波束锚定的网格单元。
 */
inline GridAnchorSelection
ComputeGridAnchorSelection(const std::vector<Wgs84HexGridCell>& cells,
                           const Vector& satEcef,
                           uint32_t nearestK,
                           const std::set<uint32_t>& blockedGridIds = {})
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

    out.nearestGridIds.reserve(nearestIndices.size());
    for (const auto idx : nearestIndices)
    {
        out.nearestGridIds.push_back(cells[idx].id);
        if (!out.found && blockedGridIds.find(cells[idx].id) == blockedGridIds.end())
        {
            out.found = true;
            out.anchorGridId = cells[idx].id;
            out.anchorEcef = cells[idx].ecef;
        }
    }
    return out;
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
    const bool written = WriteCsvFile(path, HandoverCsvHeaders::kHexGridCatalog, [&](std::ofstream& out) {
        out << std::fixed << std::setprecision(8);
        for (const auto& c : cells)
        {
            out << c.id << "," << c.latitudeDeg << "," << c.longitudeDeg << "," << c.altitudeMeters
                << "," << c.eastMeters << "," << c.northMeters << "," << c.ecef.x << ","
                << c.ecef.y << "," << c.ecef.z << "\n";
        }
    });
    if (!written)
    {
        std::cout << "[Grid] warning: failed to open catalog file: " << path << std::endl;
        return;
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
    return EnsureOutputParentDirectoryForFile(filePath);
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
} // namespace ns3

#endif // LEO_NTN_HANDOVER_UTILS_H
