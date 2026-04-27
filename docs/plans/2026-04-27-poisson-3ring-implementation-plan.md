# poisson-3ring UE Layout Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace seven-cell baseline with poisson-3ring layout: 19 hex cells with truncated Poisson UE distribution.

**Architecture:** Add new UE layout type "poisson-3ring" using truncated Poisson(λ=1.5) per cell, capped at 5 UE/cell. UE positions randomly placed within hex boundaries.

**Tech Stack:** ns-3.46, C++, existing WGS84 hex grid infrastructure.

---

### Task 1: Add Configuration Parameters

**Files:**
- Modify: `scratch/handover/leo-ntn-handover-config.h:23-185`

**Step 1: Add new parameters to BaselineSimulationConfig**

Add after line 33 (after `ueRingPointOffsetMeters`):

```cpp
double poissonLambda = 1.5;
uint32_t maxUePerCell = 5;
```

**Step 2: Register command line options**

In `RegisterBaselineCommandLineOptions`, add after line 199:

```cpp
addArg("poissonLambda", config.poissonLambda);
addArg("maxUePerCell", config.maxUePerCell);
```

**Step 3: Add validation**

In `ValidateBaselineSimulationConfig`, modify line 447-457 validation block:

Replace:
```cpp
NS_ABORT_MSG_IF(config.ueLayoutType != "line" && config.ueLayoutType != "seven-cell" &&
                    config.ueLayoutType != "r2-diagnostic",
                "ueLayoutType must be one of: line, seven-cell, r2-diagnostic");
```

With:
```cpp
NS_ABORT_MSG_IF(config.ueLayoutType != "line" && config.ueLayoutType != "seven-cell" &&
                    config.ueLayoutType != "r2-diagnostic" &&
                    config.ueLayoutType != "poisson-3ring",
                "ueLayoutType must be one of: line, seven-cell, r2-diagnostic, poisson-3ring");
```

Add new validation after the existing layout-specific checks:
```cpp
NS_ABORT_MSG_IF(config.ueLayoutType == "poisson-3ring" && config.poissonLambda <= 0.0,
                "poissonLambda must be > 0");
NS_ABORT_MSG_IF(config.ueLayoutType == "poisson-3ring" && config.maxUePerCell == 0,
                "maxUePerCell must be >= 1");
```

**Step 4: Change default ueLayoutType**

Change line 30:
```cpp
std::string ueLayoutType = "seven-cell";
```

To:
```cpp
std::string ueLayoutType = "poisson-3ring";
```

**Step 5: Commit**

```bash
git add scratch/handover/leo-ntn-handover-config.h
git commit -m "config: add poisson-3ring layout parameters"
```

---

### Task 2: Implement Poisson 3-Ring Offset Specs Generator

**Files:**
- Modify: `scratch/handover/leo-ntn-handover-runtime.h:510-600`

**Step 1: Add UeLayoutConfig fields**

Modify `UeLayoutConfig` struct (line 286-302), add after `ringPointOffsetMeters`:

```cpp
double poissonLambda = 1.5;
uint32_t maxUePerCell = 5;
uint32_t randomSeed = 0;  // 0 means use simulator's global seed
```

**Step 2: Implement truncated Poisson sampling helper**

Add after line 544 (after `BuildR2DiagnosticUeOffsetSpecs`):

```cpp
/**
 * Sample from truncated Poisson distribution.
 * Returns min(maxUePerCell, Poisson(lambda)).
 */
inline uint32_t
SampleTruncatedPoisson(double lambda, uint32_t maxUePerCell, std::mt19937& rng)
{
    std::poisson_distribution<uint32_t> dist(lambda);
    uint32_t sample = dist(rng);
    return std::min(sample, maxUePerCell);
}

/**
 * Generate random point inside a hexagon centered at origin.
 * Uses rejection sampling: generate point in bounding box, reject if outside hex.
 */
inline std::pair<double, double>
SampleRandomPointInHex(double hexRadiusMeters, std::mt19937& rng)
{
    const double boxHalfWidth = std::sqrt(3.0) * hexRadiusMeters;
    const double boxHalfHeight = 2.0 * hexRadiusMeters;
    std::uniform_real_distribution<double> uniformEast(-boxHalfWidth, boxHalfWidth);
    std::uniform_real_distribution<double> uniformNorth(-boxHalfHeight, boxHalfHeight);
    
    while (true)
    {
        const double east = uniformEast(rng);
        const double north = uniformNorth(rng);
        
        // Hexagon point test: |x| <= sqrt(3)*r and |y| <= 2*r
        // and |x| <= sqrt(3)*r - |y|/sqrt(3)
        const double absEast = std::abs(east);
        const double absNorth = std::abs(north);
        
        if (absNorth <= 2.0 * hexRadiusMeters &&
            absEast <= std::sqrt(3.0) * hexRadiusMeters &&
            absEast <= std::sqrt(3.0) * hexRadiusMeters - absNorth / std::sqrt(3.0))
        {
            return {east, north};
        }
    }
}
```

