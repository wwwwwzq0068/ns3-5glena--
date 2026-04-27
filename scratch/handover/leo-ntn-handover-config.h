#ifndef LEO_NTN_HANDOVER_CONFIG_H
#define LEO_NTN_HANDOVER_CONFIG_H

/*
 * 文件说明：
 * `leo-ntn-handover-config.h` 负责集中保存
 * `leo-ntn-handover-baseline.cc` 使用的默认参数和配置辅助函数。
 *
 * 设计目标：
 * - 将默认值、命令行绑定和参数合法性检查从主脚本中抽离；
 * - 保持 baseline 场景口径不变，只减少主流程里的样板代码；
 * - 让后续继续收紧主脚本时，有一个统一的配置入口。
 */

#include "earth-fixed-beam-target.h"
#include "leo-ntn-handover-utils.h"
#include "ns3/core-module.h"
#include <string>

namespace ns3
{

struct BaselineSimulationConfig
{
    double simTime = 60.0;
    double appStartTime = 1.0;

    uint16_t gNbNum = 8;
    uint32_t ueNum = 25;
    std::string ueLayoutType = "seven-cell";
    double ueSpacingMeters = 40000.0;
    double ueCenterSpacingMeters = 6000.0;
    double ueRingPointOffsetMeters = 5000.0;

    double satAltitudeMeters = 600000.0;
    double orbitEccentricity = 0.0;
    double orbitInclinationDeg = 53.0;
    double orbitRaanDeg = 84.9;
    double orbitArgPerigeeDeg = 0.0;
    uint32_t orbitPlaneCount = 2;
    double interPlaneRaanSpacingDeg = -0.58;
    double plane0RaanOffsetDeg = 1.09;
    double interPlaneTimeOffsetSeconds = 7.5;
    double plane0TimeOffsetSeconds = -3.5;
    double baseTrueAnomalyDeg = 0.0;
    double gmstAtEpochDeg = 0.0;
    bool autoAlignToUe = true;
    bool descendingPass = false;
    double alignmentReferenceTimeSeconds = 6.5;
    double overpassGapSeconds = 6.0;
    double plane1OverpassGapSeconds = 3.0;
    double overpassTimeOffsetSeconds = 0.0;
    double updateIntervalMs = 100.0;
    double minElevationDeg = 10.0;

    double ueLatitudeDeg = 45.6;
    double ueLongitudeDeg = 84.9;
    double ueAltitudeMeters = 0.0;
    bool lockCellAnchorToUe = true;
    double cellLatitudeDeg = 45.6;
    double cellLongitudeDeg = 84.9;
    double cellAltitudeMeters = 0.0;

    bool useWgs84HexGrid = true;
    bool lockGridCenterToUe = true;
    double gridCenterLatitudeDeg = 45.6;
    double gridCenterLongitudeDeg = 84.9;
    double gridWidthKm = 400.0;
    double gridHeightKm = 400.0;
    double anchorGridHexRadiusKm = 20.0;
    double hexCellRadiusKm = 20.0;
    uint32_t gridNearestK = 3;
    bool enforceBeamExclusionRing = true;
    uint32_t beamExclusionCandidateK = 64;
    bool enforceBeamCoverageForRealLinks = true;
    bool enforceAnchorCellForRealLinks = true;
    bool preferDemandAnchorCells = true;
    std::string anchorSelectionMode = "demand-max-ue-near-nadir";
    std::string demandSnapshotMode = "runtime-underserved-ue";
    double anchorGridSwitchGuardMeters = 0.0;
    double anchorGridHysteresisSeconds = 0.0;

    std::string outputDir = "scratch/results";
    bool printGridCatalog = true;
    std::string gridCatalogPath = JoinOutputPath(outputDir, "hex_grid_cells.csv");
    std::string plotHexGridScriptPath =
        std::string(PROJECT_SOURCE_PATH) + "/scratch/plotting/plot_hex_grid_svg.py";
    std::string satAnchorTracePath = JoinOutputPath(outputDir, "sat_anchor_trace.csv");
    std::string satGroundTrackTracePath = JoinOutputPath(outputDir, "sat_ground_track.csv");
    std::string ueLayoutPath = JoinOutputPath(outputDir, "ue_layout.csv");
    std::string gridSvgPath = JoinOutputPath(outputDir, "hex_grid_cells.svg");
    std::string gridHtmlPath = JoinOutputPath(outputDir, "hex_grid_cells.html");
    std::string handoverThroughputTracePath =
        JoinOutputPath(outputDir, "handover_dl_throughput_trace.csv");
    std::string handoverEventTracePath = JoinOutputPath(outputDir, "handover_event_trace.csv");
    std::string e2eFlowMetricsPath = JoinOutputPath(outputDir, "e2e_flow_metrics.csv");
    std::string phyDlTbMetricsPath = JoinOutputPath(outputDir, "phy_dl_tb_metrics.csv");
    std::string phyDlTbIntervalMetricsPath =
        JoinOutputPath(outputDir, "phy_dl_tb_interval_metrics.csv");
    std::string phyDlTbTracePath = JoinOutputPath(outputDir, "phy_dl_tb_trace.csv");
    double phyDlTbIntervalSeconds = 1.0;
    bool enablePhyDlTbTrace = false;

