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
#include "ns3/flow-monitor-module.h"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
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
 * 单个 UE 的端到端业务流统计结果。
 */
struct UeE2eFlowSummary
{
    /** UE 序号。 */
    uint32_t ueIndex = 0;

    /** 当前 UE 对应的下行业务端口。 */
    uint16_t dlPort = 0;

    /** 是否在 FlowMonitor 中找到匹配流。 */
    bool hasMatchedFlow = false;

    /** 源端累计发送包数。 */
    uint64_t txPackets = 0;

    /** UE 实际接收包数。 */
    uint64_t rxPackets = 0;

    /** 本次仿真窗口内未送达 UE 的包数。 */
    uint64_t lostPackets = 0;

    /** 源端累计发送字节数。 */
    uint64_t txBytes = 0;

    /** UE 实际接收字节数。 */
    uint64_t rxBytes = 0;

    /** 该 UE 下行业务的平均端到端时延，单位毫秒。 */
    double meanDelayMs = std::numeric_limits<double>::quiet_NaN();

    /** 该 UE 下行业务的平均抖动，单位毫秒。 */
    double meanJitterMs = std::numeric_limits<double>::quiet_NaN();

    /** 该 UE 下行业务的包丢失率，单位百分比。 */
    double packetLossRatePercent = 0.0;

    /** 源端口径的平均提供吞吐，单位 Mbps。 */
    double averageOfferedMbps = 0.0;

    /** 目的端口径的平均接收吞吐，单位 Mbps。 */
    double averageThroughputMbps = 0.0;

    /** FlowMonitor 累计时延和，单位秒，仅用于内部均值换算。 */
    double delaySumSeconds = 0.0;

    /** FlowMonitor 累计抖动和，单位秒，仅用于内部均值换算。 */
    double jitterSumSeconds = 0.0;
};

/**
 * 所有 UE 的端到端业务流聚合结果。
 */
struct E2eFlowAggregate
{
    /** 每个 UE 的独立端到端统计。 */
    std::vector<UeE2eFlowSummary> perUe;

    /** 匹配到的下行业务流数量。 */
    uint32_t matchedFlowCount = 0;

    /** 所有 UE 汇总后的发送包数。 */
    uint64_t totalTxPackets = 0;

    /** 所有 UE 汇总后的接收包数。 */
    uint64_t totalRxPackets = 0;

    /** 所有 UE 汇总后的未送达包数。 */
    uint64_t totalLostPackets = 0;

    /** 所有 UE 汇总后的发送字节数。 */
    uint64_t totalTxBytes = 0;

    /** 所有 UE 汇总后的接收字节数。 */
    uint64_t totalRxBytes = 0;

    /** 汇总平均端到端时延，单位毫秒。 */
    double meanDelayMs = std::numeric_limits<double>::quiet_NaN();

    /** 汇总平均抖动，单位毫秒。 */
    double meanJitterMs = std::numeric_limits<double>::quiet_NaN();

    /** 汇总包丢失率，单位百分比。 */
    double packetLossRatePercent = 0.0;

    /** 汇总平均提供吞吐，单位 Mbps。 */
    double totalAverageOfferedMbps = 0.0;

    /** 汇总平均接收吞吐，单位 Mbps。 */
    double totalAverageThroughputMbps = 0.0;
};

/**
 * 单个 UE 的 PHY 下行传输块统计结果。
 */
struct UePhyDlTbSummary
{
    /** UE 序号。 */
    uint32_t ueIndex = 0;

    /** PHY 下行总传输块数。 */
    uint64_t tbCount = 0;

    /** PHY 下行损坏传输块数。 */
    uint64_t corruptTbCount = 0;

    /** PHY 下行损坏块率，单位百分比。 */
    double corruptTbRatePercent = 0.0;

    /** PHY 下行平均 TBler。 */
    double meanTbler = std::numeric_limits<double>::quiet_NaN();

    /** PHY 下行平均 SINR，单位 dB。 */
    double meanSinrDb = std::numeric_limits<double>::quiet_NaN();

    /** PHY 下行最小 SINR，单位 dB。 */
    double minSinrDb = std::numeric_limits<double>::quiet_NaN();
};

/**
 * 所有 UE 的 PHY 下行传输块聚合结果。
 */
struct PhyDlTbAggregate
{
    /** 每个 UE 的独立 PHY 下行统计。 */
    std::vector<UePhyDlTbSummary> perUe;

    /** 所有 UE 汇总后的传输块数。 */
    uint64_t totalTbCount = 0;

