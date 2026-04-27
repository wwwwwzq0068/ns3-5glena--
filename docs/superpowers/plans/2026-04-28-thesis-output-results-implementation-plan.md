# Thesis Output Results Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the formal thesis output path for baseline-vs-improved comparison without relying on PHY/SINR artifacts.

**Architecture:** Add one C++ satellite state trace that records per-satellite load state at each constellation update, then add one Python summary script that reads formal run directories and emits per-run, per-mode, and baseline-vs-improved KPI summaries. Keep trajectory/HTML artifacts for scenario figures, keep PHY code only as hidden diagnostics, and synchronize docs to demote PHY metrics from the formal output.

**Tech Stack:** ns-3.46 C++ scratch simulation, existing header-only C++ helpers, Python 3 standard library plus optional matplotlib for plots, existing focused `./ns3 run` tests.

---

### Task 1: Add Satellite State Trace Configuration

**Files:**
- Modify: `scratch/handover/leo-ntn-handover-output-lifecycle.h`
- Modify: `scratch/handover/leo-ntn-handover-config.h`
- Modify: `scratch/handover/leo-ntn-handover-output.h`
- Test: `scratch/test-baseline-config-helpers.cc`

- [ ] **Step 1: Write the failing config helper test**

Add these checks to the first block in `scratch/test-baseline-config-helpers.cc`, after the existing `satGroundTrackTracePath` assertion:

```cpp
        Require(config.satelliteStateTracePath ==
                    JoinOutputPath(config.outputDir, "satellite_state_trace.csv"),
                "satellite state trace path should follow a non-default outputDir");
```

Add this check to the default-output block near the existing `enableSatGroundTrackTrace` assertion:

```cpp
        Require(config.enableSatelliteStateTrace,
                "satellite state trace should be enabled by default for formal load-balance output");
```

Add this block after the custom override checks for `phyDlTbTracePath`:

```cpp
        config.satelliteStateTracePath = "/tmp/manual-satellite-state.csv";
        ResolveBaselineOutputPaths(config);
        Require(config.satelliteStateTracePath == "/tmp/manual-satellite-state.csv",
                "explicit satellite state trace path should not be rewritten");
```

- [ ] **Step 2: Run the focused test and verify it fails**

Run:

```bash
./ns3 run test-baseline-config-helpers
```

Expected: compilation fails because `BaselineSimulationConfig` has no `satelliteStateTracePath` or `enableSatelliteStateTrace` member.

- [ ] **Step 3: Add the CSV header and config fields**

In `scratch/handover/leo-ntn-handover-output-lifecycle.h`, add:

```cpp
inline constexpr std::string_view kSatelliteStateTrace =
    "time_s,sat,plane,slot,cell,anchor_grid_id,attached_ue_count,"
    "offered_packet_rate,load_score,admission_allowed\n";
```

In `scratch/handover/leo-ntn-handover-config.h`, add fields next to the satellite trace fields:

```cpp
    std::string satelliteStateTracePath = JoinOutputPath(outputDir, "satellite_state_trace.csv");
    bool enableSatelliteStateTrace = true;
```

Register command-line options next to the existing satellite trace options:

```cpp
    addArg("satelliteStateTracePath", config.satelliteStateTracePath);
    addArg("enableSatelliteStateTrace", config.enableSatelliteStateTrace);
```

Update `ResolveBaselineOutputPaths`:

```cpp
    const std::string defaultSatelliteStateTracePath =
        JoinOutputPath(defaultOutputDir, "satellite_state_trace.csv");
```

and:

```cpp
    if (config.satelliteStateTracePath == defaultSatelliteStateTracePath &&
        config.outputDir != defaultOutputDir)
    {
        config.satelliteStateTracePath =
            JoinOutputPath(config.outputDir, "satellite_state_trace.csv");
    }
```

- [ ] **Step 4: Add the stream lifecycle**

In `scratch/handover/leo-ntn-handover-output.h`, add a stream pointer to `BaselineTraceOutputSet`:

```cpp
    std::ofstream* satelliteStateTrace = nullptr;
```