**Step 3: Implement BuildPoisson3RingUeOffsetSpecs**

Add after the helper functions:

```cpp
/**
 * Build UE offset specs for poisson-3ring layout.
 * 19 hex cells (center + ring1 6 + ring2 12), truncated Poisson per cell.
 */
inline std::vector<UePlacementOffsetSpec>
BuildPoisson3RingUeOffsetSpecs(uint32_t ueNumMax, const UeLayoutConfig& layout)
{
    std::vector<UePlacementOffsetSpec> specs;
    
    // Seed RNG: use layout.randomSeed if > 0, else deterministic based on hex grid
    std::mt19937 rng;
    if (layout.randomSeed > 0)
    {
        rng.seed(layout.randomSeed);
    }
    else
    {
        rng.seed(42);  // Default seed for reproducibility
    }
    
    const double hexRadius = layout.hexCellRadiusMeters;
    const double dx = std::sqrt(3.0) * hexRadius;
    const double dy = 1.5 * hexRadius;
    
    // Generate 19 hex centers: center (r=0) + ring1 (r=1) + ring2 (r=2)
    std::vector<std::pair<double, double>> hexCenters;
    for (int r = -2; r <= 2; ++r)
    {
        for (int q = -2; q <= 2; ++q)
        {
            const int s = -q - r;
            const int ringDistance = std::max({std::abs(q), std::abs(r), std::abs(s)});
            if (ringDistance > 2)
            {
                continue;
            }
            const double eastMeters = dx * (static_cast<double>(q) + 0.5 * static_cast<double>(r));
            const double northMeters = dy * static_cast<double>(r);
            hexCenters.emplace_back(eastMeters, northMeters);
        }
    }
    
    // For each hex, sample UE count from truncated Poisson
    uint32_t totalUe = 0;
    for (uint32_t cellIdx = 0; cellIdx < hexCenters.size(); ++cellIdx)
    {
        if (totalUe >= ueNumMax)
        {
            break;
        }
        
        const uint32_t ueInCell = SampleTruncatedPoisson(
            layout.poissonLambda, layout.maxUePerCell, rng);
        
        const auto& [centerEast, centerNorth] = hexCenters[cellIdx];
        const std::string role = cellIdx == 0 ? "p3-center"
                                 : cellIdx <= 6 ? "p3-ring1"
                                 : "p3-ring2";
        
        for (uint32_t ueIdx = 0; ueIdx < ueInCell && totalUe < ueNumMax; ++ueIdx)
        {
            const auto [localEast, localNorth] = SampleRandomPointInHex(hexRadius, rng);
            AppendUeOffsetSpec(specs,
                               centerEast + localEast,
                               centerNorth + localNorth,
                               role);
            ++totalUe;
        }
    }
    
    return specs;
}
```

**Step 4: Update BuildUePlacements**

Modify `BuildUePlacements` function (line 577-600), add new branch after `r2-diagnostic`:

```cpp
else if (layout.layoutType == "poisson-3ring")
{
    offsetSpecs = BuildPoisson3RingUeOffsetSpecs(ueNum, layout);
}
```

**Step 5: Commit**

```bash
git add scratch/handover/leo-ntn-handover-runtime.h
git commit -m "runtime: add BuildPoisson3RingUeOffsetSpecs generator"
```

---

### Task 3: Update Main Script Logging

**Files:**
- Modify: `scratch/leo-ntn-handover-baseline.cc:2270-2290`

**Step 1: Add poisson-3ring logging branch**

In the UE layout logging section (around line 2280), add after `r2-diagnostic` branch:

```cpp
else if (cfg.ueLayoutType == "poisson-3ring")
{
    std::cout << " cells=19(three-ring)"
              << " lambda=" << cfg.poissonLambda
              << " maxUePerCell=" << cfg.maxUePerCell
              << " expectedTotal=" << static_cast<int>(19 * cfg.poissonLambda)
              << " layoutHexRadius=" << cfg.hexCellRadiusKm << "km";
}
```

**Step 2: Update UeLayoutConfig population**

Around line 2527, add new fields to `ueLayout`:

```cpp
ueLayout.poissonLambda = cfg.poissonLambda;
ueLayout.maxUePerCell = cfg.maxUePerCell;
```

**Step 3: Update UE placement call**

The existing call at line 2533 already uses `cfg.ueNum` as max. For poisson-3ring, `ueNum` acts as optional upper bound. Keep existing logic, but note that actual UE count may be less.

**Step 4: Update actual UE count handling**

After line 2534 (after `DumpUeLayoutCsv`), add:

```cpp
if (cfg.ueLayoutType == "poisson-3ring")
{
    std::cout << "[UE-Layout] poisson-3ring generated " << uePlacements.size()
              << " UE (max=" << cfg.ueNum << ")" << std::endl;
}
```

**Step 5: Commit**

```bash
git add scratch/leo-ntn-handover-baseline.cc
git commit -m "baseline: add poisson-3ring layout logging"
```