    /** 所有 UE 汇总后的损坏传输块数。 */
    uint64_t totalCorruptTbCount = 0;

    /** 汇总损坏块率，单位百分比。 */
    double corruptTbRatePercent = 0.0;

    /** 汇总平均 TBler。 */
    double meanTbler = std::numeric_limits<double>::quiet_NaN();

    /** 汇总平均 SINR，单位 dB。 */
    double meanSinrDb = std::numeric_limits<double>::quiet_NaN();

    /** 汇总最小 SINR，单位 dB。 */
    double minSinrDb = std::numeric_limits<double>::quiet_NaN();
};

/**
 * 单个时间窗内的 PHY 下行传输块累计量。
 */
struct PhyDlTbIntervalAccumulator
{
    uint64_t tbCount = 0;
    uint64_t corruptTbCount = 0;
    double tblerSum = 0.0;
    double sinrDbSum = 0.0;
    double minSinrDb = std::numeric_limits<double>::infinity();
};

/**
 * 单个时间窗的 PHY 下行传输块统计结果。
 */
struct PhyDlTbIntervalSummary
{
    uint64_t intervalIndex = 0;
    double windowStartSeconds = 0.0;
    double windowEndSeconds = 0.0;
    uint64_t tbCount = 0;
    uint64_t corruptTbCount = 0;
    double corruptTbRatePercent = 0.0;
    double meanTbler = std::numeric_limits<double>::quiet_NaN();
    double meanSinrDb = std::numeric_limits<double>::quiet_NaN();
    double minSinrDb = std::numeric_limits<double>::quiet_NaN();
};

/**
 * 所有时间窗的 PHY 下行传输块统计结果。
 */
struct PhyDlTbIntervalAggregate
{
    std::vector<PhyDlTbIntervalSummary> intervals;
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

    /** 整体切换成功率，范围为 0 到 100。 */
    double overallSuccessRate = 0.0;

    /** 全部成功切换的平均执行时延，单位毫秒。 */
    double averageDelayMs = 0.0;

    /** 全部 UE 识别到的短时回切总数。 */
    uint32_t totalPingPongCount = 0;