    double centralFrequency = 2e9;
    double bandwidth = 40e6;
    double lambda = 250.0;
    uint32_t udpPacketSize = 1000;
    double gnbTxPower = 100.0;
    double ueTxPower = 23.0;
    // 几何波束参数（v4.3 注：用于几何链路预算/观测口径，不代表 PHY 默认天线阵元参数）
    // beamMaxGainDbi/theta3dBDeg/sideLobeAttenuationDb 用于 BeamModelConfig 几何估算
    // PHY 真实天线参数由 gnbAntennaElement/ueAntennaElement 和 b00-* 参数控制
    double beamMaxGainDbi = 38.0;
    double scanMaxDeg = 60.0;
    double theta3dBDeg = 4.0;
    double sideLobeAttenuationDb = 30.0;
    double ueRxGainDbi = 0.0;
    double atmLossDb = 0.5;
    uint32_t ueAntennaRows = 1;
    uint32_t ueAntennaColumns = 2;
    uint32_t gnbAntennaRows = 12;
    uint32_t gnbAntennaColumns = 12;
    std::string ueAntennaElement = "three-gpp";      // v4.3 新默认：现实 NR 阵元口径
    std::string gnbAntennaElement = "b00-custom";    // v4.3 新默认：定向阵元口径
    // b00-custom antenna model parameters (v4.3 新增)
    double b00MaxGainDb = 20.0;           // 阵元峰值增益 (dBi)
    double b00BeamwidthDeg = 4.0;         // 阵元方向图宽度参数（默认收紧到约 20 km 主瓣半径）
    double b00MaxAttenuationDb = 30.0;    // 旁瓣衰减上限 (dB)
    std::string beamformingMode = "ideal-earth-fixed";
    std::string earthFixedBeamTargetMode = "grid-anchor";  // grid-anchor | nadir-continuous
    double beamformingPeriodicityMs = 100.0;
    std::string realisticBfTriggerEvent = "srs-count";
    uint16_t realisticBfUpdatePeriodicity = 3;
    double realisticBfUpdateDelayMs = 0.0;
    bool shadowingEnabled = true;

