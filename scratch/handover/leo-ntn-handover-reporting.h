#ifndef MYFIRST_REPORTING_H
#define MYFIRST_REPORTING_H

/*
 * 文件说明：
 * `leo-ntn-handover-reporting.h` 负责集中管理
 * `scratch/leo-ntn-handover-baseline.cc` 的最终统计汇总逻辑。
 *
 * 设计目的：
 * 1. 将“仿真运行期状态”与“结果统计输出”解耦；
 * 2. 让主脚本更聚焦于场景搭建、事件调度和切换流程；
 * 3. 统一基础组阶段常用的吞吐、几何校验和切换结果汇总格式。
 *
 * 当前本文件只处理控制台摘要输出，不负责事件级实时日志。
 */

#include "leo-ntn-handover-runtime.h"
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

    /** 全部 UE 的切换失败次数总和。 */
    uint32_t totalHoEndError = 0;

    /** 仿真结束前尚未闭合的切换次数。 */
    uint32_t totalHoUnresolved = 0;

    /** 全部成功切换的执行时延总和，单位秒。 */
    double totalHoDelaySeconds = 0.0;

    /** 已完成吞吐恢复判定的切换次数。 */
    uint32_t totalHoRecovered = 0;

    /** 全部已完成恢复判定的吞吐恢复时间总和，单位秒。 */
    double totalHoRecoverySeconds = 0.0;

    /** 整体切换成功率，范围为 0 到 100。 */
    double overallSuccessRate = 0.0;

    /** 全部成功切换的平均执行时延，单位毫秒。 */
    double averageDelayMs = 0.0;

    /** 全部已完成恢复判定切换的平均吞吐恢复时间，单位毫秒。 */
    double averageRecoveryMs = 0.0;

    /** 全部 UE 识别到的短时回切总数。 */
    uint32_t totalPingPongCount = 0;

    /** 失败原因明细：无专用 preamble。 */
    uint32_t totalFailureNoPreamble = 0;

    /** 失败原因明细：达到最大 RACH 尝试次数。 */
    uint32_t totalFailureMaxRach = 0;

    /** 失败原因明细：source leaving timeout。 */
    uint32_t totalFailureLeaving = 0;

    /** 失败原因明细：target joining timeout。 */
    uint32_t totalFailureJoining = 0;

    /** 失败原因明细：未知或未捕获。 */
    uint32_t totalFailureUnknown = 0;
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
        out.totalHoEndError += ue.handoverEndErrorCount;
        out.totalHoDelaySeconds += ue.totalHandoverExecutionDelaySeconds;
        out.totalHoRecovered += ue.throughputRecoveryCount;
        out.totalHoRecoverySeconds += ue.totalThroughputRecoverySeconds;
        out.totalPingPongCount += ue.pingPongCount;
        out.totalFailureNoPreamble += ue.handoverFailureNoPreambleCount;
        out.totalFailureMaxRach += ue.handoverFailureMaxRachCount;
        out.totalFailureLeaving += ue.handoverFailureLeavingCount;
        out.totalFailureJoining += ue.handoverFailureJoiningCount;
        out.totalFailureUnknown += ue.handoverFailureUnknownCount;
    }
    if (out.totalHoStart >= out.totalHoEndOk + out.totalHoEndError)
    {
        out.totalHoUnresolved = out.totalHoStart - out.totalHoEndOk - out.totalHoEndError;
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
    if (out.totalHoRecovered > 0)
    {
        out.averageRecoveryMs =
            out.totalHoRecoverySeconds * 1000.0 / static_cast<double>(out.totalHoRecovered);
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

    std::cout << "=== Research summary ===" << std::endl;
    std::cout << "Average DL throughput(total): " << std::fixed << std::setprecision(3)
              << aggregate.totalAverageDlThroughputMbps << " Mbps" << std::endl;
}

/**
 * 打印整体与分 UE 的切换汇总。
 */
inline void
PrintHandoverSummary(const std::vector<UeRuntime>& ues, double pingPongWindowSeconds)
{
    const HandoverAggregate aggregate = BuildHandoverAggregate(ues);
    std::cout << "Completed handovers: " << aggregate.totalHoEndOk << std::endl;
    if (aggregate.totalHoRecovered > 0)
    {
        std::cout << "Average throughput recovery time: " << std::fixed << std::setprecision(3)
                  << aggregate.averageRecoveryMs << " ms" << std::endl;
    }
    else
    {
        std::cout << "Average throughput recovery time: n/a" << std::endl;
    }
    std::cout << "Total ping-pong events: " << aggregate.totalPingPongCount
              << " (window=" << std::fixed << std::setprecision(3) << pingPongWindowSeconds
              << "s)" << std::endl;
}

} // namespace ns3

#endif // MYFIRST_REPORTING_H
