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

#include "leo-ntn-handover-utils.h"
#include "ns3/core-module.h"
#include <string>

namespace ns3
{

struct BaselineSimulationConfig
{
    double simTime = 40.0;
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
    double interPlaneRaanSpacingDeg = -2.0;
    double interPlaneTimeOffsetSeconds = 0.0;
    double baseTrueAnomalyDeg = 0.0;
    double gmstAtEpochDeg = 0.0;
    bool autoAlignToUe = true;
    bool descendingPass = false;
    double alignmentReferenceTimeSeconds = 20.0;
    double overpassGapSeconds = 2.0;
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
    double hexCellRadiusKm = 20.0;
    uint32_t gridNearestK = 3;

    std::string outputDir = "scratch/results";
    bool printGridCatalog = true;
    std::string gridCatalogPath = JoinOutputPath(outputDir, "hex_grid_cells.csv");
    std::string beamReportScriptPath =
        std::string(PROJECT_SOURCE_PATH) + "/scratch/sat_beam_report.py";
    std::string plotHexGridScriptPath =
        std::string(PROJECT_SOURCE_PATH) + "/scratch/plot_hex_grid_svg.py";
    std::string beamTracePath = JoinOutputPath(outputDir, "sat_beam_trace.csv");
    std::string beamReportPath = JoinOutputPath(outputDir, "sat_beam_report.csv");
    std::string satAnchorTracePath = JoinOutputPath(outputDir, "sat_anchor_trace.csv");
    std::string ueLayoutPath = JoinOutputPath(outputDir, "ue_layout.csv");
    std::string gridSvgPath = JoinOutputPath(outputDir, "hex_grid_cells.svg");

    double centralFrequency = 2e9;
    double bandwidth = 40e6;
    double lambda = 1000.0;
    uint32_t udpPacketSize = 1000;
    double gnbTxPower = 100.0;
    double ueTxPower = 23.0;
    double beamMaxGainDbi = 38.0;
    double scanMaxDeg = 60.0;
    double theta3dBDeg = 4.0;
    double sideLobeAttenuationDb = 30.0;
    double ueRxGainDbi = 0.0;
    double atmLossDb = 0.5;
    double beamDropPenaltyDb = 200.0;
    bool customA3UseShadowing = true;
    double customA3ShadowingSigmaDb = 1.0;
    double customA3ShadowingCorrelationSeconds = 4.0;
    bool customA3UseRician = true;
    double customA3RicianKDb = 10.0;
    double customA3RicianCorrelationSeconds = 0.5;

