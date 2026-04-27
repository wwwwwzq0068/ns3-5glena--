#ifndef LEO_NTN_HANDOVER_SCENARIO_H
#define LEO_NTN_HANDOVER_SCENARIO_H

/*
 * 文件说明：
 * `leo-ntn-handover-scenario.h` 负责集中保存场景装配阶段仍会复用的 UE
 * 初始接入与业务安装逻辑。
 *
 * 设计目标：
 * - 将每个 UE 的初始服务星选择、attach、默认路由和业务安装从主脚本抽离；
 * - 保持七小区 baseline 的初始接入口径不变；
 * - 让 `main()` 更聚焦于场景阶段划分，而不是逐 UE 装配细节。
 */

#include "beam-link-budget.h"
#include "earth-fixed-beam-target.h"
#include "leo-ntn-handover-config.h"
#include "leo-ntn-handover-runtime.h"
#include "leo-orbit-calculator.h"
#include "wgs84-hex-grid.h"

#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <vector>

namespace ns3
{

struct UeScenarioInstallContext
{
    const std::vector<SatelliteRuntime>& satellites;
    std::vector<UeRuntime>& ues;
    std::map<uint64_t, uint32_t>& imsiToUe;
    const std::vector<Wgs84HexGridCell>* hexGridCells = nullptr;
    double gmstAtEpochRad = 0.0;
    double minElevationRad = 0.0;
    BeamModelConfig beamModelConfig;
    Vector (*resolveSatelliteBeamTargetEcef)(uint32_t satIdx, const Vector& satEcef) = nullptr;
};

struct UeScenarioInstallInputs
{
    UeScenarioInstallInputs(const NodeContainer& ueNodesIn,
                            const NetDeviceContainer& ueNetDevIn,
                            const NetDeviceContainer& gNbNetDevIn,
                            const std::vector<UePlacement>& uePlacementsIn,
                            const Ipv4InterfaceContainer& ueIpIfaceIn,
                            Ptr<Node> remoteHostIn,
                            Ptr<NrHelper> nrHelperIn,
                            Ptr<NrPointToPointEpcHelper> nrEpcHelperIn,
                            Ipv4StaticRoutingHelper& routingHelperIn)
        : ueNodes(ueNodesIn),
          ueNetDev(ueNetDevIn),
          gNbNetDev(gNbNetDevIn),
          uePlacements(uePlacementsIn),
          ueIpIface(ueIpIfaceIn),
          remoteHost(remoteHostIn),
          nrHelper(nrHelperIn),
          nrEpcHelper(nrEpcHelperIn),
          routingHelper(routingHelperIn)
    {
    }

