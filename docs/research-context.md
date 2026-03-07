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
- Improve handover success rate.
- Reduce unnecessary handovers and ping-pong behavior.
- Maintain throughput continuity during serving satellite changes.
- Keep handover delay and service interruption low.
- Support load-aware target selection when the model includes load information.

## Technical Directions
- Model LEO mobility and time-varying NTN channel behavior.
- Expand constellation scale so handover observations are not limited to a very small local overpass.
- Implement custom handover logic in NS-3 C++.
- Use signal quality metrics such as RSRP and RSRQ.
- Combine signal quality with load or cost-function based decision rules.
- Compare the custom strategy against a baseline handover method.

## Default Evaluation Metrics
- handover success rate
- handover delay
- throughput
- service continuity
- ping-pong count
- signaling overhead
- load balance, if modeled

## Default Files To Inspect
- `scratch/leo-ntn-handover-baseline.cc`
- `scratch/beam-link-budget.h`
- `scratch/leo-orbit-calculator.h`
- `scratch/leo-ntn-handover-utils.h`
- `scratch/wgs84-hex-grid.h`

## Response Style For This Project
- Explain simulation logs in research terms.
- Tie observed behavior back to trigger logic and metrics.
- Distinguish clearly between implemented behavior and intended behavior.
- Call out when a conclusion is inferred from code rather than explicitly stated.

## Code Organization Preference
- When feasible, move reusable logic and common definitions out of the main simulation file into
  helper headers, so `scratch/leo-ntn-handover-baseline.cc` remains focused on scenario assembly and
  experiment flow.
- At the current phase, treat code organization as largely settled and prioritize scenario scaling work
  unless a structural change is required by the constellation expansion itself.
