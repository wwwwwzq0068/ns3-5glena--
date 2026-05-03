# Simulation Results Retention

`scratch/results/` is a Git-ignored staging area. Keep only outputs that still support the thesis mainline comparison on:
- `E2E delay`
- `packet loss rate`
- `throughput`
- `completed handovers`
- `ping-pong`
- `load balance` via Jain fairness

Formal result runs should retain the per-run KPI traces, `satellite_state_trace.csv`, and the HTML/trajectory files needed for thesis scenario figures. PHY/SINR/TBler files are diagnostic only and are not retained as formal thesis outputs by default.

Formal thesis version: `research-v6.1`.

The only formal thesis scenario is now `2x4 + poisson-3ring + overlap-only + beam-only`, compared as baseline vs full improved.

From the repo root, generate the formal 40s paper-data runs and summary with:

```bash
bash -lc 'set -euo pipefail; ROOT="scratch/results/formal/v6.1-poisson3ring-overlap-beamonly-40s"; for mode in baseline improved; do for seed in 1 2 3; do label=$(printf "%02d" "$seed"); ./ns3 run "leo-ntn-handover-baseline --handoverMode=${mode} --gNbNum=8 --orbitPlaneCount=2 --ueNum=30 --ueLayoutType=poisson-3ring --beamExclusionMode=overlap-only --realLinkGateMode=beam-only --simTime=40 --ueLayoutRandomSeed=${seed} --outputDir=${ROOT}/${mode}/seed-${label} --enableFlowMonitor=true --enableSatAnchorTrace=true --enableSatGroundTrackTrace=true --enableSatelliteStateTrace=true --enablePhyDlTbStats=false --runGridHtmlScript=true --startupVerbose=false --printSimulationProgress=true"; done; done; python3 scratch/plotting/summarize_thesis_results.py --results-root "${ROOT}" --output-dir "${ROOT}/summary"'
```

Retain by default:
- `formal/v6.1-poisson3ring-overlap-beamonly-40s/baseline/seed-*`
- `formal/v6.1-poisson3ring-overlap-beamonly-40s/improved/seed-*`
- `formal/v6.1-poisson3ring-overlap-beamonly-40s/summary/`

Remove by default:
- loose top-level `CSV` / `SVG` / `HTML` outputs from ad hoc runs
- carrier-reuse, inter-frequency, beamwidth, sidelobe, plane-offset, and overpass-gap diagnostics
- smoke runs, startup checks, and parameter sweep leftovers
- historical fixed-UE or gate-sweep result folders unless they are being inspected as background only

If a result must survive cleanup, record its purpose, key parameters, and conclusion in `docs/current-task-memory.md` or another active research note.