    /** 全部 UE 检测到的 interference-trap 切换总数。 */
    uint32_t totalInterferenceTrapCount = 0;

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
 * 根据 FlowMonitor 结果生成每个 UE 的端到端时延与丢包统计。
 */
inline E2eFlowAggregate
BuildE2eFlowAggregate(const std::vector<UeRuntime>& ues,
                      const FlowMonitor::FlowStatsContainer& stats,
                      const Ptr<Ipv4FlowClassifier>& classifier,
                      double appDurationSeconds,
                      uint32_t udpPacketSizeBytes)
{
    E2eFlowAggregate out;
    out.perUe.reserve(ues.size());

    std::map<uint16_t, uint32_t> portToUe;
    for (uint32_t ueIdx = 0; ueIdx < ues.size(); ++ueIdx)
    {
        UeE2eFlowSummary summary;
        summary.ueIndex = ueIdx;
        summary.dlPort = ues[ueIdx].dlPort;
        out.perUe.push_back(summary);
        if (summary.dlPort != 0)
        {
            portToUe[summary.dlPort] = ueIdx;
        }
    }

    if (!classifier)
    {
        return out;
    }

    double totalDelaySeconds = 0.0;
    double totalJitterSeconds = 0.0;

    for (const auto& flowEntry : stats)
    {
        const auto tuple = classifier->FindFlow(flowEntry.first);
        if (tuple.protocol != 17)
        {
            continue;
        }

        const auto portIt = portToUe.find(tuple.destinationPort);
        if (portIt == portToUe.end())
        {
            continue;
        }

        auto& summary = out.perUe[portIt->second];
        const auto& stat = flowEntry.second;
        summary.hasMatchedFlow = true;
        summary.txPackets += stat.txPackets;
        summary.rxPackets += stat.rxPackets;
        summary.txBytes += stat.txBytes;
        summary.rxBytes += stat.rxBytes;
        summary.delaySumSeconds += stat.delaySum.GetSeconds();
        summary.jitterSumSeconds += stat.jitterSum.GetSeconds();
    }

    for (auto& summary : out.perUe)
    {
        if (!summary.hasMatchedFlow)
        {
            continue;
        }

        out.matchedFlowCount++;
        summary.lostPackets = (summary.txPackets >= summary.rxPackets) ? (summary.txPackets - summary.rxPackets) : 0;
        if (summary.rxPackets > 0)
        {
            summary.meanDelayMs =
                summary.delaySumSeconds * 1000.0 / static_cast<double>(summary.rxPackets);
            totalDelaySeconds += summary.delaySumSeconds;
        }
        if (summary.rxPackets > 1)
        {
            summary.meanJitterMs =
                summary.jitterSumSeconds * 1000.0 / static_cast<double>(summary.rxPackets - 1);
            totalJitterSeconds += summary.jitterSumSeconds;
        }
        if (summary.txPackets > 0)
        {
            summary.packetLossRatePercent =
                100.0 * static_cast<double>(summary.lostPackets) / static_cast<double>(summary.txPackets);
        }
        if (appDurationSeconds > 0.0)
        {
            summary.averageOfferedMbps =
                summary.txPackets * udpPacketSizeBytes * 8.0 / appDurationSeconds / 1e6;
            summary.averageThroughputMbps =
                summary.rxPackets * udpPacketSizeBytes * 8.0 / appDurationSeconds / 1e6;
        }

        out.totalTxPackets += summary.txPackets;
        out.totalRxPackets += summary.rxPackets;
        out.totalLostPackets += summary.lostPackets;
        out.totalTxBytes += summary.txBytes;
        out.totalRxBytes += summary.rxBytes;
    }

    if (out.totalTxPackets > 0)
    {
        out.packetLossRatePercent =
            100.0 * static_cast<double>(out.totalLostPackets) / static_cast<double>(out.totalTxPackets);
    }
    if (out.totalRxPackets > 0)
    {
        out.meanDelayMs = totalDelaySeconds * 1000.0 / static_cast<double>(out.totalRxPackets);
    }
    if (out.totalRxPackets > 1)
    {
        out.meanJitterMs = totalJitterSeconds * 1000.0 /
                           static_cast<double>(out.totalRxPackets - out.matchedFlowCount);
    }
    if (appDurationSeconds > 0.0)
    {
        out.totalAverageOfferedMbps =
            out.totalTxPackets * udpPacketSizeBytes * 8.0 / appDurationSeconds / 1e6;
        out.totalAverageThroughputMbps =
            out.totalRxPackets * udpPacketSizeBytes * 8.0 / appDurationSeconds / 1e6;
    }
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
        out.totalPingPongCount += ue.pingPongCount;
        out.totalInterferenceTrapCount += ue.interferenceTrapHoCount;
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
 * 打印端到端时延与丢包汇总。
 */
inline void
PrintE2eFlowSummary(const E2eFlowAggregate& aggregate)
{
    if (aggregate.matchedFlowCount == 0)
    {
        std::cout << "Average E2E delay(rx packets): n/a" << std::endl;
        std::cout << "Packet loss rate(total): n/a" << std::endl;
        return;
    }

    if (std::isfinite(aggregate.meanDelayMs))
    {
        std::cout << "Average E2E delay(rx packets): " << std::fixed << std::setprecision(3)
                  << aggregate.meanDelayMs << " ms" << std::endl;
    }
    else
    {
        std::cout << "Average E2E delay(rx packets): n/a" << std::endl;
    }

    std::cout << "Packet loss rate(total): " << std::fixed << std::setprecision(3)
              << aggregate.packetLossRatePercent << " %"
              << " (tx=" << aggregate.totalTxPackets
              << ", rx=" << aggregate.totalRxPackets
              << ", lost=" << aggregate.totalLostPackets << ")" << std::endl;
}

/**
 * 导出每个 UE 的端到端业务流统计。
 */
inline bool
WriteE2eFlowMetricsCsv(const std::string& path, const E2eFlowAggregate& aggregate)
{
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out.is_open())
    {
        return false;
    }

    out << "ue,dl_port,matched_flow,tx_packets,rx_packets,lost_packets,loss_rate_percent,tx_bytes,"
        << "rx_bytes,offered_mbps,throughput_mbps,mean_delay_ms,mean_jitter_ms\n";

    out << std::fixed << std::setprecision(3);
    for (const auto& summary : aggregate.perUe)
    {
        out << summary.ueIndex << "," << summary.dlPort << "," << (summary.hasMatchedFlow ? 1 : 0)
            << "," << summary.txPackets << "," << summary.rxPackets << "," << summary.lostPackets
            << "," << summary.packetLossRatePercent << "," << summary.txBytes << ","
            << summary.rxBytes << "," << summary.averageOfferedMbps << ","
            << summary.averageThroughputMbps << ",";
        if (std::isfinite(summary.meanDelayMs))
        {
            out << summary.meanDelayMs;
        }
        out << ",";
        if (std::isfinite(summary.meanJitterMs))
        {
            out << summary.meanJitterMs;
        }
        out << "\n";
    }

    out << "TOTAL,,," << aggregate.totalTxPackets << "," << aggregate.totalRxPackets << ","
        << aggregate.totalLostPackets << "," << aggregate.packetLossRatePercent << ","
        << aggregate.totalTxBytes << "," << aggregate.totalRxBytes << ","
        << aggregate.totalAverageOfferedMbps << "," << aggregate.totalAverageThroughputMbps << ",";
    if (std::isfinite(aggregate.meanDelayMs))
    {
        out << aggregate.meanDelayMs;
    }
    out << ",";
    if (std::isfinite(aggregate.meanJitterMs))
    {
        out << aggregate.meanJitterMs;
    }
    out << "\n";
    return true;
}

/**
 * 根据 UE 运行时对象生成 PHY 下行传输块聚合统计。
 */
inline PhyDlTbAggregate
BuildPhyDlTbAggregate(const std::vector<UeRuntime>& ues)
{
    PhyDlTbAggregate out;
    out.perUe.reserve(ues.size());

    double totalTblerSum = 0.0;
    double totalSinrDbSum = 0.0;
    double globalMinSinrDb = std::numeric_limits<double>::infinity();

    for (uint32_t ueIdx = 0; ueIdx < ues.size(); ++ueIdx)
    {
        const auto& ue = ues[ueIdx];
        UePhyDlTbSummary summary;
        summary.ueIndex = ueIdx;
        summary.tbCount = ue.phyDlTbCount;
        summary.corruptTbCount = ue.phyDlCorruptTbCount;

        if (summary.tbCount > 0)
        {
            summary.corruptTbRatePercent =
                100.0 * static_cast<double>(summary.corruptTbCount) / static_cast<double>(summary.tbCount);
            summary.meanTbler = ue.phyDlTblerSum / static_cast<double>(summary.tbCount);
            summary.meanSinrDb = ue.phyDlSinrDbSum / static_cast<double>(summary.tbCount);
            summary.minSinrDb = ue.phyDlMinSinrDb;

            totalTblerSum += ue.phyDlTblerSum;
            totalSinrDbSum += ue.phyDlSinrDbSum;
            globalMinSinrDb = std::min(globalMinSinrDb, ue.phyDlMinSinrDb);
        }

        out.totalTbCount += summary.tbCount;
        out.totalCorruptTbCount += summary.corruptTbCount;
        out.perUe.push_back(summary);
    }

    if (out.totalTbCount > 0)
    {
        out.corruptTbRatePercent =
            100.0 * static_cast<double>(out.totalCorruptTbCount) / static_cast<double>(out.totalTbCount);
        out.meanTbler = totalTblerSum / static_cast<double>(out.totalTbCount);
        out.meanSinrDb = totalSinrDbSum / static_cast<double>(out.totalTbCount);
        out.minSinrDb = globalMinSinrDb;
    }

    return out;
}

/**
 * 打印 PHY 下行传输块摘要。
 */
inline void
PrintPhyDlTbSummary(const PhyDlTbAggregate& aggregate)
{
    std::cout << "PHY DL TB error rate(total): " << std::fixed << std::setprecision(3)
              << aggregate.corruptTbRatePercent << " %"
              << " (tb=" << aggregate.totalTbCount
              << ", corrupt=" << aggregate.totalCorruptTbCount << ")" << std::endl;

    if (std::isfinite(aggregate.meanTbler))
    {
        std::cout << "PHY DL mean TBler(total): " << std::fixed << std::setprecision(6)
                  << aggregate.meanTbler << std::endl;
    }
    else
    {
        std::cout << "PHY DL mean TBler(total): n/a" << std::endl;
    }

    if (std::isfinite(aggregate.meanSinrDb))
    {
        std::cout << "PHY DL mean SINR(total): " << std::fixed << std::setprecision(3)
                  << aggregate.meanSinrDb << " dB";
        if (std::isfinite(aggregate.minSinrDb))
        {
            std::cout << " (min=" << aggregate.minSinrDb << " dB)";
        }
        std::cout << std::endl;
    }
    else
    {
        std::cout << "PHY DL mean SINR(total): n/a" << std::endl;
    }
}

/**
 * 导出每个 UE 的 PHY 下行传输块统计。
 */
inline bool
WritePhyDlTbMetricsCsv(const std::string& path, const PhyDlTbAggregate& aggregate)
{
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out.is_open())
    {
        return false;
    }