---

### Task 4: Update Validation and Defaults Test

**Files:**
- Modify: `scratch/test-baseline-defaults.cc:29-31`

**Step 1: Update default layout test**

Change line 29:
```cpp
Require(config.ueLayoutType == "seven-cell", "baseline default should keep seven-cell layout");
```

To:
```cpp
Require(config.ueLayoutType == "poisson-3ring", "baseline default should use poisson-3ring layout");
```

**Step 2: Remove fixed UE count check**

Change line 30:
```cpp
Require(config.ueNum == 25, "baseline default should keep 25 UE");
```

To (ueNum now acts as max bound, not fixed count):
```cpp
Require(config.ueNum >= 20 && config.ueNum <= 35, "baseline default ueNum should allow 20-35 range");
```

Or simply remove this check since UE count is now dynamic.

**Step 3: Add parameter checks**

Add after existing checks:
```cpp
Require(config.poissonLambda == 1.5, "baseline default should use lambda=1.5");
Require(config.maxUePerCell == 5, "baseline default should use maxUePerCell=5");
```

**Step 4: Commit**

```bash
git add scratch/test-baseline-defaults.cc
git commit -m "test: update baseline defaults for poisson-3ring"
```

---

### Task 5: Build and Smoke Test

**Step 1: Build the project**

```bash
cd /Users/mac/Desktop/workspace/ns-3.46
cmake --build build --target leo-ntn-handover-baseline
```

Expected: Build succeeds without errors.

**Step 2: Run smoke test**

```bash
./build/scratch/leo-ntn-handover-baseline --simTime=10 --ueLayoutType=poisson-3ring --ueNum=30
```

Expected:
- Program starts and runs without abort
- Log shows "poisson-3ring" layout with expected parameters
- `ue_layout.csv` contains approximately 28-30 UE
- `hex_grid_cells.csv` contains cells (check that 19 are available)

**Step 3: Check output files**

```bash
head -5 scratch/results/ue_layout.csv
wc -l scratch/results/ue_layout.csv
```

Expected: UE count around 28-30, roles show p3-center/p3-ring1/p3-ring2.

**Step 4: Commit**

```bash
git add scratch/results/
git commit -m "test: smoke test poisson-3ring layout"
```

---

### Task 6: Update Documentation

**Files:**
- Modify: `scratch/baseline-definition.md`
- Modify: `docs/current-task-memory.md`
- Modify: `scratch/README.md`

**Step 1: Update baseline-definition.md**

Update the baseline definition section to reflect new layout:
- Change "UE 使用带一圈空白邻区的 seven-cell" to "UE 使用 poisson-3ring 三跳布局"
- Update ueNum from fixed 25 to "泊松 λ=1.5，期望约 28 UE"
- Add description of truncated Poisson distribution

**Step 2: Update current-task-memory.md**

Update:
- `ueLayoutType` default from "seven-cell" to "poisson-3ring"
- `ueNum` from fixed 25 to dynamic (max bound)
- Add `poissonLambda = 1.5` and `maxUePerCell = 5`

**Step 3: Update scratch/README.md**

Add poisson-3ring layout description in UE layout section.

**Step 4: Commit**

```bash
git add scratch/baseline-definition.md docs/current-task-memory.md scratch/README.md
git commit -m "docs: update baseline docs for poisson-3ring layout"
```

---

### Task 7: Verify Research Behavior

**Step 1: Run baseline comparison**

```bash
./build/scratch/leo-ntn-handover-baseline --simTime=40 --ueLayoutType=poisson-3ring --handoverMode=baseline --outputDir=scratch/results/manual/poisson3ring-baseline
```

**Step 2: Check handover events**

```bash
cat scratch/results/manual/poisson3ring-baseline/handover_event_trace.csv | wc -l
```

Expected: Non-zero handover events (not empty trace).

**Step 3: Check satellite load distribution**

Look at console output or trace files for satellite attachment counts. Expected: More even distribution across 8 satellites.

**Step 4: Compare with seven-cell**

Run same test with seven-cell and compare UE count, handover events, and satellite load.

---

## Summary

| Task | Files Modified | Commit |
|---|---|---|
| Task 1 | leo-ntn-handover-config.h | config: add poisson-3ring layout parameters |
| Task 2 | leo-ntn-handover-runtime.h | runtime: add BuildPoisson3RingUeOffsetSpecs generator |
| Task 3 | leo-ntn-handover-baseline.cc | baseline: add poisson-3ring layout logging |
| Task 4 | test-baseline-defaults.cc | test: update baseline defaults for poisson-3ring |
| Task 5 | (build & run) | test: smoke test poisson-3ring layout |
| Task 6 | baseline-definition.md, current-task-memory.md, README.md | docs: update baseline docs for poisson-3ring layout |
| Task 7 | (verification) | - |

**Backward Compatibility:**
- seven-cell and r2-diagnostic preserved via ueLayoutType parameter
- Historical experiments reproducible with explicit --ueLayoutType=seven-cell