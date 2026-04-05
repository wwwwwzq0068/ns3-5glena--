// Copyright (c) 2026
//
// SPDX-License-Identifier: GPL-2.0-only

#include "nr-leo-a3-measurement-handover-algorithm.h"

#include "nr-common.h"

#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/uinteger.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>

namespace ns3
{

namespace
{

constexpr std::array<uint16_t, 16> kSupportedTimeToTriggerMs = {
    0, 40, 64, 80, 100, 128, 160, 256, 320, 480, 512, 640, 1024, 1280, 2560, 5120};

}

NS_LOG_COMPONENT_DEFINE("NrLeoA3MeasurementHandoverAlgorithm");

NS_OBJECT_ENSURE_REGISTERED(NrLeoA3MeasurementHandoverAlgorithm);

NrLeoA3MeasurementHandoverAlgorithm::NrLeoA3MeasurementHandoverAlgorithm()
    : m_handoverManagementSapUser(nullptr)
{
    NS_LOG_FUNCTION(this);
    m_handoverManagementSapProvider =
        new MemberNrHandoverManagementSapProvider<NrLeoA3MeasurementHandoverAlgorithm>(this);
}

NrLeoA3MeasurementHandoverAlgorithm::~NrLeoA3MeasurementHandoverAlgorithm()
{
    NS_LOG_FUNCTION(this);
}

TypeId
NrLeoA3MeasurementHandoverAlgorithm::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::NrLeoA3MeasurementHandoverAlgorithm")
            .SetParent<NrHandoverAlgorithm>()
            .SetGroupName("Nr")
            .AddConstructor<NrLeoA3MeasurementHandoverAlgorithm>()
            .AddAttribute(
                "Hysteresis",
                "Handover margin in dB (rounded to the nearest multiple of 0.5 dB).",
                DoubleValue(2.0),
                MakeDoubleAccessor(&NrLeoA3MeasurementHandoverAlgorithm::m_hysteresisDb),
                MakeDoubleChecker<double>(0.0, 15.0))
            .AddAttribute("TimeToTrigger",
                          "Duration during which the A3 entry condition must stay true before "
                          "the UE reports it. Non-standard values are normalized upward to the "
                          "next 3GPP-supported TTT.",
                          TimeValue(MilliSeconds(160)),
                          MakeTimeAccessor(&NrLeoA3MeasurementHandoverAlgorithm::m_timeToTrigger),
                          MakeTimeChecker())
            .AddAttribute("ReportIntervalMs",
                          "Measurement report interval in milliseconds. Supported values are "
                          "120, 240, 480, 640, 1024, 2048, 5120, and 10240.",
                          UintegerValue(120),
                          MakeUintegerAccessor(
                              &NrLeoA3MeasurementHandoverAlgorithm::m_reportIntervalMs),
                          MakeUintegerChecker<uint16_t>(120, 10240))
            .AddAttribute("MaxReportCells",
                          "Maximum number of neighbour cells to include in the A3 report.",
                          UintegerValue(8),
                          MakeUintegerAccessor(
                              &NrLeoA3MeasurementHandoverAlgorithm::m_maxReportCells),
                          MakeUintegerChecker<uint8_t>(1, 32))
            .AddAttribute("TriggerHandover",
                          "Whether to trigger the handover immediately when a valid "
                          "MeasurementReport is received. Disable this when the scenario "
                          "wants to reuse the standard PHY/RRC MeasurementReport path but "
                          "apply its own target-selection logic.",
                          BooleanValue(true),
                          MakeBooleanAccessor(
                              &NrLeoA3MeasurementHandoverAlgorithm::m_triggerHandover),
                          MakeBooleanChecker())
            .AddTraceSource("MeasurementReport",
                            "Forwarded MeasurementReport that matched the handover measId "
                            "registered by this algorithm. Exporting RNTI and MeasResults.",
                            MakeTraceSourceAccessor(
                                &NrLeoA3MeasurementHandoverAlgorithm::m_measurementReportTrace),
                            "ns3::TracedCallback::Uint16tNrRrcSapMeasResults");
    return tid;
}

void
NrLeoA3MeasurementHandoverAlgorithm::SetNrHandoverManagementSapUser(
    NrHandoverManagementSapUser* s)
{
    NS_LOG_FUNCTION(this << s);
    m_handoverManagementSapUser = s;
}

NrHandoverManagementSapProvider*
NrLeoA3MeasurementHandoverAlgorithm::GetNrHandoverManagementSapProvider()
{
    NS_LOG_FUNCTION(this);
    return m_handoverManagementSapProvider;
}

uint16_t
NrLeoA3MeasurementHandoverAlgorithm::NormalizeTimeToTriggerMs(uint16_t timeToTriggerMs)
{
    const auto normalized =
        std::lower_bound(kSupportedTimeToTriggerMs.begin(),
                         kSupportedTimeToTriggerMs.end(),
                         timeToTriggerMs);

    if (normalized != kSupportedTimeToTriggerMs.end())
    {
        return *normalized;
    }

    return kSupportedTimeToTriggerMs.back();
}