    out << "ue,tb_count,corrupt_tb_count,corrupt_tb_rate_percent,mean_tbler,mean_sinr_db,min_sinr_db\n";

    out << std::fixed << std::setprecision(6);
    for (const auto& summary : aggregate.perUe)
    {
        out << summary.ueIndex << "," << summary.tbCount << "," << summary.corruptTbCount << ","
            << summary.corruptTbRatePercent << ",";
        if (std::isfinite(summary.meanTbler))
        {
            out << summary.meanTbler;
        }
        out << ",";
        if (std::isfinite(summary.meanSinrDb))
        {
            out << summary.meanSinrDb;
        }
        out << ",";
        if (std::isfinite(summary.minSinrDb))
        {
            out << summary.minSinrDb;
        }
        out << "\n";
    }

    out << "TOTAL," << aggregate.totalTbCount << "," << aggregate.totalCorruptTbCount << ","
        << aggregate.corruptTbRatePercent << ",";
    if (std::isfinite(aggregate.meanTbler))
    {
        out << aggregate.meanTbler;
    }
    out << ",";
    if (std::isfinite(aggregate.meanSinrDb))
    {
        out << aggregate.meanSinrDb;
    }
    out << ",";
    if (std::isfinite(aggregate.minSinrDb))
    {
        out << aggregate.minSinrDb;
    }
    out << "\n";
    return true;
}