    double hoHysteresisDb = 2.0;
    uint32_t hoTttMs = 160;
    uint16_t measurementReportIntervalMs = 120;
    uint8_t measurementMaxReportCells = 8;
    std::string handoverMode = "baseline";
    double improvedSignalWeight = 0.7;
    double improvedRsrqWeight = 0.3;
    double improvedLoadWeight = 0.3;
    double improvedVisibilityWeight = 0.2;
    double improvedMinLoadScoreDelta = 0.2;
    double improvedMaxSignalGapDb = 3.0;
    double improvedMinStableLeadTimeSeconds = 0.12;
    double improvedMinVisibilitySeconds = 1.0;
    double improvedVisibilityHorizonSeconds = 8.0;
    double improvedVisibilityPredictionStepSeconds = 0.5;
    double improvedMinJointScoreMargin = 0.03;
    double improvedMinCandidateRsrpDbm = -110.0;
    double improvedMinCandidateRsrqDb = -17.0;
    double improvedServingWeakRsrpDbm = -108.0;
    double improvedServingWeakRsrqDb = -15.0;
    double improvedMinRsrqAdvantageDb = 0.0;  // Minimum RSRQ advantage over serving for same-frequency candidates
    bool improvedEnableCrossLayerPhyAssist = false;
    double improvedCrossLayerPhyAlpha = 0.02;
    double improvedCrossLayerTblerThreshold = 0.48;
    double improvedCrossLayerSinrThresholdDb = -5.0;
    uint32_t improvedCrossLayerMinSamples = 50;
    double pingPongWindowSeconds = 1.5;
    bool forceRlcAmForEpc = false;
    bool disableUeIpv4Forwarding = true;
    bool compactReport = true;
    bool printGridAnchorEvents = false;
    bool printKpiReports = false;
    bool printNrtEvents = false;
    bool printOrbitCheck = false;
    bool printRrcStateTransitions = false;
    bool startupVerbose = false;
    double kpiIntervalSeconds = 2.0;
    bool printSimulationProgress = true;
    double progressReportIntervalSeconds = 2.0;
    bool runGridSvgScript = true;
    double throughputReportIntervalSeconds = 0.0;
    bool enableHandoverThroughputTrace = true;
    double handoverThroughputTraceIntervalSeconds = 0.005;
    double maxSupportedUesPerSatellite = 5.0;
    double loadCongestionThreshold = 0.8;
    bool enableSrsInFSlots = false;
    bool enableSrsInUlSlots = false;
    uint32_t srsSymbols = 0;
};

inline void
RegisterBaselineCommandLineOptions(CommandLine& cmd, BaselineSimulationConfig& config)
{
    auto addArg = [&](const char* name, auto& value) { cmd.AddValue(name, "", value); };

    addArg("simTime", config.simTime);
    addArg("appStartTime", config.appStartTime);
    addArg("gNbNum", config.gNbNum);
    addArg("ueNum", config.ueNum);
    addArg("ueLayoutType", config.ueLayoutType);
    addArg("ueSpacingMeters", config.ueSpacingMeters);
    addArg("ueCenterSpacingMeters", config.ueCenterSpacingMeters);
    addArg("ueRingPointOffsetMeters", config.ueRingPointOffsetMeters);
    addArg("satAltitudeMeters", config.satAltitudeMeters);
    addArg("orbitEccentricity", config.orbitEccentricity);
    addArg("orbitInclinationDeg", config.orbitInclinationDeg);
    addArg("orbitRaanDeg", config.orbitRaanDeg);
    addArg("orbitArgPerigeeDeg", config.orbitArgPerigeeDeg);
    addArg("orbitPlaneCount", config.orbitPlaneCount);
    addArg("interPlaneRaanSpacingDeg", config.interPlaneRaanSpacingDeg);
    addArg("plane0RaanOffsetDeg", config.plane0RaanOffsetDeg);
    addArg("interPlaneTimeOffsetSeconds", config.interPlaneTimeOffsetSeconds);
    addArg("plane0TimeOffsetSeconds", config.plane0TimeOffsetSeconds);
    addArg("baseTrueAnomalyDeg", config.baseTrueAnomalyDeg);
    addArg("gmstAtEpochDeg", config.gmstAtEpochDeg);
    addArg("autoAlignToUe", config.autoAlignToUe);
    addArg("descendingPass", config.descendingPass);
    addArg("alignmentReferenceTimeSeconds", config.alignmentReferenceTimeSeconds);
    addArg("overpassGapSeconds", config.overpassGapSeconds);
    addArg("plane1OverpassGapSeconds", config.plane1OverpassGapSeconds);
    addArg("overpassTimeOffsetSeconds", config.overpassTimeOffsetSeconds);
    addArg("updateIntervalMs", config.updateIntervalMs);
    addArg("minElevationDeg", config.minElevationDeg);
    addArg("ueLatitudeDeg", config.ueLatitudeDeg);
    addArg("ueLongitudeDeg", config.ueLongitudeDeg);
    addArg("ueAltitudeMeters", config.ueAltitudeMeters);
    addArg("lockCellAnchorToUe", config.lockCellAnchorToUe);
    addArg("cellLatitudeDeg", config.cellLatitudeDeg);
    addArg("cellLongitudeDeg", config.cellLongitudeDeg);
    addArg("cellAltitudeMeters", config.cellAltitudeMeters);
    addArg("useWgs84HexGrid", config.useWgs84HexGrid);
    addArg("lockGridCenterToUe", config.lockGridCenterToUe);
    addArg("gridCenterLatitudeDeg", config.gridCenterLatitudeDeg);
    addArg("gridCenterLongitudeDeg", config.gridCenterLongitudeDeg);
    addArg("gridWidthKm", config.gridWidthKm);
    addArg("gridHeightKm", config.gridHeightKm);
    addArg("anchorGridHexRadiusKm", config.anchorGridHexRadiusKm);
    addArg("hexCellRadiusKm", config.hexCellRadiusKm);
    addArg("gridNearestK", config.gridNearestK);
    addArg("enforceBeamExclusionRing", config.enforceBeamExclusionRing);
    addArg("beamExclusionCandidateK", config.beamExclusionCandidateK);
    addArg("enforceBeamCoverageForRealLinks", config.enforceBeamCoverageForRealLinks);
    addArg("enforceAnchorCellForRealLinks", config.enforceAnchorCellForRealLinks);
    addArg("preferDemandAnchorCells", config.preferDemandAnchorCells);
    addArg("anchorSelectionMode", config.anchorSelectionMode);
    addArg("demandSnapshotMode", config.demandSnapshotMode);
    addArg("anchorGridSwitchGuardMeters", config.anchorGridSwitchGuardMeters);
    addArg("anchorGridHysteresisSeconds", config.anchorGridHysteresisSeconds);
    addArg("outputDir", config.outputDir);
    addArg("printGridCatalog", config.printGridCatalog);
    addArg("gridCatalogPath", config.gridCatalogPath);
    addArg("centralFrequency", config.centralFrequency);
    addArg("bandwidth", config.bandwidth);
    addArg("lambda", config.lambda);
    addArg("udpPacketSize", config.udpPacketSize);
    addArg("gnbTxPower", config.gnbTxPower);
    addArg("ueTxPower", config.ueTxPower);
    addArg("beamMaxGainDbi", config.beamMaxGainDbi);
    addArg("scanMaxDeg", config.scanMaxDeg);
    addArg("theta3dBDeg", config.theta3dBDeg);
    addArg("sideLobeAttenuationDb", config.sideLobeAttenuationDb);
    addArg("ueRxGainDbi", config.ueRxGainDbi);
    addArg("atmLossDb", config.atmLossDb);
    addArg("ueAntennaRows", config.ueAntennaRows);
    addArg("ueAntennaColumns", config.ueAntennaColumns);
    addArg("gnbAntennaRows", config.gnbAntennaRows);
    addArg("gnbAntennaColumns", config.gnbAntennaColumns);
    addArg("ueAntennaElement", config.ueAntennaElement);
    addArg("gnbAntennaElement", config.gnbAntennaElement);
    addArg("b00MaxGainDb", config.b00MaxGainDb);
    addArg("b00BeamwidthDeg", config.b00BeamwidthDeg);
    addArg("b00MaxAttenuationDb", config.b00MaxAttenuationDb);
    addArg("beamformingMode", config.beamformingMode);
    addArg("earthFixedBeamTargetMode", config.earthFixedBeamTargetMode);
    addArg("beamformingPeriodicityMs", config.beamformingPeriodicityMs);
    addArg("realisticBfTriggerEvent", config.realisticBfTriggerEvent);
    addArg("realisticBfUpdatePeriodicity", config.realisticBfUpdatePeriodicity);
    addArg("realisticBfUpdateDelayMs", config.realisticBfUpdateDelayMs);
    addArg("shadowingEnabled", config.shadowingEnabled);
    addArg("hoHysteresisDb", config.hoHysteresisDb);
    addArg("hoTttMs", config.hoTttMs);
    addArg("measurementReportIntervalMs", config.measurementReportIntervalMs);
    addArg("measurementMaxReportCells", config.measurementMaxReportCells);
    addArg("handoverMode", config.handoverMode);
    addArg("improvedSignalWeight", config.improvedSignalWeight);
    addArg("improvedRsrqWeight", config.improvedRsrqWeight);
    addArg("improvedLoadWeight", config.improvedLoadWeight);
    addArg("improvedVisibilityWeight", config.improvedVisibilityWeight);
    addArg("improvedMinLoadScoreDelta", config.improvedMinLoadScoreDelta);
    addArg("improvedMaxSignalGapDb", config.improvedMaxSignalGapDb);
    addArg("improvedMinStableLeadTimeSeconds", config.improvedMinStableLeadTimeSeconds);
    addArg("improvedMinVisibilitySeconds", config.improvedMinVisibilitySeconds);
    addArg("improvedVisibilityHorizonSeconds", config.improvedVisibilityHorizonSeconds);
    addArg("improvedVisibilityPredictionStepSeconds", config.improvedVisibilityPredictionStepSeconds);
    addArg("improvedMinJointScoreMargin", config.improvedMinJointScoreMargin);
    addArg("improvedMinCandidateRsrpDbm", config.improvedMinCandidateRsrpDbm);
    addArg("improvedMinCandidateRsrqDb", config.improvedMinCandidateRsrqDb);
    addArg("improvedServingWeakRsrpDbm", config.improvedServingWeakRsrpDbm);
    addArg("improvedServingWeakRsrqDb", config.improvedServingWeakRsrqDb);
    addArg("improvedMinRsrqAdvantageDb", config.improvedMinRsrqAdvantageDb);
    addArg("improvedEnableCrossLayerPhyAssist", config.improvedEnableCrossLayerPhyAssist);
    addArg("improvedCrossLayerPhyAlpha", config.improvedCrossLayerPhyAlpha);
    addArg("improvedCrossLayerTblerThreshold", config.improvedCrossLayerTblerThreshold);
    addArg("improvedCrossLayerSinrThresholdDb", config.improvedCrossLayerSinrThresholdDb);
    addArg("improvedCrossLayerMinSamples", config.improvedCrossLayerMinSamples);
    addArg("pingPongWindowSeconds", config.pingPongWindowSeconds);
    addArg("forceRlcAmForEpc", config.forceRlcAmForEpc);
    addArg("disableUeIpv4Forwarding", config.disableUeIpv4Forwarding);
    addArg("compactReport", config.compactReport);
    addArg("printGridAnchorEvents", config.printGridAnchorEvents);
    addArg("printKpiReports", config.printKpiReports);
    addArg("printNrtEvents", config.printNrtEvents);
    addArg("printOrbitCheck", config.printOrbitCheck);
    addArg("printRrcStateTransitions", config.printRrcStateTransitions);
    addArg("startupVerbose", config.startupVerbose);
    addArg("kpiIntervalSeconds", config.kpiIntervalSeconds);
    addArg("printSimulationProgress", config.printSimulationProgress);
    addArg("progressReportIntervalSeconds", config.progressReportIntervalSeconds);
    addArg("runGridSvgScript", config.runGridSvgScript);
    addArg("plotHexGridScriptPath", config.plotHexGridScriptPath);
    addArg("satAnchorTracePath", config.satAnchorTracePath);
    addArg("satGroundTrackTracePath", config.satGroundTrackTracePath);
    addArg("ueLayoutPath", config.ueLayoutPath);
    addArg("gridSvgPath", config.gridSvgPath);
    addArg("gridHtmlPath", config.gridHtmlPath);
    addArg("throughputReportIntervalSeconds", config.throughputReportIntervalSeconds);
    addArg("handoverThroughputTracePath", config.handoverThroughputTracePath);
    addArg("handoverEventTracePath", config.handoverEventTracePath);
    addArg("e2eFlowMetricsPath", config.e2eFlowMetricsPath);
    addArg("phyDlTbMetricsPath", config.phyDlTbMetricsPath);
    addArg("phyDlTbIntervalMetricsPath", config.phyDlTbIntervalMetricsPath);
    addArg("phyDlTbTracePath", config.phyDlTbTracePath);
    addArg("phyDlTbIntervalSeconds", config.phyDlTbIntervalSeconds);
    addArg("enablePhyDlTbTrace", config.enablePhyDlTbTrace);
    addArg("enableHandoverThroughputTrace", config.enableHandoverThroughputTrace);
    addArg("handoverThroughputTraceIntervalSeconds", config.handoverThroughputTraceIntervalSeconds);
    addArg("maxSupportedUesPerSatellite", config.maxSupportedUesPerSatellite);
    addArg("loadCongestionThreshold", config.loadCongestionThreshold);
    addArg("enableSrsInFSlots", config.enableSrsInFSlots);
    addArg("enableSrsInUlSlots", config.enableSrsInUlSlots);
    addArg("srsSymbols", config.srsSymbols);
}

inline void
ResolveBaselineOutputPaths(BaselineSimulationConfig& config)
{
    const std::string defaultOutputDir = "scratch/results";
    const std::string defaultGridCatalogPath = JoinOutputPath(defaultOutputDir, "hex_grid_cells.csv");
    const std::string defaultSatAnchorTracePath =
        JoinOutputPath(defaultOutputDir, "sat_anchor_trace.csv");
    const std::string defaultSatGroundTrackTracePath =
        JoinOutputPath(defaultOutputDir, "sat_ground_track.csv");
    const std::string defaultUeLayoutPath = JoinOutputPath(defaultOutputDir, "ue_layout.csv");
    const std::string defaultGridSvgPath = JoinOutputPath(defaultOutputDir, "hex_grid_cells.svg");
    const std::string defaultGridHtmlPath = JoinOutputPath(defaultOutputDir, "hex_grid_cells.html");
    const std::string defaultHandoverThroughputTracePath =
        JoinOutputPath(defaultOutputDir, "handover_dl_throughput_trace.csv");
    const std::string defaultHandoverEventTracePath =
        JoinOutputPath(defaultOutputDir, "handover_event_trace.csv");
    const std::string defaultE2eFlowMetricsPath =
        JoinOutputPath(defaultOutputDir, "e2e_flow_metrics.csv");
    const std::string defaultPhyDlTbMetricsPath =
        JoinOutputPath(defaultOutputDir, "phy_dl_tb_metrics.csv");
    const std::string defaultPhyDlTbIntervalMetricsPath =
        JoinOutputPath(defaultOutputDir, "phy_dl_tb_interval_metrics.csv");
    const std::string defaultPhyDlTbTracePath =
        JoinOutputPath(defaultOutputDir, "phy_dl_tb_trace.csv");

    if (config.gridCatalogPath == defaultGridCatalogPath && config.outputDir != defaultOutputDir)
    {
        config.gridCatalogPath = JoinOutputPath(config.outputDir, "hex_grid_cells.csv");
    }
    if (config.satAnchorTracePath == defaultSatAnchorTracePath && config.outputDir != defaultOutputDir)
    {
        config.satAnchorTracePath = JoinOutputPath(config.outputDir, "sat_anchor_trace.csv");
    }
    if (config.satGroundTrackTracePath == defaultSatGroundTrackTracePath &&
        config.outputDir != defaultOutputDir)
    {
        config.satGroundTrackTracePath = JoinOutputPath(config.outputDir, "sat_ground_track.csv");
    }
    if (config.ueLayoutPath == defaultUeLayoutPath && config.outputDir != defaultOutputDir)
    {
        config.ueLayoutPath = JoinOutputPath(config.outputDir, "ue_layout.csv");
    }
    if (config.gridSvgPath == defaultGridSvgPath && config.outputDir != defaultOutputDir)
    {
        config.gridSvgPath = JoinOutputPath(config.outputDir, "hex_grid_cells.svg");
    }
    if (config.gridHtmlPath == defaultGridHtmlPath && config.outputDir != defaultOutputDir)
    {
        config.gridHtmlPath = JoinOutputPath(config.outputDir, "hex_grid_cells.html");
    }
    if (config.handoverThroughputTracePath == defaultHandoverThroughputTracePath &&
        config.outputDir != defaultOutputDir)
    {
        config.handoverThroughputTracePath =
            JoinOutputPath(config.outputDir, "handover_dl_throughput_trace.csv");
    }
    if (config.handoverEventTracePath == defaultHandoverEventTracePath &&
        config.outputDir != defaultOutputDir)
    {
        config.handoverEventTracePath = JoinOutputPath(config.outputDir, "handover_event_trace.csv");
    }
    if (config.e2eFlowMetricsPath == defaultE2eFlowMetricsPath && config.outputDir != defaultOutputDir)
    {
        config.e2eFlowMetricsPath = JoinOutputPath(config.outputDir, "e2e_flow_metrics.csv");
    }
    if (config.phyDlTbMetricsPath == defaultPhyDlTbMetricsPath &&
        config.outputDir != defaultOutputDir)
    {
        config.phyDlTbMetricsPath = JoinOutputPath(config.outputDir, "phy_dl_tb_metrics.csv");
    }
    if (config.phyDlTbIntervalMetricsPath == defaultPhyDlTbIntervalMetricsPath &&
        config.outputDir != defaultOutputDir)
    {
        config.phyDlTbIntervalMetricsPath =
            JoinOutputPath(config.outputDir, "phy_dl_tb_interval_metrics.csv");
    }
    if (config.phyDlTbTracePath == defaultPhyDlTbTracePath && config.outputDir != defaultOutputDir)
    {
        config.phyDlTbTracePath = JoinOutputPath(config.outputDir, "phy_dl_tb_trace.csv");
    }

}

inline void
ApplyBaselineDerivedLocationConfig(BaselineSimulationConfig& config)
{
    if (config.lockCellAnchorToUe)
    {
        config.cellLatitudeDeg = config.ueLatitudeDeg;
        config.cellLongitudeDeg = config.ueLongitudeDeg;
        config.cellAltitudeMeters = config.ueAltitudeMeters;
    }

    if (config.lockGridCenterToUe)
    {
        config.gridCenterLatitudeDeg = config.ueLatitudeDeg;
        config.gridCenterLongitudeDeg = config.ueLongitudeDeg;
    }
}

inline void
ValidateBaselineSimulationConfig(BaselineSimulationConfig& config)
{
    NS_ABORT_MSG_IF(config.gNbNum < 2, "gNbNum must be >= 2 for handover validation");
    NS_ABORT_MSG_IF(config.ueNum == 0, "ueNum must be >= 1");
    NS_ABORT_MSG_IF(config.simTime <= 0.0, "simTime must be > 0");
    NS_ABORT_MSG_IF(config.appStartTime < 0.0, "appStartTime must be >= 0");
    NS_ABORT_MSG_IF(config.ueLayoutType != "line" && config.ueLayoutType != "seven-cell" &&
                        config.ueLayoutType != "r2-diagnostic",
                    "ueLayoutType must be one of: line, seven-cell, r2-diagnostic");
    NS_ABORT_MSG_IF(config.ueSpacingMeters <= 0.0, "ueSpacingMeters must be > 0");
    NS_ABORT_MSG_IF(config.ueCenterSpacingMeters <= 0.0, "ueCenterSpacingMeters must be > 0");
    NS_ABORT_MSG_IF(config.ueRingPointOffsetMeters <= 0.0,
                    "ueRingPointOffsetMeters must be > 0");
    NS_ABORT_MSG_IF(config.ueLayoutType == "seven-cell" && config.ueNum != 25,
                    "seven-cell layout currently requires ueNum == 25");
    NS_ABORT_MSG_IF(config.ueLayoutType == "r2-diagnostic" && config.ueNum != 19,
                    "r2-diagnostic layout currently requires ueNum == 19");
    NS_ABORT_MSG_IF(config.orbitPlaneCount == 0, "orbitPlaneCount must be >= 1");
    NS_ABORT_MSG_IF(config.gNbNum < config.orbitPlaneCount, "gNbNum must be >= orbitPlaneCount");
    NS_ABORT_MSG_IF(config.orbitEccentricity < 0.0 || config.orbitEccentricity >= 1.0,
                    "orbitEccentricity must satisfy 0 <= e < 1");
    NS_ABORT_MSG_IF(std::abs(config.interPlaneRaanSpacingDeg) >= 180.0,
                    "interPlaneRaanSpacingDeg must satisfy |value| < 180");
    NS_ABORT_MSG_IF(std::abs(config.plane0RaanOffsetDeg) >= 180.0,
                    "plane0RaanOffsetDeg must satisfy |value| < 180");
    NS_ABORT_MSG_IF(config.interPlaneTimeOffsetSeconds < 0.0,
                    "interPlaneTimeOffsetSeconds must be >= 0");
    NS_ABORT_MSG_IF(config.overpassGapSeconds <= 0.0, "overpassGapSeconds must be > 0");
    NS_ABORT_MSG_IF(config.plane1OverpassGapSeconds <= 0.0,
                    "plane1OverpassGapSeconds must be > 0");
    NS_ABORT_MSG_IF(config.scanMaxDeg <= 0.0 || config.scanMaxDeg >= 90.0,
                    "scanMaxDeg must satisfy 0 < scanMaxDeg < 90");
    NS_ABORT_MSG_IF(config.theta3dBDeg <= 0.0, "theta3dBDeg must be > 0");
    NS_ABORT_MSG_IF(config.ueAntennaRows == 0, "ueAntennaRows must be >= 1");
    NS_ABORT_MSG_IF(config.ueAntennaColumns == 0, "ueAntennaColumns must be >= 1");
    NS_ABORT_MSG_IF(config.gnbAntennaRows == 0, "gnbAntennaRows must be >= 1");
    NS_ABORT_MSG_IF(config.gnbAntennaColumns == 0, "gnbAntennaColumns must be >= 1");
    NS_ABORT_MSG_IF(config.ueAntennaElement != "isotropic" &&
                        config.ueAntennaElement != "three-gpp" &&
                        config.ueAntennaElement != "b00-custom",
                    "ueAntennaElement must be 'isotropic', 'three-gpp', or 'b00-custom'");
    NS_ABORT_MSG_IF(config.gnbAntennaElement != "isotropic" &&
                        config.gnbAntennaElement != "three-gpp" &&
                        config.gnbAntennaElement != "b00-custom",
                    "gnbAntennaElement must be 'isotropic', 'three-gpp', or 'b00-custom'");
    NS_ABORT_MSG_IF(config.beamformingMode != "ideal-direct-path" &&
                        config.beamformingMode != "ideal-earth-fixed" &&
                        config.beamformingMode != "ideal-cell-scan" &&
                        config.beamformingMode != "ideal-direct-path-quasi-omni" &&
                        config.beamformingMode != "ideal-cell-scan-quasi-omni" &&
                        config.beamformingMode != "ideal-quasi-omni-direct-path" &&
                        config.beamformingMode != "realistic",
                    "beamformingMode must be one of: ideal-direct-path, ideal-earth-fixed, ideal-cell-scan, ideal-direct-path-quasi-omni, ideal-cell-scan-quasi-omni, ideal-quasi-omni-direct-path, realistic");
    NS_ABORT_MSG_IF(config.earthFixedBeamTargetMode != "grid-anchor" &&
                        config.earthFixedBeamTargetMode != "nadir-continuous",
                    "earthFixedBeamTargetMode must be one of: grid-anchor, nadir-continuous");
    NS_ABORT_MSG_IF(config.beamformingPeriodicityMs < 0.0,
                    "beamformingPeriodicityMs must be >= 0");
    NS_ABORT_MSG_IF(config.realisticBfTriggerEvent != "srs-count" &&
                        config.realisticBfTriggerEvent != "delayed-update",
                    "realisticBfTriggerEvent must be either 'srs-count' or 'delayed-update'");
    NS_ABORT_MSG_IF(config.realisticBfUpdatePeriodicity == 0,
                    "realisticBfUpdatePeriodicity must be >= 1");
    NS_ABORT_MSG_IF(config.realisticBfUpdateDelayMs < 0.0,
                    "realisticBfUpdateDelayMs must be >= 0");
    NS_ABORT_MSG_IF(config.handoverMode != "baseline" && config.handoverMode != "improved",
                    "handoverMode must be either 'baseline' or 'improved'");
    NS_ABORT_MSG_IF(config.measurementMaxReportCells == 0,
                    "measurementMaxReportCells must be >= 1");
    NS_ABORT_MSG_IF(config.improvedSignalWeight < 0.0, "improvedSignalWeight must be >= 0");
    NS_ABORT_MSG_IF(config.improvedRsrqWeight < 0.0, "improvedRsrqWeight must be >= 0");
    NS_ABORT_MSG_IF(config.improvedLoadWeight < 0.0, "improvedLoadWeight must be >= 0");
    NS_ABORT_MSG_IF(config.improvedVisibilityWeight < 0.0,
                    "improvedVisibilityWeight must be >= 0");
    NS_ABORT_MSG_IF(config.improvedSignalWeight + config.improvedRsrqWeight +
                            config.improvedLoadWeight +
                            config.improvedVisibilityWeight <=
                        0.0,
                    "improvedSignalWeight + improvedRsrqWeight + improvedLoadWeight + improvedVisibilityWeight must be > 0");
    NS_ABORT_MSG_IF(config.improvedMinLoadScoreDelta < 0.0,
                    "improvedMinLoadScoreDelta must be >= 0");
    NS_ABORT_MSG_IF(config.improvedMaxSignalGapDb < 0.0,
                    "improvedMaxSignalGapDb must be >= 0");
    NS_ABORT_MSG_IF(config.improvedMinStableLeadTimeSeconds < 0.0,
                    "improvedMinStableLeadTimeSeconds must be >= 0");
    NS_ABORT_MSG_IF(config.improvedMinVisibilitySeconds < 0.0,
                    "improvedMinVisibilitySeconds must be >= 0");
    NS_ABORT_MSG_IF(config.improvedVisibilityHorizonSeconds <= 0.0,
                    "improvedVisibilityHorizonSeconds must be > 0");
    NS_ABORT_MSG_IF(config.improvedVisibilityPredictionStepSeconds <= 0.0,
                    "improvedVisibilityPredictionStepSeconds must be > 0");
    NS_ABORT_MSG_IF(config.improvedMinJointScoreMargin < 0.0,
                    "improvedMinJointScoreMargin must be >= 0");
    NS_ABORT_MSG_IF(config.improvedMinCandidateRsrpDbm > -44.0 ||
                        config.improvedMinCandidateRsrpDbm < -140.0,
                    "improvedMinCandidateRsrpDbm must be in [-140, -44]");
    NS_ABORT_MSG_IF(config.improvedMinCandidateRsrqDb > 3.0 ||
                        config.improvedMinCandidateRsrqDb < -19.5,
                    "improvedMinCandidateRsrqDb must be in [-19.5, 3]");
    NS_ABORT_MSG_IF(config.improvedServingWeakRsrpDbm > -44.0 ||
                        config.improvedServingWeakRsrpDbm < -140.0,
                    "improvedServingWeakRsrpDbm must be in [-140, -44]");
    NS_ABORT_MSG_IF(config.improvedServingWeakRsrqDb > 3.0 ||
                        config.improvedServingWeakRsrqDb < -19.5,
                    "improvedServingWeakRsrqDb must be in [-19.5, 3]");
    NS_ABORT_MSG_IF(config.improvedCrossLayerPhyAlpha <= 0.0 ||
                        config.improvedCrossLayerPhyAlpha > 1.0,
                    "improvedCrossLayerPhyAlpha must be in (0, 1]");
    NS_ABORT_MSG_IF(config.improvedCrossLayerTblerThreshold < 0.0 ||
                        config.improvedCrossLayerTblerThreshold > 1.0,
                    "improvedCrossLayerTblerThreshold must be in [0, 1]");
    NS_ABORT_MSG_IF(config.improvedCrossLayerMinSamples == 0,
                    "improvedCrossLayerMinSamples must be >= 1");
    NS_ABORT_MSG_IF(config.kpiIntervalSeconds <= 0.0, "kpiIntervalSeconds must be > 0");
    NS_ABORT_MSG_IF(config.gridWidthKm <= 0.0, "gridWidthKm must be > 0");
    NS_ABORT_MSG_IF(config.gridHeightKm <= 0.0, "gridHeightKm must be > 0");
    NS_ABORT_MSG_IF(config.anchorGridHexRadiusKm <= 0.0, "anchorGridHexRadiusKm must be > 0");
    NS_ABORT_MSG_IF(config.hexCellRadiusKm <= 0.0, "hexCellRadiusKm must be > 0");
    NS_ABORT_MSG_IF(config.gridNearestK == 0, "gridNearestK must be >= 1");
    NS_ABORT_MSG_IF(config.enforceBeamExclusionRing && config.beamExclusionCandidateK == 0,
                    "beamExclusionCandidateK must be >= 1 when enforceBeamExclusionRing is true");
    NS_ABORT_MSG_IF(config.anchorSelectionMode != "demand-nearest" &&
                        config.anchorSelectionMode != "demand-max-ue-near-nadir",
                    "anchorSelectionMode must be one of: demand-nearest, demand-max-ue-near-nadir");
    NS_ABORT_MSG_IF(config.demandSnapshotMode != "static-layout" &&
                        config.demandSnapshotMode != "runtime-underserved-ue",
                    "demandSnapshotMode must be one of: static-layout, runtime-underserved-ue");
    NS_ABORT_MSG_IF(config.anchorGridSwitchGuardMeters < 0.0,
                    "anchorGridSwitchGuardMeters must be >= 0");
    NS_ABORT_MSG_IF(config.anchorGridHysteresisSeconds < 0.0,
                    "anchorGridHysteresisSeconds must be >= 0");
    NS_ABORT_MSG_IF(config.outputDir.empty(), "outputDir must not be empty");
    NS_ABORT_MSG_IF(config.phyDlTbIntervalSeconds <= 0.0,
                    "phyDlTbIntervalSeconds must be > 0");
    NS_ABORT_MSG_IF(config.progressReportIntervalSeconds <= 0.0,
                    "progressReportIntervalSeconds must be > 0");
    NS_ABORT_MSG_IF(config.pingPongWindowSeconds <= 0.0,
                    "pingPongWindowSeconds must be > 0");
    NS_ABORT_MSG_IF(config.enableHandoverThroughputTrace &&
                        config.handoverThroughputTraceIntervalSeconds <= 0.0,
                    "handoverThroughputTraceIntervalSeconds must be > 0 when enabled");
    NS_ABORT_MSG_IF(config.maxSupportedUesPerSatellite <= 0.0,
                    "maxSupportedUesPerSatellite must be > 0");
    NS_ABORT_MSG_IF(config.loadCongestionThreshold <= 0.0 || config.loadCongestionThreshold > 1.0,
                    "loadCongestionThreshold must satisfy 0 < x <= 1");
}

} // namespace ns3

#endif // LEO_NTN_HANDOVER_CONFIG_H
