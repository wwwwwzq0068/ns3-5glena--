# Simulation Results Staging

This directory keeps only the current results that are still aligned with the
active research comparison interface.

Current baseline: `research-v4.2` (published)
Pending release: `research-v4.3` (antenna migration, under review)

Retained directories:
- `exp/v4.3/`
  - pending formal comparison runs for `B00-V43` baseline vs improved
- `exp/v4.2/`
  - current formal comparison runs used for `B00` vs `I31` (published)

Recent manual diagnostic runs:
- temporary `manual/B00-*` and `manual/I31-*` folders may be created to inspect
  `PHY DL TB` behavior, `shadowingEnabled`, cross-layer gates, or antenna-array
  sensitivity
- these folders are diagnostic only unless they are later promoted into the
  current formal comparison path

Current per-run output files:
- `e2e_flow_metrics.csv`
- `phy_dl_tb_metrics.csv`
- `ue_layout.csv`
- `sat_anchor_trace.csv`
- `hex_grid_cells.csv`
- `handover_dl_throughput_trace.csv`
- `handover_event_trace.csv`

Cleanup rule:
- old version snapshots, debug runs, smoke tests, temporary verification runs,
  and any result folders that cannot be confirmed as part of the current
  `v4.2 + E2E/PHY metrics` interface should be removed instead of retained here

This directory is ignored by Git on purpose. Keep it small and treat it as a
staging area for only the runs that are still useful for the current thesis
comparison path.