/**
 * 根据时间窗累计量生成 PHY 下行分段统计。
 */
inline PhyDlTbIntervalAggregate
BuildPhyDlTbIntervalAggregate(const std::map<uint64_t, PhyDlTbIntervalAccumulator>& accumulators,
                              double intervalSeconds)
{
    PhyDlTbIntervalAggregate out;
    out.intervals.reserve(accumulators.size());

    for (const auto& [intervalIndex, acc] : accumulators)
    {
        PhyDlTbIntervalSummary summary;
        summary.intervalIndex = intervalIndex;
        summary.windowStartSeconds = static_cast<double>(intervalIndex) * intervalSeconds;
        summary.windowEndSeconds = summary.windowStartSeconds + intervalSeconds;
        summary.tbCount = acc.tbCount;
        summary.corruptTbCount = acc.corruptTbCount;

        if (summary.tbCount > 0)
        {
            summary.corruptTbRatePercent =
                100.0 * static_cast<double>(summary.corruptTbCount) / static_cast<double>(summary.tbCount);
            summary.meanTbler = acc.tblerSum / static_cast<double>(summary.tbCount);
            summary.meanSinrDb = acc.sinrDbSum / static_cast<double>(summary.tbCount);
            summary.minSinrDb = acc.minSinrDb;
        }

        out.intervals.push_back(summary);
    }

    return out;
}

/**
 * 导出按时间窗聚合的 PHY 下行传输块统计。
 */
inline bool
WritePhyDlTbIntervalMetricsCsv(const std::string& path, const PhyDlTbIntervalAggregate& aggregate)
{
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out.is_open())
    {
        return false;
    }

    out << "interval_index,window_start_s,window_end_s,tb_count,corrupt_tb_count,"
           "corrupt_tb_rate_percent,mean_tbler,mean_sinr_db,min_sinr_db\n";

    out << std::fixed << std::setprecision(6);
    for (const auto& summary : aggregate.intervals)
    {
        out << summary.intervalIndex << "," << summary.windowStartSeconds << ","
            << summary.windowEndSeconds << "," << summary.tbCount << ","
            << summary.corruptTbCount << "," << summary.corruptTbRatePercent << ",";
        if (std::isfinite(summary.meanTbler))
        {
            out << summary.meanTbler;
        }
        out << ",";
        if (std::isfinite(summary.meanSinrDb))
        {
            out << summary.meanSinrDb;
        }
        out << ",";
        if (std::isfinite(summary.minSinrDb))
        {
            out << summary.minSinrDb;
        }
        out << "\n";
    }

    return true;
}

/**
 * 打印整体与分 UE 的切换汇总。
 */
inline void
PrintHandoverSummary(const std::vector<UeRuntime>& ues, double pingPongWindowSeconds)
{
    const HandoverAggregate aggregate = BuildHandoverAggregate(ues);
    std::cout << "Completed handovers: " << aggregate.totalHoEndOk << std::endl;
    std::cout << "Total interference-trap HO: " << aggregate.totalInterferenceTrapCount
              << std::endl;
    std::cout << "Total ping-pong events: " << aggregate.totalPingPongCount
              << " (window=" << std::fixed << std::setprecision(3) << pingPongWindowSeconds
              << "s)" << std::endl;
}

} // namespace ns3

#endif // MYFIRST_REPORTING_H
