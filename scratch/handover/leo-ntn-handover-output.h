#ifndef LEO_NTN_HANDOVER_OUTPUT_H
#define LEO_NTN_HANDOVER_OUTPUT_H

#include "leo-ntn-handover-config.h"
#include "leo-ntn-handover-output-lifecycle.h"
#include "ns3/core-module.h"

#include <fstream>

namespace ns3
{

struct BaselineTraceOutputSet
{
    std::ofstream* satAnchorTrace = nullptr;
    std::ofstream* satGroundTrackTrace = nullptr;
    std::ofstream* satelliteStateTrace = nullptr;
    std::ofstream* handoverThroughputTrace = nullptr;
    std::ofstream* handoverEventTrace = nullptr;
    std::ofstream* phyDlTbTrace = nullptr;
};

inline void
InitializeBaselineTraceOutputs(const BaselineSimulationConfig& config,
                               const BaselineTraceOutputSet& outputs)
{
    NS_ABORT_MSG_IF(outputs.satAnchorTrace == nullptr ||
                        outputs.satGroundTrackTrace == nullptr ||
                        outputs.satelliteStateTrace == nullptr ||
                        outputs.handoverThroughputTrace == nullptr ||
                        outputs.handoverEventTrace == nullptr ||
                        outputs.phyDlTbTrace == nullptr,
                    "BaselineTraceOutputSet contains null stream pointers");

    if (config.enableSatAnchorTrace)
    {
        const bool satAnchorReady = ResetCsvOutputStream(*outputs.satAnchorTrace,
                                                         config.satAnchorTracePath,
                                                         HandoverCsvHeaders::kSatAnchorTrace);
        NS_ABORT_MSG_IF(!satAnchorReady,
                        "Failed to open satellite anchor trace CSV: "
                            << config.satAnchorTracePath);
    }
    else
    {
        CloseOutputStream(*outputs.satAnchorTrace);
    }

    if (config.enableSatGroundTrackTrace)
    {
        const bool satGroundReady =
            ResetCsvOutputStream(*outputs.satGroundTrackTrace,
                                 config.satGroundTrackTracePath,
                                 HandoverCsvHeaders::kSatGroundTrackTrace);
        NS_ABORT_MSG_IF(!satGroundReady,
                        "Failed to open satellite ground-track CSV: "
                            << config.satGroundTrackTracePath);
    }
    else
    {
        CloseOutputStream(*outputs.satGroundTrackTrace);
    }

    if (config.enableSatelliteStateTrace)
    {
        const bool satelliteStateReady =
            ResetCsvOutputStream(*outputs.satelliteStateTrace,
                                 config.satelliteStateTracePath,
                                 HandoverCsvHeaders::kSatelliteStateTrace);
        NS_ABORT_MSG_IF(!satelliteStateReady,
                        "Failed to open satellite state trace CSV: "
                            << config.satelliteStateTracePath);
    }
    else
    {
        CloseOutputStream(*outputs.satelliteStateTrace);
    }

    const bool handoverEventReady = ResetCsvOutputStream(*outputs.handoverEventTrace,
                                                         config.handoverEventTracePath,
                                                         HandoverCsvHeaders::kHandoverEventTrace);
    NS_ABORT_MSG_IF(!handoverEventReady,
                    "Failed to open handover event trace CSV: "
                        << config.handoverEventTracePath);
    outputs.handoverEventTrace->flush();

    if (config.enablePhyDlTbTrace)
    {
        const bool phyReady = ResetCsvOutputStream(*outputs.phyDlTbTrace,
                                                   config.phyDlTbTracePath,
                                                   HandoverCsvHeaders::kPhyDlTbTrace);
        NS_ABORT_MSG_IF(!phyReady,
                        "Failed to open PHY DL TB trace CSV: " << config.phyDlTbTracePath);
    }
    else
    {
        CloseOutputStream(*outputs.phyDlTbTrace);
    }

    if (config.enableHandoverThroughputTrace)
    {
        const bool throughputReady =
            ResetCsvOutputStream(*outputs.handoverThroughputTrace,
                                 config.handoverThroughputTracePath,
                                 HandoverCsvHeaders::kHandoverThroughputTrace);
        NS_ABORT_MSG_IF(!throughputReady,
                        "Failed to open handover throughput trace CSV: "
                            << config.handoverThroughputTracePath);
    }
    else
    {
        CloseOutputStream(*outputs.handoverThroughputTrace);
    }
}

inline void
CloseBaselineTraceOutputs(const BaselineTraceOutputSet& outputs)
{
    NS_ABORT_MSG_IF(outputs.satAnchorTrace == nullptr ||
                        outputs.satGroundTrackTrace == nullptr ||
                        outputs.satelliteStateTrace == nullptr ||
                        outputs.handoverThroughputTrace == nullptr ||
                        outputs.handoverEventTrace == nullptr ||
                        outputs.phyDlTbTrace == nullptr,
                    "BaselineTraceOutputSet contains null stream pointers");

    CloseOutputStreams(*outputs.satAnchorTrace,
                       *outputs.satGroundTrackTrace,
                       *outputs.satelliteStateTrace,
                       *outputs.handoverThroughputTrace,
                       *outputs.handoverEventTrace,
                       *outputs.phyDlTbTrace);
}

} // namespace ns3

#endif // LEO_NTN_HANDOVER_OUTPUT_H
