#ifndef MYFIRST_REPORTING_H
#define MYFIRST_REPORTING_H

/*
 * 文件说明：
 * `myfirst-reporting.h` 负责集中管理 `scratch/myfirst.cc` 的最终统计汇总逻辑。
 *
 * 设计目的：
 * 1. 将“仿真运行期状态”与“结果统计输出”解耦；
 * 2. 让主脚本更聚焦于场景搭建、事件调度和切换流程；
 * 3. 统一基础组阶段常用的吞吐、几何校验和切换结果汇总格式。
 *
 * 当前本文件只处理控制台摘要输出，不负责事件级实时日志。
 */

#include "myfirst-runtime.h"
#include <iomanip>
#include <iostream>
#include <vector>

namespace ns3
{

/**
 * 单个 UE 的最终下行统计结果。
 */
struct UeDlSummary
{
    /** UE 序号，与日志中的 `ue=` 保持一致。 */
    uint32_t ueIndex = 0;

    /** 仿真结束时 UDP Server 统计到的总接收包数。 */
    uint64_t rxPackets = 0;

    /** 该 UE 在业务持续时间内的平均下行吞吐，单位 Mbps。 */
    double averageDlThroughputMbps = 0.0;
};

/**
 * 所有 UE 的下行统计汇总。
 */
struct DlTrafficAggregate
{
    /** 每个 UE 的独立吞吐统计。 */
    std::vector<UeDlSummary> perUe;

    /** 所有 UE 接收到的总包数。 */
    uint64_t totalRxPackets = 0;

    /** 所有 UE 汇总后的平均总吞吐，单位 Mbps。 */
    double totalAverageDlThroughputMbps = 0.0;
};

/**
 * 切换统计的聚合结果。
 */
struct HandoverAggregate
{
    /** 全部 UE 的切换开始次数总和。 */
    uint32_t totalHoStart = 0;

    /** 全部 UE 的切换成功次数总和。 */
    uint32_t totalHoEndOk = 0;

    /** 全部成功切换的执行时延总和，单位秒。 */
    double totalHoDelaySeconds = 0.0;

    /** 整体切换成功率，范围为 0 到 100。 */
    double overallSuccessRate = 0.0;