void
NrLeoA3MeasurementHandoverAlgorithm::DoInitialize()
{
    NS_LOG_FUNCTION(this);

    const uint8_t hysteresisIeValue =
        nr::EutranMeasurementMapping::ActualHysteresis2IeValue(m_hysteresisDb);
    const int64_t requestedTimeToTriggerMs =
        std::max<int64_t>(0, m_timeToTrigger.GetMilliSeconds());
    const uint16_t effectiveTimeToTriggerMs = NormalizeTimeToTriggerMs(
        static_cast<uint16_t>(std::min<int64_t>(requestedTimeToTriggerMs,
                                                std::numeric_limits<uint16_t>::max())));

    if (effectiveTimeToTriggerMs != requestedTimeToTriggerMs)
    {
        NS_LOG_WARN("Normalizing non-standard TimeToTrigger=" << requestedTimeToTriggerMs
                                                              << "ms to "
                                                              << effectiveTimeToTriggerMs
                                                              << "ms for standard RRC encoding");
    }

    NrRrcSap::ReportConfigEutra reportConfig;
    reportConfig.eventId = NrRrcSap::ReportConfigEutra::EVENT_A3;
    reportConfig.a3Offset = 0;
    reportConfig.hysteresis = hysteresisIeValue;
    reportConfig.timeToTrigger = effectiveTimeToTriggerMs;
    reportConfig.reportOnLeave = false;
    reportConfig.triggerQuantity = NrRrcSap::ReportConfigEutra::RSRP;
    reportConfig.reportQuantity = NrRrcSap::ReportConfigEutra::BOTH;
    reportConfig.reportInterval =
        static_cast<decltype(reportConfig.reportInterval)>(
            ConvertReportIntervalMsToEnumValue(m_reportIntervalMs));
    reportConfig.maxReportCells = m_maxReportCells;

    m_measIds = m_handoverManagementSapUser->AddUeMeasReportConfigForHandover(reportConfig);

    NrHandoverAlgorithm::DoInitialize();
}

void
NrLeoA3MeasurementHandoverAlgorithm::DoDispose()
{
    NS_LOG_FUNCTION(this);
    delete m_handoverManagementSapProvider;
    m_handoverManagementSapProvider = nullptr;
    m_handoverManagementSapUser = nullptr;
}

void
NrLeoA3MeasurementHandoverAlgorithm::DoReportUeMeas(uint16_t rnti,
                                                    NrRrcSap::MeasResults measResults)
{
    NS_LOG_FUNCTION(this << rnti << (uint16_t)measResults.measId);

    if (std::find(m_measIds.begin(), m_measIds.end(), measResults.measId) == m_measIds.end())
    {
        NS_LOG_WARN("Ignoring measId " << (uint16_t)measResults.measId);
        return;
    }

    m_measurementReportTrace(rnti, measResults);

    if (!measResults.haveMeasResultNeighCells || measResults.measResultListEutra.empty())
    {
        NS_LOG_WARN(this << " Event A3 received without neighbour measurements");
        return;
    }

    uint16_t bestNeighbourCellId = 0;
    uint8_t bestNeighbourRsrp = 0;

    for (const auto& neighbour : measResults.measResultListEutra)
    {
        if (!neighbour.haveRsrpResult)
        {
            NS_LOG_WARN("RSRP measurement is missing from cell ID " << neighbour.physCellId);
            continue;
        }

        if (neighbour.rsrpResult > bestNeighbourRsrp && IsValidNeighbour(neighbour.physCellId))
        {
            bestNeighbourCellId = neighbour.physCellId;
            bestNeighbourRsrp = neighbour.rsrpResult;
        }
    }

    if (bestNeighbourCellId == 0)
    {
        return;
    }

    if (!m_triggerHandover)
    {
        NS_LOG_LOGIC("MeasurementReport received for cellId " << bestNeighbourCellId
                                                              << " but TriggerHandover is OFF");
        return;
    }

    NS_LOG_LOGIC("Trigger Handover to cellId " << bestNeighbourCellId);
    NS_LOG_LOGIC("target cell RSRP " << (uint16_t)bestNeighbourRsrp);
    NS_LOG_LOGIC("serving cell RSRP " << (uint16_t)measResults.measResultPCell.rsrpResult);

    m_handoverManagementSapUser->TriggerHandover(rnti, bestNeighbourCellId);
}

uint8_t
NrLeoA3MeasurementHandoverAlgorithm::ConvertReportIntervalMsToEnumValue(uint16_t reportIntervalMs)
{
    switch (reportIntervalMs)
    {
    case 120:
        return NrRrcSap::ReportConfigEutra::MS120;
    case 240:
        return NrRrcSap::ReportConfigEutra::MS240;
    case 480:
        return NrRrcSap::ReportConfigEutra::MS480;
    case 640:
        return NrRrcSap::ReportConfigEutra::MS640;
    case 1024:
        return NrRrcSap::ReportConfigEutra::MS1024;
    case 2048:
        return NrRrcSap::ReportConfigEutra::MS2048;
    case 5120:
        return NrRrcSap::ReportConfigEutra::MS5120;
    case 10240:
        return NrRrcSap::ReportConfigEutra::MS10240;
    default:
        NS_ABORT_MSG("Unsupported ReportIntervalMs=" << reportIntervalMs);
    }

    return NrRrcSap::ReportConfigEutra::MS120;
}

bool
NrLeoA3MeasurementHandoverAlgorithm::IsValidNeighbour(uint16_t cellId) const
{
    NS_LOG_FUNCTION(this << cellId);
    return cellId > 0;
}

} // namespace ns3
