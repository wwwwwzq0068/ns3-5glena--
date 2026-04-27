# Research Context

## Thesis Scope
- Topic: LEO constellation handover strategy design and simulation in NS-3 for 5G-NTN.
- Current implementation path: ns-3.46 plus 5G-LENA/NR, with LEO mobility and beam-aware handover logic.
- Main scenarios:
  - inter-beam handover within the same satellite
  - inter-satellite handover across moving LEO nodes

## Core Problem
- LEO satellites move quickly relative to ground users.
- Visibility windows are short.
- Beam coverage moves rapidly.
- Plain terrestrial handover logic causes frequent handovers, ping-pong risk, and unnecessary signaling.
- Signal-only decisions can ignore load imbalance across beams or satellites.

## Research Goals
- Current close-out priorities for the thesis are:
  - lower `E2E delay`
  - lower `packet loss rate`
  - higher throughput
  - fewer completed handovers when they are unnecessary
  - fewer unnecessary handovers and lower `ping-pong`
  - better `load balance` by Jain fairness
- Improve handover success rate.
- Reduce unnecessary handovers and ping-pong behavior.
- Maintain throughput continuity during serving satellite changes.
- Incorporate load-aware target selection rather than leaving it as an optional extension, because the thesis task explicitly requires considering both signal quality and satellite load.
- `SINR`, `PHY DL TB error / TBler`, detailed antenna tuning, handover delay, and signaling overhead are now diagnostics only; they are not part of the formal thesis result table.

## Technical Directions
- Model LEO mobility and time-varying NTN channel behavior.
- Use the current `2x4` dual-plane local scenario as the fixed near-term baseline, with `B00` as the current control group for baseline / improved comparison.
- Implement a clean traditional A3-style baseline in NS-3 C++.
- Use signal quality metrics such as RSRP and RSRQ.
- Design a custom strategy that combines signal quality with load or cost-function based decision rules.
- Compare the improved strategy against the baseline handover method in the same scenario.

## Default Evaluation Metrics
- `E2E delay`
- `packet loss rate`
- throughput
- completed handovers
- ping-pong count
- load balance by Jain fairness

## Default Files To Inspect
- `scratch/leo-ntn-handover-baseline.cc`
- `scratch/handover/beam-link-budget.h`
- `scratch/handover/leo-orbit-calculator.h`
- `scratch/handover/leo-ntn-handover-utils.h`
- `scratch/handover/wgs84-hex-grid.h`

## Response Style For This Project
- Explain simulation logs in research terms.
- Tie observed behavior back to trigger logic and metrics.
- Distinguish clearly between implemented behavior and intended behavior.
- Call out when a conclusion is inferred from code rather than explicitly stated.

## Code Organization Preference
- When feasible, move reusable logic and common definitions out of the main simulation file into
  helper headers, so `scratch/leo-ntn-handover-baseline.cc` remains focused on scenario assembly and
  experiment flow.
- At the current phase, treat code organization as largely settled and prioritize baseline-vs-improved
  handover strategy work unless a structural change is required by the algorithm implementation.
