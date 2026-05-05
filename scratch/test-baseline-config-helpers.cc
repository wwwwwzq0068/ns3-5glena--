#include "handover/leo-ntn-handover-config.h"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace ns3;

namespace
{

void
Require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "[TEST-FAIL] " << message << std::endl;
        std::exit(1);
    }
}

} // namespace

int
main()
{
    {
        BaselineSimulationConfig config;
        const std::string plotScriptPath = config.plotHexGridScriptPath;

        config.outputDir = "scratch/results/unit/config-paths";
        ResolveBaselineOutputPaths(config);

        Require(config.gridCatalogPath == JoinOutputPath(config.outputDir, "hex_grid_cells.csv"),
                "grid catalog path should follow a non-default outputDir");
        Require(config.satAnchorTracePath ==
                    JoinOutputPath(config.outputDir, "sat_anchor_trace.csv"),
                "sat anchor trace path should follow a non-default outputDir");
        Require(config.satGroundTrackTracePath ==
                    JoinOutputPath(config.outputDir, "sat_ground_track.csv"),
                "sat ground-track trace path should follow a non-default outputDir");
        Require(config.satelliteStateTracePath ==
                    JoinOutputPath(config.outputDir, "satellite_state_trace.csv"),
                "satellite state trace path should follow a non-default outputDir");
        Require(config.handoverEventTracePath ==
                    JoinOutputPath(config.outputDir, "handover_event_trace.csv"),
                "handover event path should follow a non-default outputDir");
        Require(config.e2eFlowMetricsPath ==
                    JoinOutputPath(config.outputDir, "e2e_flow_metrics.csv"),
                "e2e flow metrics path should follow a non-default outputDir");
        Require(config.gridHtmlPath == JoinOutputPath(config.outputDir, "hex_grid_cells.html"),
                "grid HTML path should follow a non-default outputDir");
        Require(config.plotHexGridScriptPath == plotScriptPath,
                "plot script path should stay anchored to the project source tree");
    }

    {
        BaselineSimulationConfig config;
        config.outputDir = "scratch/results/unit/custom-overrides";
        config.gridCatalogPath = "scratch/results/manual-grid.csv";
        config.gridHtmlPath = "/tmp/manual-grid.html";
        config.satelliteStateTracePath = "/tmp/manual-satellite-state.csv";

        ResolveBaselineOutputPaths(config);

        Require(config.gridCatalogPath == "scratch/results/manual-grid.csv",
                "explicit grid catalog path should not be rewritten");
        Require(config.gridHtmlPath == "/tmp/manual-grid.html",
                "explicit grid HTML path should not be rewritten");
        Require(config.satelliteStateTracePath == "/tmp/manual-satellite-state.csv",
                "explicit satellite state trace path should not be rewritten");
        Require(config.handoverThroughputTracePath ==
                    JoinOutputPath(config.outputDir, "handover_dl_throughput_trace.csv"),
                "default handover throughput path should still follow outputDir");
    }

    {
        BaselineSimulationConfig config;
        config.ueLatitudeDeg = 12.34;
        config.ueLongitudeDeg = 56.78;
        config.ueAltitudeMeters = 90.0;
        config.cellLatitudeDeg = -1.0;
        config.cellLongitudeDeg = -2.0;
        config.cellAltitudeMeters = -3.0;
        config.gridCenterLatitudeDeg = -4.0;
        config.gridCenterLongitudeDeg = -5.0;

        ApplyBaselineDerivedLocationConfig(config);

        Require(config.cellLatitudeDeg == config.ueLatitudeDeg,
                "cell latitude should lock to the UE latitude by default");
        Require(config.cellLongitudeDeg == config.ueLongitudeDeg,
                "cell longitude should lock to the UE longitude by default");
        Require(config.cellAltitudeMeters == config.ueAltitudeMeters,
                "cell altitude should lock to the UE altitude by default");
        Require(config.gridCenterLatitudeDeg == config.ueLatitudeDeg,
                "grid center latitude should lock to the UE latitude by default");
        Require(config.gridCenterLongitudeDeg == config.ueLongitudeDeg,
                "grid center longitude should lock to the UE longitude by default");
    }

    {
        BaselineSimulationConfig config;
        config.lockCellAnchorToUe = false;
        config.lockGridCenterToUe = false;
        config.ueLatitudeDeg = 12.34;
        config.ueLongitudeDeg = 56.78;
        config.ueAltitudeMeters = 90.0;
        config.cellLatitudeDeg = 1.0;
        config.cellLongitudeDeg = 2.0;
        config.cellAltitudeMeters = 3.0;
        config.gridCenterLatitudeDeg = 4.0;
        config.gridCenterLongitudeDeg = 5.0;

        ApplyBaselineDerivedLocationConfig(config);

        Require(config.cellLatitudeDeg == 1.0,
                "unlocked cell latitude should preserve the configured value");
        Require(config.cellLongitudeDeg == 2.0,
                "unlocked cell longitude should preserve the configured value");
        Require(config.cellAltitudeMeters == 3.0,
                "unlocked cell altitude should preserve the configured value");
        Require(config.gridCenterLatitudeDeg == 4.0,
                "unlocked grid center latitude should preserve the configured value");
        Require(config.gridCenterLongitudeDeg == 5.0,
                "unlocked grid center longitude should preserve the configured value");
    }

    {
        BaselineSimulationConfig config;
        Require(config.beamExclusionCandidateK == 64,
                "final scenario should keep K=64 anchor candidate search");
    }

    {
        BaselineSimulationConfig config;
        Require(config.anchorSelectionMode == "demand-max-ue-near-nadir",
                "final scenario should use demand-aware near-nadir anchor selection");
        Require(config.demandSnapshotMode == "runtime-underserved-ue",
                "final scenario should rebuild demand from runtime underserved UEs");
    }

    {
        BaselineSimulationConfig config;
        Require(!config.enableSatAnchorTrace,
                "satellite anchor trace should be disabled by default for faster turnaround runs");
        Require(!config.enableSatGroundTrackTrace,
                "satellite ground-track trace should be disabled by default for faster turnaround runs");
        Require(config.enableSatelliteStateTrace,
                "satellite state trace should be enabled by default for formal load-balance output");
        Require(!config.enableFlowMonitor,
                "FlowMonitor should be disabled by default for faster turnaround runs");
    }

    std::cout << "[TEST-PASS] baseline config helper behavior is correct" << std::endl;
    return 0;
}