    double hoHysteresisDb = 2.0;
    uint32_t hoTttMs = 200;
    double pingPongWindowSeconds = 1.5;
    bool forceRlcAmForEpc = false;
    bool disableUeIpv4Forwarding = true;
    bool strictNrtGuard = false;
    double strictNrtMarginDb = -1.0;
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
    bool runBeamReportScript = true;
    bool runGridSvgScript = true;
    double throughputReportIntervalSeconds = 0.0;
    double maxSupportedUesPerSatellite = 3.0;
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
    addArg("interPlaneTimeOffsetSeconds", config.interPlaneTimeOffsetSeconds);
    addArg("baseTrueAnomalyDeg", config.baseTrueAnomalyDeg);
    addArg("gmstAtEpochDeg", config.gmstAtEpochDeg);
    addArg("autoAlignToUe", config.autoAlignToUe);
    addArg("descendingPass", config.descendingPass);
    addArg("alignmentReferenceTimeSeconds", config.alignmentReferenceTimeSeconds);
    addArg("overpassGapSeconds", config.overpassGapSeconds);
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
    addArg("hexCellRadiusKm", config.hexCellRadiusKm);
    addArg("gridNearestK", config.gridNearestK);
    addArg("outputDir", config.outputDir);
    addArg("printGridCatalog", config.printGridCatalog);
    addArg("gridCatalogPath", config.gridCatalogPath);
    addArg("beamMaxGainDbi", config.beamMaxGainDbi);
    addArg("scanMaxDeg", config.scanMaxDeg);
    addArg("theta3dBDeg", config.theta3dBDeg);
    addArg("sideLobeAttenuationDb", config.sideLobeAttenuationDb);
    addArg("ueRxGainDbi", config.ueRxGainDbi);
    addArg("atmLossDb", config.atmLossDb);
    addArg("beamDropPenaltyDb", config.beamDropPenaltyDb);
    addArg("customA3UseShadowing", config.customA3UseShadowing);
    addArg("customA3ShadowingSigmaDb", config.customA3ShadowingSigmaDb);
    addArg("customA3ShadowingCorrelationSeconds", config.customA3ShadowingCorrelationSeconds);
    addArg("customA3UseRician", config.customA3UseRician);
    addArg("customA3RicianKDb", config.customA3RicianKDb);
    addArg("customA3RicianCorrelationSeconds", config.customA3RicianCorrelationSeconds);
    addArg("hoHysteresisDb", config.hoHysteresisDb);
    addArg("hoTttMs", config.hoTttMs);
    addArg("pingPongWindowSeconds", config.pingPongWindowSeconds);
    addArg("forceRlcAmForEpc", config.forceRlcAmForEpc);
    addArg("disableUeIpv4Forwarding", config.disableUeIpv4Forwarding);
    addArg("strictNrtGuard", config.strictNrtGuard);
    addArg("strictNrtMarginDb", config.strictNrtMarginDb);
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
    addArg("runBeamReportScript", config.runBeamReportScript);
    addArg("beamReportScriptPath", config.beamReportScriptPath);
    addArg("runGridSvgScript", config.runGridSvgScript);
    addArg("plotHexGridScriptPath", config.plotHexGridScriptPath);
    addArg("beamTracePath", config.beamTracePath);
    addArg("beamReportPath", config.beamReportPath);
    addArg("satAnchorTracePath", config.satAnchorTracePath);
    addArg("ueLayoutPath", config.ueLayoutPath);
    addArg("gridSvgPath", config.gridSvgPath);
    addArg("throughputReportIntervalSeconds", config.throughputReportIntervalSeconds);
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
    const std::string defaultBeamTracePath =
        JoinOutputPath(defaultOutputDir, "sat_beam_trace.csv");
    const std::string defaultBeamReportPath =
        JoinOutputPath(defaultOutputDir, "sat_beam_report.csv");
    const std::string defaultSatAnchorTracePath =
        JoinOutputPath(defaultOutputDir, "sat_anchor_trace.csv");
    const std::string defaultUeLayoutPath = JoinOutputPath(defaultOutputDir, "ue_layout.csv");
    const std::string defaultGridSvgPath = JoinOutputPath(defaultOutputDir, "hex_grid_cells.svg");