    /** 全部成功切换的平均执行时延，单位毫秒。 */
    double averageDelayMs = 0.0;
};

/**
 * 根据 UE 运行时对象生成最终下行统计。
 */
inline DlTrafficAggregate
BuildDlTrafficAggregate(const std::vector<UeRuntime>& ues,
                        double appDurationSeconds,
                        uint32_t udpPacketSizeBytes)
{
    DlTrafficAggregate out;
    out.perUe.reserve(ues.size());
    for (uint32_t ueIdx = 0; ueIdx < ues.size(); ++ueIdx)
    {
        const auto& ue = ues[ueIdx];
        UeDlSummary summary;
        summary.ueIndex = ueIdx;
        summary.rxPackets = ue.server ? ue.server->GetReceived() : 0;
        summary.averageDlThroughputMbps =
            (appDurationSeconds > 0.0)
                ? (summary.rxPackets * udpPacketSizeBytes * 8.0 / appDurationSeconds / 1e6)
                : 0.0;
        out.totalRxPackets += summary.rxPackets;
        out.perUe.push_back(summary);
    }

    out.totalAverageDlThroughputMbps =
        (appDurationSeconds > 0.0)
            ? (out.totalRxPackets * udpPacketSizeBytes * 8.0 / appDurationSeconds / 1e6)
            : 0.0;
    return out;
}

/**
 * 汇总所有 UE 的切换次数、成功率与平均执行时延。
 */
inline HandoverAggregate
BuildHandoverAggregate(const std::vector<UeRuntime>& ues)
{
    HandoverAggregate out;
    for (const auto& ue : ues)
    {
        out.totalHoStart += ue.handoverStartCount;
        out.totalHoEndOk += ue.handoverEndOkCount;
        out.totalHoDelaySeconds += ue.totalHandoverExecutionDelaySeconds;
    }
    if (out.totalHoStart > 0)
    {
        out.overallSuccessRate =
            100.0 * static_cast<double>(out.totalHoEndOk) / static_cast<double>(out.totalHoStart);
    }
    if (out.totalHoEndOk > 0)
    {
        out.averageDelayMs = out.totalHoDelaySeconds * 1000.0 / static_cast<double>(out.totalHoEndOk);
    }
    return out;
}

/**
 * 打印所有 UE 的最终下行统计。
 */
inline void
PrintDlTrafficSummary(const std::vector<UeRuntime>& ues,
                      double appDurationSeconds,
                      uint32_t udpPacketSizeBytes)
{
    const DlTrafficAggregate aggregate =
        BuildDlTrafficAggregate(ues, appDurationSeconds, udpPacketSizeBytes);

    std::cout << "=== Final UE DL stats ===" << std::endl;
    for (const auto& summary : aggregate.perUe)
    {
        std::cout << "UE" << summary.ueIndex << ": packets=" << summary.rxPackets
                  << " avgThroughput=" << std::fixed << std::setprecision(3)
                  << summary.averageDlThroughputMbps << " Mbps" << std::endl;
    }
    std::cout << "Received packets(total): " << aggregate.totalRxPackets << std::endl;
    std::cout << "Average DL throughput: " << std::fixed << std::setprecision(3)
              << aggregate.totalAverageDlThroughputMbps << " Mbps (total)" << std::endl;
}

/**
 * 打印每颗卫星在仿真期间的最小斜距和对应时刻。
 *
 * 这部分信息主要用于检查几何场景是否合理，
 * 例如卫星过境顺序、最近时刻是否按预期递进，以及不同卫星的最小距离是否大体一致。
 */
inline void
PrintOverpassDistanceSummary(const std::vector<SatelliteRuntime>& satellites,
                             const std::vector<double>& minDistancesMeters,
                             const std::vector<double>& minDistanceTimesSeconds)
{
    std::cout << "=== Overpass distance summary ===" << std::endl;
    const uint32_t reportCount = std::min<uint32_t>(satellites.size(), minDistancesMeters.size());
    for (uint32_t satIdx = 0; satIdx < reportCount; ++satIdx)
    {
        const uint16_t cellId = satellites[satIdx].dev ? satellites[satIdx].dev->GetCellId() : 0;
        std::cout << "sat" << satIdx << " cell=" << cellId
                  << " minDistance=" << minDistancesMeters[satIdx] / 1000.0 << " km"
                  << " at t="
                  << ((satIdx < minDistanceTimesSeconds.size()) ? minDistanceTimesSeconds[satIdx] : 0.0)
                  << " s" << std::endl;
    }
}

/**
 * 打印整体与分 UE 的切换汇总。
 */
inline void
PrintHandoverSummary(const std::vector<UeRuntime>& ues)
{
    std::cout << "=== Handover summary ===" << std::endl;
    const HandoverAggregate aggregate = BuildHandoverAggregate(ues);
    std::cout << "Total HO start: " << aggregate.totalHoStart << std::endl;
    std::cout << "Total HO success: " << aggregate.totalHoEndOk << std::endl;
    std::cout << "Overall HO success rate: " << std::fixed << std::setprecision(1)
              << aggregate.overallSuccessRate << "%" << std::endl;
    std::cout << "Average HO execution delay: " << std::fixed << std::setprecision(3)
              << aggregate.averageDelayMs << " ms" << std::endl;

    for (uint32_t ueIdx = 0; ueIdx < ues.size(); ++ueIdx)
    {
        const auto& ue = ues[ueIdx];
        const double ueSuccessRate =
            (ue.handoverStartCount > 0)
                ? (100.0 * static_cast<double>(ue.handoverEndOkCount) / static_cast<double>(ue.handoverStartCount))
                : 0.0;
        const double ueAvgDelayMs =
            (ue.handoverEndOkCount > 0)
                ? (ue.totalHandoverExecutionDelaySeconds * 1000.0 / static_cast<double>(ue.handoverEndOkCount))
                : 0.0;

        std::cout << "UE" << ueIdx << ": ho=" << ue.handoverEndOkCount << "/" << ue.handoverStartCount
                  << " successRate=" << std::fixed << std::setprecision(1) << ueSuccessRate << "%"
                  << " avgDelay=" << std::fixed << std::setprecision(3) << ueAvgDelayMs << " ms";
        if (ue.hasPredictedHandover)
        {
            std::cout << " predicted sat" << ue.expectedSourceIndex << "->sat" << ue.expectedTargetIndex
                      << " success=" << (ue.seenExpectedHandover ? "YES" : "NO");
        }
        std::cout << std::endl;
    }
}

} // namespace ns3

#endif // MYFIRST_REPORTING_H
