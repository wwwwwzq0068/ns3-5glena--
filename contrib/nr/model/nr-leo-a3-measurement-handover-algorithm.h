// Copyright (c) 2026
//
// SPDX-License-Identifier: GPL-2.0-only

#ifndef NR_LEO_A3_MEASUREMENT_HANDOVER_ALGORITHM_H
#define NR_LEO_A3_MEASUREMENT_HANDOVER_ALGORITHM_H

#include "nr-handover-algorithm.h"
#include "nr-handover-management-sap.h"
#include "nr-rrc-sap.h"

#include "ns3/nstime.h"

namespace ns3
{

/**
 * A3 handover algorithm variant for the LEO-NTN scenario that consumes the
 * standard UE MeasurementReport path instead of an external proxy metric.
 *
 * Compared with the stock NR A3 algorithm, this class exposes a shorter report
 * interval and max-report-cells knobs so the scenario can stay closer to the
 * current NTN observation cadence.
 */
class NrLeoA3MeasurementHandoverAlgorithm : public NrHandoverAlgorithm
{
  public:
    NrLeoA3MeasurementHandoverAlgorithm();
    ~NrLeoA3MeasurementHandoverAlgorithm() override;

    static TypeId GetTypeId();
    static uint16_t NormalizeTimeToTriggerMs(uint16_t timeToTriggerMs);

    void SetNrHandoverManagementSapUser(NrHandoverManagementSapUser* s) override;
    NrHandoverManagementSapProvider* GetNrHandoverManagementSapProvider() override;

    friend class MemberNrHandoverManagementSapProvider<NrLeoA3MeasurementHandoverAlgorithm>;

  protected:
    void DoInitialize() override;
    void DoDispose() override;
    void DoReportUeMeas(uint16_t rnti, NrRrcSap::MeasResults measResults) override;

  private:
    static uint8_t ConvertReportIntervalMsToEnumValue(uint16_t reportIntervalMs);

    bool IsValidNeighbour(uint16_t cellId) const;

    std::vector<uint8_t> m_measIds;
    double m_hysteresisDb;
    Time m_timeToTrigger;
    uint16_t m_reportIntervalMs;
    uint8_t m_maxReportCells;
    NrHandoverManagementSapUser* m_handoverManagementSapUser;
    NrHandoverManagementSapProvider* m_handoverManagementSapProvider;
};

} // namespace ns3

#endif /* NR_LEO_A3_MEASUREMENT_HANDOVER_ALGORITHM_H */