    if (config.gridCatalogPath == defaultGridCatalogPath && config.outputDir != defaultOutputDir)
    {
        config.gridCatalogPath = JoinOutputPath(config.outputDir, "hex_grid_cells.csv");
    }
    if (config.beamTracePath == defaultBeamTracePath && config.outputDir != defaultOutputDir)
    {
        config.beamTracePath = JoinOutputPath(config.outputDir, "sat_beam_trace.csv");
    }
    if (config.beamReportPath == defaultBeamReportPath && config.outputDir != defaultOutputDir)
    {
        config.beamReportPath = JoinOutputPath(config.outputDir, "sat_beam_report.csv");
    }
    if (config.satAnchorTracePath == defaultSatAnchorTracePath && config.outputDir != defaultOutputDir)
    {
        config.satAnchorTracePath = JoinOutputPath(config.outputDir, "sat_anchor_trace.csv");
    }
    if (config.ueLayoutPath == defaultUeLayoutPath && config.outputDir != defaultOutputDir)
    {
        config.ueLayoutPath = JoinOutputPath(config.outputDir, "ue_layout.csv");
    }
    if (config.gridSvgPath == defaultGridSvgPath && config.outputDir != defaultOutputDir)
    {
        config.gridSvgPath = JoinOutputPath(config.outputDir, "hex_grid_cells.svg");
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
    NS_ABORT_MSG_IF(config.ueLayoutType != "line" && config.ueLayoutType != "seven-cell",
                    "ueLayoutType must be either 'line' or 'seven-cell'");
    NS_ABORT_MSG_IF(config.ueSpacingMeters <= 0.0, "ueSpacingMeters must be > 0");
    NS_ABORT_MSG_IF(config.ueCenterSpacingMeters <= 0.0, "ueCenterSpacingMeters must be > 0");
    NS_ABORT_MSG_IF(config.ueRingPointOffsetMeters <= 0.0,
                    "ueRingPointOffsetMeters must be > 0");
    NS_ABORT_MSG_IF(config.ueLayoutType == "seven-cell" && config.ueNum != 25,
                    "seven-cell layout currently requires ueNum == 25");
    NS_ABORT_MSG_IF(config.orbitPlaneCount == 0, "orbitPlaneCount must be >= 1");
    NS_ABORT_MSG_IF(config.gNbNum < config.orbitPlaneCount, "gNbNum must be >= orbitPlaneCount");
    NS_ABORT_MSG_IF(config.orbitEccentricity < 0.0 || config.orbitEccentricity >= 1.0,
                    "orbitEccentricity must satisfy 0 <= e < 1");
    NS_ABORT_MSG_IF(std::abs(config.interPlaneRaanSpacingDeg) >= 180.0,
                    "interPlaneRaanSpacingDeg must satisfy |value| < 180");
    NS_ABORT_MSG_IF(config.interPlaneTimeOffsetSeconds < 0.0,
                    "interPlaneTimeOffsetSeconds must be >= 0");
    NS_ABORT_MSG_IF(config.scanMaxDeg <= 0.0 || config.scanMaxDeg >= 90.0,
                    "scanMaxDeg must satisfy 0 < scanMaxDeg < 90");
    NS_ABORT_MSG_IF(config.theta3dBDeg <= 0.0, "theta3dBDeg must be > 0");
    NS_ABORT_MSG_IF(config.customA3ShadowingSigmaDb < 0.0, "customA3ShadowingSigmaDb must be >= 0");
    NS_ABORT_MSG_IF(config.customA3ShadowingCorrelationSeconds <= 0.0,
                    "customA3ShadowingCorrelationSeconds must be > 0");
    NS_ABORT_MSG_IF(config.customA3RicianKDb < 0.0, "customA3RicianKDb must be >= 0");
    NS_ABORT_MSG_IF(config.customA3RicianCorrelationSeconds <= 0.0,
                    "customA3RicianCorrelationSeconds must be > 0");
    NS_ABORT_MSG_IF(config.kpiIntervalSeconds <= 0.0, "kpiIntervalSeconds must be > 0");
    NS_ABORT_MSG_IF(config.gridWidthKm <= 0.0, "gridWidthKm must be > 0");
    NS_ABORT_MSG_IF(config.gridHeightKm <= 0.0, "gridHeightKm must be > 0");
    NS_ABORT_MSG_IF(config.hexCellRadiusKm <= 0.0, "hexCellRadiusKm must be > 0");
    NS_ABORT_MSG_IF(config.gridNearestK == 0, "gridNearestK must be >= 1");
    NS_ABORT_MSG_IF(config.outputDir.empty(), "outputDir must not be empty");
    NS_ABORT_MSG_IF(config.progressReportIntervalSeconds <= 0.0,
                    "progressReportIntervalSeconds must be > 0");
    NS_ABORT_MSG_IF(config.pingPongWindowSeconds <= 0.0,
                    "pingPongWindowSeconds must be > 0");
    NS_ABORT_MSG_IF(config.maxSupportedUesPerSatellite <= 0.0,
                    "maxSupportedUesPerSatellite must be > 0");
    NS_ABORT_MSG_IF(config.loadCongestionThreshold <= 0.0 || config.loadCongestionThreshold > 1.0,
                    "loadCongestionThreshold must satisfy 0 < x <= 1");

    if (config.strictNrtMarginDb < 0.0)
    {
        config.strictNrtMarginDb = config.hoHysteresisDb;
    }

    NS_ABORT_MSG_IF(config.strictNrtMarginDb < 0.0, "strictNrtMarginDb must be >= 0");
}

} // namespace ns3

#endif // LEO_NTN_HANDOVER_CONFIG_H
