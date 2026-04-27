#ifndef LEO_NTN_HANDOVER_RADIO_H
#define LEO_NTN_HANDOVER_RADIO_H

/*
 * 文件说明：
 * `leo-ntn-handover-radio.h` 负责收纳 baseline 场景仍会复用的无线配置与
 * NR/EPC bootstrap 逻辑。
 *
 * 设计目标：
 * - 将天线/波束模式解析与 NR helper 初始化从主脚本抽离；
 * - 保持当前 baseline 默认无线口径不变；
 * - 让 `main()` 更聚焦于场景装配顺序，而不是无线样板代码。
 */

#include "b00-equivalent-antenna-model.h"
#include "earth-fixed-gnb-beamforming.h"
#include "leo-ntn-handover-config.h"

#include "ns3/antenna-module.h"
#include "ns3/core-module.h"
#include "ns3/nr-gnb-rrc.h"
#include "ns3/nr-module.h"

#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace ns3
{

enum class AntennaElementMode
{
    ISOTROPIC,
    THREE_GPP,
    B00_CUSTOM,
};

inline AntennaElementMode
ParseAntennaElementMode(const std::string& mode)
{
    if (mode == "three-gpp")
    {
        return AntennaElementMode::THREE_GPP;
    }
    if (mode == "b00-custom")
    {
        return AntennaElementMode::B00_CUSTOM;
    }
    return AntennaElementMode::ISOTROPIC;
}

inline const char*
ToString(AntennaElementMode mode)
{
    if (mode == AntennaElementMode::THREE_GPP)
    {
        return "three-gpp";
    }
    if (mode == AntennaElementMode::B00_CUSTOM)
    {
        return "b00-custom";
    }
    return "isotropic";
}

inline Ptr<AntennaModel>
CreateAntennaElement(AntennaElementMode mode, const BaselineSimulationConfig& config)
{
    if (mode == AntennaElementMode::THREE_GPP)
    {
        return CreateObject<ThreeGppAntennaModel>();
    }
    if (mode == AntennaElementMode::B00_CUSTOM)
    {
        Ptr<B00EquivalentAntennaModel> model = CreateObject<B00EquivalentAntennaModel>();
        model->SetMaxGainDb(config.b00MaxGainDb);
        model->SetBeamwidthDeg(config.b00BeamwidthDeg);
        model->SetMaxAttenuationDb(config.b00MaxAttenuationDb);
        return model;
    }
    return CreateObject<IsotropicAntennaModel>();
}

enum class BeamformingMode
{
    IDEAL_DIRECT_PATH,
    IDEAL_EARTH_FIXED,
    IDEAL_CELL_SCAN,
    IDEAL_DIRECT_PATH_QUASI_OMNI,
    IDEAL_CELL_SCAN_QUASI_OMNI,
    IDEAL_QUASI_OMNI_DIRECT_PATH,
    REALISTIC,
};

inline BeamformingMode
ParseBeamformingMode(const std::string& mode)
{
    if (mode == "ideal-earth-fixed")
    {
        return BeamformingMode::IDEAL_EARTH_FIXED;
    }
    if (mode == "ideal-cell-scan")
    {
        return BeamformingMode::IDEAL_CELL_SCAN;
    }
    if (mode == "ideal-direct-path-quasi-omni")
    {
        return BeamformingMode::IDEAL_DIRECT_PATH_QUASI_OMNI;
    }
    if (mode == "ideal-cell-scan-quasi-omni")
    {
        return BeamformingMode::IDEAL_CELL_SCAN_QUASI_OMNI;
    }
    if (mode == "ideal-quasi-omni-direct-path")
    {
        return BeamformingMode::IDEAL_QUASI_OMNI_DIRECT_PATH;
    }
    if (mode == "realistic")
    {
        return BeamformingMode::REALISTIC;
    }
    return BeamformingMode::IDEAL_DIRECT_PATH;
}

inline const char*
ToString(BeamformingMode mode)
{
    switch (mode)
    {
    case BeamformingMode::IDEAL_EARTH_FIXED:
        return "ideal-earth-fixed";
    case BeamformingMode::IDEAL_CELL_SCAN:
        return "ideal-cell-scan";
    case BeamformingMode::IDEAL_DIRECT_PATH_QUASI_OMNI:
        return "ideal-direct-path-quasi-omni";
    case BeamformingMode::IDEAL_CELL_SCAN_QUASI_OMNI:
        return "ideal-cell-scan-quasi-omni";
    case BeamformingMode::IDEAL_QUASI_OMNI_DIRECT_PATH:
        return "ideal-quasi-omni-direct-path";
    case BeamformingMode::REALISTIC:
        return "realistic";
    case BeamformingMode::IDEAL_DIRECT_PATH:
    default:
        return "ideal-direct-path";
    }
}

inline TypeId
GetIdealBeamformingMethodTypeId(BeamformingMode mode)
{
    switch (mode)
    {
    case BeamformingMode::IDEAL_EARTH_FIXED:
        return EarthFixedGnbBeamforming::GetTypeId();
    case BeamformingMode::IDEAL_CELL_SCAN:
        return CellScanBeamforming::GetTypeId();
    case BeamformingMode::IDEAL_DIRECT_PATH_QUASI_OMNI:
        return DirectPathQuasiOmniBeamforming::GetTypeId();
    case BeamformingMode::IDEAL_CELL_SCAN_QUASI_OMNI:
        return CellScanQuasiOmniBeamforming::GetTypeId();
    case BeamformingMode::IDEAL_QUASI_OMNI_DIRECT_PATH:
        return QuasiOmniDirectPathBeamforming::GetTypeId();
    case BeamformingMode::IDEAL_DIRECT_PATH:
    default:
        return DirectPathBeamforming::GetTypeId();
    }
}

inline RealisticBfManager::TriggerEvent
ParseRealisticBfTriggerEvent(const std::string& triggerEvent)
{
    if (triggerEvent == "delayed-update")
    {
        return RealisticBfManager::DELAYED_UPDATE;
    }
    return RealisticBfManager::SRS_COUNT;
}

inline const char*
ToString(RealisticBfManager::TriggerEvent triggerEvent)
{
    return triggerEvent == RealisticBfManager::DELAYED_UPDATE ? "delayed-update" : "srs-count";
}

struct RadioBootstrapModes
{
    AntennaElementMode gnbAntennaElementMode = AntennaElementMode::ISOTROPIC;
    AntennaElementMode ueAntennaElementMode = AntennaElementMode::ISOTROPIC;
    BeamformingMode beamformingMode = BeamformingMode::IDEAL_DIRECT_PATH;
    RealisticBfManager::TriggerEvent realisticBfTriggerEvent = RealisticBfManager::SRS_COUNT;
    bool useRealisticBeamforming = false;
    uint32_t effectiveSrsSymbols = 0;
};

inline RadioBootstrapModes
ResolveRadioBootstrapModes(const BaselineSimulationConfig& config)
{
    RadioBootstrapModes modes;
    modes.gnbAntennaElementMode = ParseAntennaElementMode(config.gnbAntennaElement);
    modes.ueAntennaElementMode = ParseAntennaElementMode(config.ueAntennaElement);
    modes.beamformingMode = ParseBeamformingMode(config.beamformingMode);
    modes.realisticBfTriggerEvent = ParseRealisticBfTriggerEvent(config.realisticBfTriggerEvent);
    modes.useRealisticBeamforming = (modes.beamformingMode == BeamformingMode::REALISTIC);
    modes.effectiveSrsSymbols =
        (modes.useRealisticBeamforming && config.srsSymbols == 0) ? 1u : config.srsSymbols;
    return modes;
}

struct NrRadioBootstrap
{
    Ptr<NrHelper> nrHelper;
    Ptr<NrPointToPointEpcHelper> nrEpcHelper;
    std::unique_ptr<OperationBandInfo> band;
    BandwidthPartInfoPtrVector allBwps;
};

inline NrRadioBootstrap
BuildNrRadioBootstrap(const BaselineSimulationConfig& config,
                      const RadioBootstrapModes& modes,
                      EarthFixedGnbBeamforming::AnchorResolver anchorResolver)
{
    Config::SetDefault("ns3::ThreeGppChannelModel::UpdatePeriod", TimeValue(MilliSeconds(10)));
    Config::SetDefault("ns3::NrUePhy::UeMeasurementsFilterPeriod", TimeValue(MilliSeconds(50)));
    Config::SetDefault("ns3::NrAnr::Threshold", UintegerValue(0));
    if (config.forceRlcAmForEpc)
    {
        Config::SetDefault("ns3::NrGnbRrc::EpsBearerToRlcMapping",
                           EnumValue(NrGnbRrc::RLC_AM_ALWAYS));
    }

    NrRadioBootstrap bootstrap;
    bootstrap.nrHelper = CreateObject<NrHelper>();
    bootstrap.nrEpcHelper = CreateObject<NrPointToPointEpcHelper>();
    bootstrap.nrEpcHelper->SetAttribute("S1uLinkDelay", TimeValue(MilliSeconds(0)));
    bootstrap.nrHelper->SetEpcHelper(bootstrap.nrEpcHelper);

    Ptr<NrChannelHelper> channelHelper = CreateObject<NrChannelHelper>();
    channelHelper->ConfigureFactories("NTN-Rural", "LOS", "ThreeGpp");
    channelHelper->SetPathlossAttribute("ShadowingEnabled",
                                        BooleanValue(config.shadowingEnabled));

    CcBwpCreator ccBwpCreator;
    CcBwpCreator::SimpleOperationBandConf bandConf(config.centralFrequency, config.bandwidth, 1);
    bootstrap.band =
        std::make_unique<OperationBandInfo>(ccBwpCreator.CreateOperationBandContiguousCc(bandConf));
    std::vector<std::reference_wrapper<OperationBandInfo>> bandRefs{std::ref(*bootstrap.band)};
    channelHelper->AssignChannelsToBands(bandRefs);
    bootstrap.allBwps = CcBwpCreator::GetAllBwps({*bootstrap.band});

    bootstrap.nrHelper->SetGnbPhyAttribute("TxPower", DoubleValue(config.gnbTxPower));
    bootstrap.nrHelper->SetUePhyAttribute("TxPower", DoubleValue(config.ueTxPower));

    bootstrap.nrHelper->SetSchedulerTypeId(TypeId::LookupByName("ns3::NrMacSchedulerOfdmaRR"));
    bootstrap.nrHelper->SetSchedulerAttribute("EnableSrsInFSlots",
                                              BooleanValue(config.enableSrsInFSlots));
    bootstrap.nrHelper->SetSchedulerAttribute("EnableSrsInUlSlots",
                                              BooleanValue(config.enableSrsInUlSlots));
    bootstrap.nrHelper->SetSchedulerAttribute("SrsSymbols",
                                              UintegerValue(modes.effectiveSrsSymbols));
    bootstrap.nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(config.ueAntennaRows));
    bootstrap.nrHelper->SetUeAntennaAttribute("NumColumns",
                                              UintegerValue(config.ueAntennaColumns));
    bootstrap.nrHelper->SetUeAntennaAttribute(
        "AntennaElement",
        PointerValue(CreateAntennaElement(modes.ueAntennaElementMode, config)));
    bootstrap.nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(config.gnbAntennaRows));
    bootstrap.nrHelper->SetGnbAntennaAttribute("NumColumns",
                                               UintegerValue(config.gnbAntennaColumns));
    bootstrap.nrHelper->SetGnbAntennaAttribute(
        "AntennaElement",
        PointerValue(CreateAntennaElement(modes.gnbAntennaElementMode, config)));

    EarthFixedGnbBeamforming::SetAnchorResolver(std::move(anchorResolver));
    Ptr<BeamformingHelperBase> beamformingHelper;
    if (modes.useRealisticBeamforming)
    {
        Ptr<RealisticBeamformingHelper> realisticBeamformingHelper =
            CreateObject<RealisticBeamformingHelper>();
        realisticBeamformingHelper->SetBeamformingMethod(
            RealisticBeamformingAlgorithm::GetTypeId());
        bootstrap.nrHelper->SetGnbBeamManagerTypeId(RealisticBfManager::GetTypeId());
        bootstrap.nrHelper->SetGnbBeamManagerAttribute(
            "TriggerEvent", EnumValue(modes.realisticBfTriggerEvent));
        bootstrap.nrHelper->SetGnbBeamManagerAttribute(
            "UpdatePeriodicity", UintegerValue(config.realisticBfUpdatePeriodicity));
        bootstrap.nrHelper->SetGnbBeamManagerAttribute(
            "UpdateDelay", TimeValue(MilliSeconds(config.realisticBfUpdateDelayMs)));
        beamformingHelper = realisticBeamformingHelper;
    }
    else
    {
        Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject<IdealBeamformingHelper>();
        idealBeamformingHelper->SetAttribute(
            "BeamformingMethod",
            TypeIdValue(GetIdealBeamformingMethodTypeId(modes.beamformingMode)));
        idealBeamformingHelper->SetAttribute(
            "BeamformingPeriodicity",
            TimeValue(MilliSeconds(config.beamformingPeriodicityMs)));
        beamformingHelper = idealBeamformingHelper;
    }
    bootstrap.nrHelper->SetBeamformingHelper(beamformingHelper);

    return bootstrap;
}

} // namespace ns3

#endif // LEO_NTN_HANDOVER_RADIO_H
