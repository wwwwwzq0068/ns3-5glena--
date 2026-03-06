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
- When a task involves simulation behavior or logs, read `scratch/myfirst.cc` first.
- Treat inter-beam and inter-satellite handover as the main scenarios of interest.
- Prefer research-oriented interpretation first, code explanation second.
- When proposing changes, keep evaluation tied to handover success rate, handover delay, throughput, continuity, and signaling overhead.
- When code grows, prefer moving reusable logic, helper functions, and shared definitions into external header files when reasonable, so the main simulation script stays easier to read.