    const NodeContainer& ueNodes;
    const NetDeviceContainer& ueNetDev;
    const NetDeviceContainer& gNbNetDev;
    const std::vector<UePlacement>& uePlacements;
    const Ipv4InterfaceContainer& ueIpIface;
    Ptr<Node> remoteHost;
    Ptr<NrHelper> nrHelper;
    Ptr<NrPointToPointEpcHelper> nrEpcHelper;
    Ipv4StaticRoutingHelper& routingHelper;
};

struct NoopUeTransportBootstrap
{
    void operator()(const Ptr<NrUeNetDevice>&) const
    {
    }
};

inline void
DisableUeIpv4ForwardingIfRequested(const BaselineSimulationConfig& config, const NodeContainer& ueNodes)
{
    if (!config.disableUeIpv4Forwarding)
    {
        return;
    }

    for (uint32_t ueIdx = 0; ueIdx < ueNodes.GetN(); ++ueIdx)
    {
        Ptr<Ipv4> ueIpv4 = ueNodes.Get(ueIdx)->GetObject<Ipv4>();
        if (!ueIpv4)
        {
            continue;
        }
        for (uint32_t ifIdx = 0; ifIdx < ueIpv4->GetNInterfaces(); ++ifIdx)
        {
            ueIpv4->SetForwarding(ifIdx, false);
        }
    }
}

inline bool
IsUeInInitialAnchorCell(const UeScenarioInstallContext& context,
                        const BaselineSimulationConfig& config,
                        const UeRuntime& ue,
                        uint32_t satIdx)
{
    const auto beamTargetMode = ParseEarthFixedBeamTargetMode(config.earthFixedBeamTargetMode);
    if (!config.enforceAnchorCellForRealLinks || !RequiresDiscreteAnchorCellGate(beamTargetMode))
    {
        return true;
    }
    if (context.hexGridCells == nullptr || context.hexGridCells->empty() ||
        context.satellites[satIdx].currentAnchorGridId == 0)
    {
        return false;
    }

    const auto nearestIndices = FindNearestHexCellIndices(*context.hexGridCells, ue.groundPoint.ecef, 1);
    if (nearestIndices.empty())
    {
        return false;
    }
    return (*context.hexGridCells)[nearestIndices.front()].id ==
           context.satellites[satIdx].currentAnchorGridId;
}

inline uint32_t
SelectInitialAttachSatellite(const UeScenarioInstallContext& context,
                             const BaselineSimulationConfig& config,
                             const UeRuntime& ue,
                             uint32_t ueIdx)
{
    const auto beamTargetMode = ParseEarthFixedBeamTargetMode(config.earthFixedBeamTargetMode);
    uint32_t initialAttachIdx = 0;
    uint32_t bestVisibleIdx = 0;
    uint32_t bestAnyIdx = 0;
    uint32_t bestEligibleIdx = 0;
    double bestEligibleRsrp = -std::numeric_limits<double>::infinity();
    double bestVisibleElevation = -std::numeric_limits<double>::infinity();
    double bestAnyElevation = -std::numeric_limits<double>::infinity();

    for (uint32_t satIdx = 0; satIdx < context.satellites.size(); ++satIdx)
    {
        const auto state = LeoOrbitCalculator::Calculate(0.0,
                                                         context.satellites[satIdx].orbit,
                                                         context.gmstAtEpochRad,
                                                         ue.groundPoint,
                                                         config.centralFrequency,
                                                         context.minElevationRad);
        const Vector beamTargetEcef =
            context.resolveSatelliteBeamTargetEcef
                ? context.resolveSatelliteBeamTargetEcef(satIdx, state.ecef)
                : ResolveEarthFixedBeamTarget(state.ecef,
                                              context.satellites[satIdx].cellAnchorEcef,
                                              beamTargetMode);
        const auto budget = CalculateEarthFixedBeamBudget(state.ecef,
                                                          ue.groundPoint.ecef,
                                                          beamTargetEcef,
                                                          context.beamModelConfig);

        const bool accessAllowed =
            !config.enforceBeamCoverageForRealLinks ||
            (budget.beamLocked &&
             budget.offBoresightAngleRad <= context.beamModelConfig.theta3dBRad &&
             IsUeInInitialAnchorCell(context, config, ue, satIdx));
        if (state.elevationRad > bestAnyElevation)
        {
            bestAnyElevation = state.elevationRad;
            bestAnyIdx = satIdx;
        }
        if (state.visible && state.elevationRad > bestVisibleElevation)
        {
            bestVisibleElevation = state.elevationRad;
            bestVisibleIdx = satIdx;
        }
        if (state.visible && accessAllowed && budget.rsrpDbm > bestEligibleRsrp)
        {
            bestEligibleRsrp = budget.rsrpDbm;
            bestEligibleIdx = satIdx;
        }
    }

    if (std::isfinite(bestEligibleRsrp))
    {
        initialAttachIdx = bestEligibleIdx;
    }
    else if (std::isfinite(bestVisibleElevation))
    {
        initialAttachIdx = bestVisibleIdx;
        if (config.startupVerbose)
        {
            std::cout << "[Setup] warning: ue" << ueIdx
                      << " visible satellites exist but none satisfy real-link access gate at t=0, "
                      << "fallback to highest-elevation visible sat" << initialAttachIdx
                      << std::endl;
        }
    }
    else
    {
        initialAttachIdx = bestAnyIdx;
        if (config.startupVerbose)
        {
            std::cout << "[Setup] warning: ue" << ueIdx
                      << " no visible satellite at t=0, attaching to highest-elevation sat"
                      << initialAttachIdx
                      << " (el=" << LeoOrbitCalculator::RadToDeg(bestAnyElevation) << "deg)"
                      << std::endl;
        }
    }
    return initialAttachIdx;
}

template <typename BootstrapTransportCallback>
inline void
InstallUeInitialAttachAndTraffic(UeScenarioInstallContext& context,
                                 const BaselineSimulationConfig& config,
                                 const NodeContainer& ueNodes,
                                 const NetDeviceContainer& ueNetDev,
                                 const NetDeviceContainer& gNbNetDev,
                                 const std::vector<UePlacement>& uePlacements,
                                 const Ipv4InterfaceContainer& ueIpIface,
                                 Ptr<Node> remoteHost,
                                 Ptr<NrHelper> nrHelper,
                                 Ptr<NrPointToPointEpcHelper> nrEpcHelper,
                                 Ipv4StaticRoutingHelper& routingHelper,
                                 BootstrapTransportCallback&& bootstrapTransportCallback)
{
    context.ues.clear();
    context.ues.reserve(config.ueNum);
    context.imsiToUe.clear();

    for (uint32_t ueIdx = 0; ueIdx < config.ueNum; ++ueIdx)
    {
        UeRuntime ue;
        ue.node = ueNodes.Get(ueIdx);
        ue.dev = DynamicCast<NrUeNetDevice>(ueNetDev.Get(ueIdx));
        ue.groundPoint = uePlacements[ueIdx].groundPoint;
        ue.placementRole = uePlacements[ueIdx].role;
        ue.eastOffsetMeters = uePlacements[ueIdx].eastOffsetMeters;
        ue.northOffsetMeters = uePlacements[ueIdx].northOffsetMeters;
        ResetUeRuntime(ue, config.gNbNum);

        ue.initialAttachIdx = SelectInitialAttachSatellite(context, config, ue, ueIdx);

        bootstrapTransportCallback(ue.dev);

        nrHelper->AttachToGnb(ueNetDev.Get(ueIdx), gNbNetDev.Get(ue.initialAttachIdx));

        Ptr<Ipv4StaticRouting> ueStaticRouting =
            routingHelper.GetStaticRouting(ueNodes.Get(ueIdx)->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(nrEpcHelper->GetUeDefaultGatewayAddress(), 1);

        const uint16_t dlPort = static_cast<uint16_t>(1234 + ueIdx);
        UdpServerHelper ueUdpServer(dlPort);
        ApplicationContainer serverApps = ueUdpServer.Install(ueNodes.Get(ueIdx));

        UdpClientHelper dlClient(ueIpIface.GetAddress(ueIdx), dlPort);
        dlClient.SetAttribute("PacketSize", UintegerValue(config.udpPacketSize));
        dlClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
        dlClient.SetAttribute("Interval", TimeValue(Seconds(1.0 / config.lambda)));
        ApplicationContainer clientApps = dlClient.Install(remoteHost);

        NrEpsBearer bearer(NrEpsBearer::GBR_CONV_VOICE);
        Ptr<NrEpcTft> tft = Create<NrEpcTft>();
        NrEpcTft::PacketFilter dlpf;
        dlpf.localPortStart = dlPort;
        dlpf.localPortEnd = dlPort;
        tft->Add(dlpf);
        nrHelper->ActivateDedicatedEpsBearer(ueNetDev.Get(ueIdx), bearer, tft);

        serverApps.Start(Seconds(config.appStartTime));
        clientApps.Start(Seconds(config.appStartTime));
        serverApps.Stop(Seconds(config.simTime));
        clientApps.Stop(Seconds(config.simTime));

        ue.dlPort = dlPort;
        ue.server = serverApps.Get(0)->GetObject<UdpServer>();
        if (ue.dev)
        {
            context.imsiToUe[ue.dev->GetImsi()] = ueIdx;
        }
        context.ues.push_back(ue);
    }
}

template <typename BootstrapTransportCallback>
inline void
InstallUeInitialAttachAndTraffic(UeScenarioInstallContext& context,
                                 const BaselineSimulationConfig& config,
                                 const UeScenarioInstallInputs& inputs,
                                 BootstrapTransportCallback&& bootstrapTransportCallback)
{
    InstallUeInitialAttachAndTraffic(context,
                                     config,
                                     inputs.ueNodes,
                                     inputs.ueNetDev,
                                     inputs.gNbNetDev,
                                     inputs.uePlacements,
                                     inputs.ueIpIface,
                                     inputs.remoteHost,
                                     inputs.nrHelper,
                                     inputs.nrEpcHelper,
                                     inputs.routingHelper,
                                     bootstrapTransportCallback);
}

inline void
InstallUeInitialAttachAndTraffic(UeScenarioInstallContext& context,
                                 const BaselineSimulationConfig& config,
                                 const UeScenarioInstallInputs& inputs)
{
    InstallUeInitialAttachAndTraffic(
        context, config, inputs, NoopUeTransportBootstrap{});
}

inline void
InstallUeInitialAttachAndTraffic(UeScenarioInstallContext& context,
                                 const BaselineSimulationConfig& config,
                                 const NodeContainer& ueNodes,
                                 const NetDeviceContainer& ueNetDev,
                                 const NetDeviceContainer& gNbNetDev,
                                 const std::vector<UePlacement>& uePlacements,
                                 const Ipv4InterfaceContainer& ueIpIface,
                                 Ptr<Node> remoteHost,
                                 Ptr<NrHelper> nrHelper,
                                 Ptr<NrPointToPointEpcHelper> nrEpcHelper,
                                 Ipv4StaticRoutingHelper& routingHelper)
{
    InstallUeInitialAttachAndTraffic(context,
                                     config,
                                     ueNodes,
                                     ueNetDev,
                                     gNbNetDev,
                                     uePlacements,
                                     ueIpIface,
                                     remoteHost,
                                     nrHelper,
                                     nrEpcHelper,
                                     routingHelper,
                                     NoopUeTransportBootstrap{});
}

} // namespace ns3

#endif // LEO_NTN_HANDOVER_SCENARIO_H
