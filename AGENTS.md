# Project Memory

## Stable Context
- Primary stack: ns-3.46 with 5G-LENA/NR in a 5G-NTN research setting.
- Primary research topic: handover optimization for LEO satellite networks.
- Thesis title: Design and Simulation of 5G-NTN Handover Strategy for LEO Constellations Based on NS-3.

## Default Focus
- Prioritize handover logic over generic connectivity bring-up.
- Default analysis targets:
  - trigger condition and target selection
  - TTT and hysteresis behavior
  - neighbor management and gating
  - RSRP/RSRQ evolution
  - throughput continuity during handover
  - ping-pong suppression
  - load-aware decision logic when present

## Working Defaults
- When a task involves simulation behavior or logs, read `scratch/leo-ntn-handover-baseline.cc` first.
- Treat inter-beam and inter-satellite handover as the main scenarios of interest.
- Prefer research-oriented interpretation first, code explanation second.
- When proposing changes, keep evaluation tied to handover success rate, handover delay, throughput, continuity, and signaling overhead.
- When code grows, prefer moving reusable logic, helper functions, and shared definitions into external header files when reasonable, so the main simulation script stays easier to read.
- Current implementation phase: baseline file reorganization and handover log cleanup are considered complete; prioritize constellation scaling and scenario representativeness next.

## Required Preflight
- For every new task in this repository, read these files before making assumptions or editing code:
  - `docs/research-context.md`
  - `docs/current-task-memory.md`
  - `docs/research-workflow.md`
  - `scratch/README.md`
  - `scratch/baseline-definition.md`
  - `scratch/results/README.md`
  - `scratch/midterm-report/README.md`
- After reading them, the first user-facing progress update must briefly restate:
  - which document set was checked
  - the current baseline/scenario understanding
  - the immediate next step
- Do not change scenario definition, default parameters, baseline wording, or midterm-report framing until these documents have been checked against the requested task.
- If a request conflicts with the current documented research direction, call out the conflict explicitly before editing.
- Any task that modifies important files under `scratch/` must end with a doc-sync check against at least:
  - `scratch/README.md`
  - `scratch/baseline-definition.md`
  - `scratch/midterm-report/README.md`

## Collaboration Preferences
- Prefer editing existing files over creating new documents or parallel explanations.
- When a file becomes long or repetitive, prefer a small local replacement over appending new layers, but do not broaden the edit into unrelated cleanup.
- Keep documentation short and decision-oriented; avoid stacking background information unless necessary.
- Keep code changes minimal and focused; prefer the simplest design that preserves future extensibility.
- When possible, make one small, clear change at a time instead of expanding scope.
- When modifying code, avoid both patch-stacking and aggressive pruning: add only what the task needs, and delete old logic only when it is directly related to the task and clearly low-value for the current research path.
- When explaining simulation logic or code behavior, prefer plain-language explanations before formal terminology.
- Start with the core idea first, then map it to parameter names and code details.
- Use small numeric examples when they make the behavior easier to understand.
- Avoid overly abstract explanations when a concrete description would be clearer.
