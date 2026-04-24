# R2 UE Diagnostic Layout Design

**Date:** 2026-04-24

**Goal**

Define a small-area UE diagnostic layout that helps inspect beam landing behavior before replacing the current `seven-cell` baseline scenario.

The immediate objective is not to maximize handover count. The immediate objective is to answer three questions in the current `8`-satellite `v4.3` geometry:

- whether the continuous satellite ground track is smooth while the discrete beam landing trace jumps between nearby hex cells
- whether high `PHY DL TB error / BLER` is concentrated around those jump periods or jump corridors
- which hex corridors should later receive denser UE placement for `handover / ping-pong / load` experiments

**Context**

- The current documented baseline remains `2x4`, `25 UE`, `seven-cell`, with one empty neighbor ring between the center cluster and outer UE cells.
- The user wants the eventual direction to be a new main UE layout that better exposes `handover`, `ping-pong`, and load-aware behavior.
- The current blocker is diagnostic: `BLER` remains high and the beam landing behavior appears unstable, so the layout must first expose where the landing trace actually goes.
- Replacing `seven-cell` immediately would blur the distinction between a geometry diagnosis step and a new formal baseline definition.

**Recommendation**

Use a temporary diagnostic scenario first, then decide whether the later corridor-based layout is strong enough to replace `seven-cell`.

The recommended first-step layout is:

- a local `2-ring` hex window centered on the current center hex of the existing `seven-cell` footprint
- `19` occupied hex cells total
- `1 UE` placed at the center of each occupied hex

This keeps the geometry readable, keeps compute cost below the current `25 UE` baseline, and makes `sat_anchor_trace` interpretation much cleaner than a denser uniform fill.

## Design Summary

### Phase A: `R2-diagnostic`

Purpose:

- inspect the relationship between continuous ground motion, discrete landing-cell motion, and PHY error concentration

Scenario shape:

- keep the current `8` satellites, `2x4` constellation, antenna model, beamforming mode, carrier reuse mode, and handover logic unchanged
- replace the current UE layout only for this diagnostic run
- use a `2-ring` hex region around the current center hex of the existing `seven-cell` footprint
- place exactly `1 UE` at each hex center in that `2-ring` region
- do not expand to full-area uniform filling
- do not add extra edge-offset or boundary-biased UE points in the first version

Why this shape:

- it is the smallest layout that still shows a local hex neighborhood, not just a single corridor guess
- it aligns directly with the discrete landing-cell representation
- it avoids overloading the machine while remaining spatially interpretable

### Phase B: `trajectory-corridor`

Purpose:

- use the `R2-diagnostic` result to place UE on the actual high-interest landing corridors instead of guessing where competition happens

Scenario shape:

- no longer occupy all `19` hex cells
- concentrate UE on the repeated landing-jump corridors and dual-plane competition belt discovered in Phase A
- keep UE count modest at first, targeting roughly `19-25 UE`

Expected UE roles:

- corridor UE: the main UE set placed on repeated landing-jump paths
- boundary UE: a smaller set placed on corridor intersections or strongest ambiguity cells
- background UE: a small residual set kept outside hotspots so the scene does not collapse into an all-hotspot stress test

### Phase C: candidate new baseline

Purpose:

- decide whether the corridor-based scene is strong enough to replace `seven-cell` as the main comparison scenario

This upgrade is allowed only if the new layout can stably expose:

- repeatable cross-satellite competition
- clearer `handover / ping-pong / TTT / hysteresis` behavior
- more interpretable load imbalance
- a spatially explainable link between trajectory geometry, landing-cell changes, and PHY error concentration

## Outputs To Inspect In Phase A

The first diagnostic run should focus on a narrow output set:

- `ue_layout.csv`
  - verify the `19` UE points really occupy the intended `2-ring` window
- `sat_ground_track.csv`
  - inspect the continuous projected ground motion
- `sat_anchor_trace.csv`
  - inspect discrete landing-cell transitions
- `hex_grid_cells.html`
  - overlay UE points, continuous tracks, and landing traces in one view
- `phy_dl_tb_metrics.csv`
  - inspect per-UE error concentration
- `phy_dl_tb_interval_metrics.csv`
  - inspect whether error spikes align with landing-cell jump periods

## Diagnostic Questions

Phase A is successful only if it can answer the following:

1. Is the continuous ground track smooth while the landing-cell trace still jumps between a smaller set of adjacent hex cells?
2. Are high `TB error / BLER` periods aligned mainly with landing-cell transitions or specific narrow corridors rather than the whole region?
3. Do a small number of hotspot hex cells or corridors appear repeatedly across the observation window?

If the answer is mostly yes, Phase B should place UE along those corridors.

If the answer is mostly no, then the dominant problem is more likely in beam/anchor policy or PHY interference rather than in the current UE distribution alone.

## Non-Goals For Phase A

- do not redefine the formal baseline yet
- do not tune handover parameters yet
- do not densify UE inside each hex yet
- do not change the carrier or beamforming setup just to force visible movement
- do not optimize for maximum handover count in the diagnostic run

## Acceptance Criteria For Advancing To Phase B

Move from `R2-diagnostic` to `trajectory-corridor` only when all of the following are true:

1. The `2-ring` diagnostic layout reveals a stable subset of hotspot hex cells or jump corridors rather than a purely random pattern.
2. High PHY error can be related to specific spatial regions or landing transitions.
3. The result is interpretable enough to justify targeted UE placement instead of another broad uniform fill.

## Acceptance Criteria For Replacing `seven-cell`

The later corridor-based layout may replace `seven-cell` only when it satisfies all of the following:

1. It exposes cross-satellite competition more reliably than the current baseline.
2. It makes `handover / ping-pong / load` behavior easier to compare between baseline and improved.
3. Its error and transition behavior can still be explained spatially from the plotted tracks.
4. The scenario remains small enough to run comfortably on the current machine.

## Implementation Boundary

When implementation starts, keep the first code change local to UE placement generation and scenario naming. Do not mix the first diagnostic layout patch with unrelated handover-rule or PHY-policy edits.
