# Simulation Results Retention

`scratch/results/` is a Git-ignored staging area. Keep only outputs that still support the thesis mainline comparison on:
- `E2E delay`
- `packet loss rate`
- `SINR`
- `ping-pong`
- `load balance`

Retain by default:
- `exp/v4.2/B00-baseline-b00-control/`
- `exp/v4.2/I31-improved-s120-m03/`
- `manual/thesis-default40-baseline/`
- `manual/thesis-default40-baseline-rerun/`
- `manual/thesis-default40-improved-rerun/`
- `manual/formal-r2diag19-40s-b00/`
- `manual/formal-r2diag19-40s-improved-rsrq2/`
- `manual/formal-r2diag19-40s-ofdma/`
- `manual/r2diag40-baseline-no-exclusion-rerun/`

Remove by default:
- loose top-level `CSV` / `SVG` / `HTML` outputs from ad hoc runs
- carrier-reuse, inter-frequency, beamwidth, sidelobe, plane-offset, and overpass-gap diagnostics
- smoke runs, startup checks, and parameter sweep leftovers

If a result must survive cleanup, record its purpose, key parameters, and conclusion in `docs/current-task-memory.md` or another active research note.
