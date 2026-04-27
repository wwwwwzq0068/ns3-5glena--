# Simulation Results Staging

Keep only results that still serve the thesis mainline.

Current comparison priority:
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

Remove:
- loose top-level CSV / SVG / HTML outputs after ad hoc runs
- carrier-reuse / inter-frequency diagnosis
- beamwidth / sidelobe / plane-offset / overpass-gap scans
- smoke runs, startup checks, and parameter sweep leftovers

This directory is Git-ignored and should stay small.
