# Simulation Results Staging

This directory is the default landing area for generated outputs from
`scratch/leo-ntn-handover-baseline.cc`.

As constellation size grows, expect trace volume here to increase; keep routine
large-scale runs in this directory and only promote selected baseline outputs.

Current default files include:
- `hex_grid_cells.csv`
- `hex_grid_cells.svg`
- `ue_layout.csv`
- `sat_beam_trace.csv`
- `sat_anchor_trace.csv`
- `sat_beam_report.csv`
- `handover_dl_throughput_trace.csv`
- `handover_event_trace.csv`

This directory is ignored by Git on purpose. Keep routine runs here, and only promote selected baseline results into version control when they are worth preserving.
