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
                        outputs.handoverThroughputTrace == nullptr ||
                        outputs.handoverEventTrace == nullptr ||
                        outputs.phyDlTbTrace == nullptr,
                    "BaselineTraceOutputSet contains null stream pointers");

    const bool satAnchorReady = ResetCsvOutputStream(*outputs.satAnchorTrace,
                                                     config.satAnchorTracePath,
                                                     HandoverCsvHeaders::kSatAnchorTrace);
    NS_ABORT_MSG_IF(!satAnchorReady,
                    "Failed to open satellite anchor trace CSV: " << config.satAnchorTracePath);
    const bool satGroundReady =
        ResetCsvOutputStream(*outputs.satGroundTrackTrace,
                             config.satGroundTrackTracePath,
                             HandoverCsvHeaders::kSatGroundTrackTrace);
    NS_ABORT_MSG_IF(!satGroundReady,
                    "Failed to open satellite ground-track CSV: "
                        << config.satGroundTrackTracePath);
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
                        outputs.handoverThroughputTrace == nullptr ||
                        outputs.handoverEventTrace == nullptr ||
                        outputs.phyDlTbTrace == nullptr,
                    "BaselineTraceOutputSet contains null stream pointers");

    CloseOutputStreams(*outputs.satAnchorTrace,
                       *outputs.satGroundTrackTrace,
                       *outputs.handoverThroughputTrace,
                       *outputs.handoverEventTrace,
                       *outputs.phyDlTbTrace);
}

} // namespace ns3

#endif // LEO_NTN_HANDOVER_OUTPUT_H
