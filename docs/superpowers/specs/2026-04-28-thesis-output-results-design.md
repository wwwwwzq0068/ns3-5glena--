# Thesis Output Results Design

## Purpose

This design tightens the final thesis output path before changing the simulation workflow.
The final results should support a direct comparison between the traditional A3 baseline and
the full improved handover strategy, with enough raw satellite state data to explain load
balancing and enough trajectory data to draw scenario figures.

## Scope

Formal thesis output compares only:

- `baseline`: standard MeasurementReport-driven A3-style handover, signal-only target choice.
- `improved`: full signal/load/visibility-aware handover strategy with the existing stability
  and gating protections enabled.

`improved-score-only` remains a diagnostic mode but is not part of the formal thesis output
path.

## Metric Boundary

The formal result metrics are:

- E2E delay
- packet loss rate
- downlink throughput
- completed handovers
- ping-pong count
- load balance by Jain fairness index

The formal result metrics intentionally exclude:

- SINR
- minimum SINR
- TBler
- PHY DL TB error rate
- handover delay
- signaling overhead

PHY-layer statistics should remain available only as a hidden diagnostic path. Formal runs and
summary scripts must not require `enablePhyDlTbStats=true`.

This changes the previous research-output emphasis: existing docs that list `SINR` as a core
final metric must be synchronized when implementation starts.

## Formal Directory Layout

Formal results should use:

```text
scratch/results/formal/
  baseline/
    seed-01/
    seed-02/
    seed-03/
  improved/
    seed-01/
    seed-02/
    seed-03/
  summary/
```

The first formal pass should use three seeds per mode. The layout can extend to five seeds
without changing the summary script.

## Per-Run Outputs

Each `seed-*` run directory should keep:

```text
e2e_flow_metrics.csv
handover_event_trace.csv
handover_dl_throughput_trace.csv
satellite_state_trace.csv
hex_grid_cells.csv
ue_layout.csv
sat_ground_track.csv
sat_anchor_trace.csv
hex_grid_cells.html
```

`hex_grid_cells.html` is the preferred scenario inspection artifact. `hex_grid_cells.csv`,
`ue_layout.csv`, `sat_ground_track.csv`, and `sat_anchor_trace.csv` remain because the HTML and
future thesis figures need the source geometry and trajectory data.

## Satellite State Trace

Add a formal satellite state trace:

```text
time_s,sat,plane,slot,cell,anchor_grid_id,
attached_ue_count,offered_packet_rate,load_score,admission_allowed
```

This trace is separate from `sat_anchor_trace.csv`. The anchor trace explains beam/coverage
movement; the satellite state trace explains load distribution and supports Jain fairness.

## Load Balance Metric

For each sampled time point, compute Jain fairness from satellite `attached_ue_count`:

```text
J = (sum(x_i)^2) / (n * sum(x_i^2))
```

where `x_i` is the attached UE count for satellite `i`, and `n` is the number of satellites.
Skip time points where all satellites have zero attached UEs. The run-level load balance metric
is the mean Jain index over valid sampled time points. Higher is better, with `1.0` meaning fully
balanced load.

## Summary Outputs

The formal summary path should generate:

```text
run_summary.csv
paper_kpi_summary.csv
paper_kpi_comparison.csv
paper_kpi_comparison.png
load_balance_jain_timeseries.png
```

`run_summary.csv` contains one row per run:

```text
run_id,mode,seed,e2e_delay_ms,packet_loss_percent,
throughput_mbps,completed_ho,ping_pong_count,load_balance_jain
```

`paper_kpi_summary.csv` contains one row per mode:

```text
mode,runs,
e2e_delay_ms_mean,e2e_delay_ms_std,
packet_loss_percent_mean,packet_loss_percent_std,
throughput_mbps_mean,throughput_mbps_std,
completed_ho_mean,completed_ho_std,
ping_pong_mean,ping_pong_std,
load_balance_jain_mean,load_balance_jain_std
```

`paper_kpi_comparison.csv` compares improved against baseline:

```text
metric,baseline_mean,baseline_std,improved_mean,improved_std,
absolute_delta,relative_delta_percent,better_direction
```

The relative direction should be:

- lower is better: E2E delay, packet loss, ping-pong count
- context dependent: completed handovers
- higher is better: throughput, Jain load fairness

## Implementation Boundary

Implementation should first add the satellite state trace and the summary script. The existing
PHY diagnostic implementation can stay in code, but formal output docs and summary scripts should
not mention PHY files as required artifacts.

Important `scratch/` documentation must be synchronized after implementation:

- `scratch/README.md`
- `scratch/baseline-definition.md`
- `scratch/midterm-report/README.md`

The broader research-memory docs should also be updated to demote `SINR` from final output:

- `docs/research-context.md`
- `docs/current-task-memory.md`