Update null checks in `InitializeBaselineTraceOutputs` and `CloseBaselineTraceOutputs` to include it. Add initialization after the ground-track block:

```cpp
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
```

Include the stream in `CloseOutputStreams`.

- [ ] **Step 5: Run the focused test and verify it passes**

Run:

```bash
./ns3 run test-baseline-config-helpers
```

Expected: `[TEST-PASS] baseline config helper behavior is correct`.

### Task 2: Emit Satellite State Trace Rows

**Files:**
- Modify: `scratch/leo-ntn-handover-baseline.cc`
- Test: `scratch/test-baseline-config-helpers.cc`

- [ ] **Step 1: Add the trace stream and output directory wiring**

In `scratch/leo-ntn-handover-baseline.cc`, add the global stream near the other trace streams:

```cpp
// 逐时刻卫星负载状态导出文件，用于论文负载均衡统计。
static std::ofstream g_satelliteStateTrace;
```

In output directory creation, add:

```cpp
        (!cfg.enableSatelliteStateTrace ||
         EnsureParentDirectoryForFile(cfg.satelliteStateTracePath)) &&
```

When constructing `BaselineTraceOutputSet`, pass `&g_satelliteStateTrace` in the new slot.

- [ ] **Step 2: Implement row emission**

Add this helper near `FlushSatelliteAnchorTraceRows`:

```cpp
static void
FlushSatelliteStateTraceRows(double nowSeconds)
{
    if (!g_satelliteStateTrace.is_open())
    {
        return;
    }

    for (uint32_t satIdx = 0; satIdx < g_satellites.size(); ++satIdx)
    {
        const auto& sat = g_satellites[satIdx];
        const uint16_t cellId = sat.dev ? sat.dev->GetCellId() : 0;
        g_satelliteStateTrace << std::fixed << std::setprecision(3)
                              << nowSeconds << "," << satIdx << ","
                              << sat.orbitPlaneIndex << "," << sat.orbitSlotIndex << ","
                              << cellId << "," << sat.currentAnchorGridId << ","
                              << sat.attachedUeCount << "," << sat.offeredPacketRate << ","
                              << sat.loadScore << "," << (sat.admissionAllowed ? 1 : 0)
                              << "\n";
    }
}
```

Call it in `UpdateConstellation` immediately after `UpdateSatelliteLoadStats(...)` so the row reflects the current UE attachment counts and load score:

```cpp
    FlushSatelliteStateTraceRows(nowSeconds);
```

- [ ] **Step 3: Run the focused config test**

Run:

```bash
./ns3 run test-baseline-config-helpers
```

Expected: `[TEST-PASS] baseline config helper behavior is correct`.

- [ ] **Step 4: Run a short smoke simulation with satellite state trace**

Run:

```bash
./ns3 run "leo-ntn-handover-baseline --simTime=3 --outputDir=scratch/results/manual/thesis-output-smoke --enableFlowMonitor=true --enableSatAnchorTrace=true --enableSatGroundTrackTrace=true --printSimulationProgress=false --startupVerbose=false"
```

Expected: simulation exits successfully and creates `scratch/results/manual/thesis-output-smoke/satellite_state_trace.csv`.

- [ ] **Step 5: Inspect the satellite state trace header**

Run:

```bash
head -n 3 scratch/results/manual/thesis-output-smoke/satellite_state_trace.csv
```

Expected first line:

```text
time_s,sat,plane,slot,cell,anchor_grid_id,attached_ue_count,offered_packet_rate,load_score,admission_allowed
```

### Task 3: Add Formal Summary Script

**Files:**
- Create: `scratch/plotting/summarize_thesis_results.py`
- Test: local temporary fixture under `/tmp`

- [ ] **Step 1: Write the summary script with strict CSV parsing**

Create `scratch/plotting/summarize_thesis_results.py` with these responsibilities:

- Accept `--results-root scratch/results/formal` and `--output-dir scratch/results/formal/summary`.
- Discover run directories under `baseline/seed-*` and `improved/seed-*`.
- Read `e2e_flow_metrics.csv`, using the `TOTAL` row for delay, loss, and throughput.
- Read `handover_event_trace.csv`, counting `HO_END_OK` rows and rows with `ping_pong_detected == 1`.
- Read `satellite_state_trace.csv`, computing run-level mean Jain fairness over non-empty time points.
- Write `run_summary.csv`, `paper_kpi_summary.csv`, and `paper_kpi_comparison.csv`.
- If matplotlib is available, write `paper_kpi_comparison.png` and `load_balance_jain_timeseries.png`; otherwise write CSVs only and print a warning.

The script must not read or require any PHY/SINR/TBler file.

- [ ] **Step 2: Create a minimal fixture outside the repo**

Run:

```bash
mkdir -p /tmp/thesis-output-fixture/baseline/seed-01 /tmp/thesis-output-fixture/improved/seed-01 /tmp/thesis-output-fixture/summary
```

Create these fixture files using `printf` or a short shell-safe editor:

`/tmp/thesis-output-fixture/baseline/seed-01/e2e_flow_metrics.csv`

```text
ue,dl_port,matched_flow,tx_packets,rx_packets,lost_packets,loss_rate_percent,tx_bytes,rx_bytes,offered_mbps,throughput_mbps,mean_delay_ms,mean_jitter_ms
TOTAL,,,100,80,20,20.000,100000,80000,1.000,0.800,50.000,1.000
```

`/tmp/thesis-output-fixture/improved/seed-01/e2e_flow_metrics.csv`

```text
ue,dl_port,matched_flow,tx_packets,rx_packets,lost_packets,loss_rate_percent,tx_bytes,rx_bytes,offered_mbps,throughput_mbps,mean_delay_ms,mean_jitter_ms
TOTAL,,,100,90,10,10.000,100000,90000,1.000,0.900,40.000,1.000
```

`handover_event_trace.csv` for baseline:

```text
time_s,ue,ho_id,event,source_cell,target_cell,source_sat,target_sat,delay_ms,ping_pong_detected,failure_reason
1.000,0,1,HO_END_OK,1,2,0,1,20.000,1,
2.000,1,2,HO_END_OK,2,3,1,2,20.000,0,
```

`handover_event_trace.csv` for improved:

```text
time_s,ue,ho_id,event,source_cell,target_cell,source_sat,target_sat,delay_ms,ping_pong_detected,failure_reason
1.000,0,1,HO_END_OK,1,2,0,1,20.000,0,
```

`satellite_state_trace.csv` for baseline:

```text
time_s,sat,plane,slot,cell,anchor_grid_id,attached_ue_count,offered_packet_rate,load_score,admission_allowed
1.000,0,0,0,1,10,4,1000.000,0.700,1
1.000,1,0,1,2,11,0,0.000,0.000,1
```

`satellite_state_trace.csv` for improved:

```text
time_s,sat,plane,slot,cell,anchor_grid_id,attached_ue_count,offered_packet_rate,load_score,admission_allowed
1.000,0,0,0,1,10,2,500.000,0.400,1
1.000,1,0,1,2,11,2,500.000,0.400,1
```

- [ ] **Step 3: Run the script on the fixture**

Run:

```bash
python3 scratch/plotting/summarize_thesis_results.py --results-root /tmp/thesis-output-fixture --output-dir /tmp/thesis-output-fixture/summary
```

Expected CSV facts:

- `run_summary.csv` has two data rows.
- Baseline `completed_ho` is `2`.
- Baseline `ping_pong_count` is `1`.
- Baseline `load_balance_jain` is `0.500000`.
- Improved `load_balance_jain` is `1.000000`.
- `paper_kpi_comparison.csv` contains no SINR, PHY, TBler, handover delay, or signaling row.

- [ ] **Step 4: Run the script against the smoke output if available**

Run:

```bash
mkdir -p scratch/results/formal/baseline/seed-01 scratch/results/formal/improved/seed-01 scratch/results/formal/summary
```

Copy the smoke output into both run directories only if it contains the required formal files, then run:

```bash
python3 scratch/plotting/summarize_thesis_results.py --results-root scratch/results/formal --output-dir scratch/results/formal/summary
```

Expected: script either writes summaries or clearly reports which required files are missing.

### Task 4: Remove Old Formal Output Dependencies

**Files:**
- Modify: `scratch/results/README.md`
- Modify: `scratch/README.md`
- Modify: `scratch/baseline-definition.md`
- Modify: `docs/research-context.md`
- Modify: `docs/current-task-memory.md`
- Modify only if necessary: `scratch/midterm-report/README.md`

- [ ] **Step 1: Update result retention docs**

In `scratch/results/README.md`, replace the retained-output focus list with:

```markdown
- `E2E delay`
- `packet loss rate`
- `throughput`
- `completed handovers`
- `ping-pong`
- `load balance` via Jain fairness
```

Remove PHY/SINR/TBler from the default retention list. Keep trajectory/HTML artifacts as formal scenario figure materials.

- [ ] **Step 2: Update scratch overview docs**

In `scratch/README.md`, state that formal outputs keep `hex_grid_cells.html`, `sat_ground_track.csv`, `sat_anchor_trace.csv`, and `satellite_state_trace.csv`, while PHY traces are diagnostics only.

In `scratch/baseline-definition.md`, update the final KPI wording to remove SINR from the formal main table and mention Jain fairness for load balance.

In `scratch/midterm-report/README.md`, only add a short sentence if needed that active formal output wording lives in the updated docs.

- [ ] **Step 3: Update research memory docs**

In `docs/research-context.md` and `docs/current-task-memory.md`, demote SINR/TBler/PHY outputs to diagnostic support and make formal close-out metrics:

```text
E2E delay, packet loss rate, throughput, completed handovers, ping-pong, Jain load fairness
```

Do not change the baseline scenario definition.

- [ ] **Step 4: Search for stale formal-output wording**

Run:

```bash
rg -n "mean SINR|SINR|TBler|PHY DL|handover delay|signaling overhead" docs scratch/*.md scratch/results/README.md
```

Expected: remaining matches describe diagnostics or excluded metrics, not formal thesis main-table outputs.

### Task 5: Final Verification

**Files:**
- Verify all modified code and docs

- [ ] **Step 1: Run focused C++ tests**

Run:

```bash
./ns3 run test-baseline-config-helpers
./ns3 run test-baseline-defaults
./ns3 run test-earth-fixed-beam-target
```

Expected: all tests exit successfully.

- [ ] **Step 2: Run the smoke simulation**

Run:

```bash
./ns3 run "leo-ntn-handover-baseline --simTime=3 --outputDir=scratch/results/manual/thesis-output-smoke --enableFlowMonitor=true --enableSatAnchorTrace=true --enableSatGroundTrackTrace=true --printSimulationProgress=false --startupVerbose=false"
```

Expected: simulation exits successfully and writes the formal output files, including `satellite_state_trace.csv`.

- [ ] **Step 3: Run summary script on fixture**

Run:

```bash
python3 scratch/plotting/summarize_thesis_results.py --results-root /tmp/thesis-output-fixture --output-dir /tmp/thesis-output-fixture/summary
```

Expected: CSV summary files are written and contain no PHY/SINR/TBler metrics.

- [ ] **Step 4: Inspect git diff**

Run:

```bash
git diff -- scratch/handover/leo-ntn-handover-output-lifecycle.h scratch/handover/leo-ntn-handover-config.h scratch/handover/leo-ntn-handover-output.h scratch/leo-ntn-handover-baseline.cc scratch/test-baseline-config-helpers.cc scratch/plotting/summarize_thesis_results.py scratch/README.md scratch/baseline-definition.md scratch/results/README.md scratch/midterm-report/README.md docs/research-context.md docs/current-task-memory.md docs/superpowers/specs/2026-04-28-thesis-output-results-design.md docs/superpowers/plans/2026-04-28-thesis-output-results-implementation-plan.md
```

Expected: diff only contains the formal output refactor, summary script, and doc synchronization.
