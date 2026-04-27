# 几何波束参数自动统一实现计划

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 消除几何波束参数手动分离，让 BeamModelConfig 从真实 PHY 参数自动推导。

**Architecture:** 删除 LeoNtnHandoverConfig 中的几何参数字段，在配置解析时动态推导 g_beamModelConfig 参数。

**Tech Stack:** NS-3 C++, beam-link-budget.h, leo-ntn-handover-config.h

---

## Task 1: 删除几何参数字段

**Files:**
- Modify: `scratch/handover/leo-ntn-handover-config.h:113-116`

**Step 1: 删除字段定义**

删除以下字段：
```cpp
// 删除这三行（第 113-116 行）
double beamMaxGainDbi = 38.0;
double scanMaxDeg = 60.0;           // 保留（物理限制）
double theta3dBDeg = 4.0;
double sideLobeAttenuationDb = 30.0;
```

实际只删除：
```cpp
double beamMaxGainDbi = 38.0;       // 删除
double theta3dBDeg = 4.0;           // 删除
double sideLobeAttenuationDb = 30.0; // 删除
```

保留：
```cpp
double scanMaxDeg = 60.0;           // 保留（物理限制）
double ueRxGainDbi = 0.0;           // 保留（UE 接收增益）
double atmLossDb = 0.5;             // 保留（链路损耗）
```

**Step 2: 删除命令行参数绑定**

在 `leo-ntn-handover-config.h` 中搜索并删除：
```cpp
// 搜索 beamMaxGainDbi/theta3dBDeg/sideLobeAttenuationDb 的 addArg 调用
addArg("beamMaxGainDbi", config.beamMaxGainDbi);      // 删除
addArg("theta3dBDeg", config.theta3dBDeg);            // 删除
addArg("sideLobeAttenuationDb", config.sideLobeAttenuationDb); // 删除
```

**Step 3: 编译验证**

Run: `./ns3 clean && ./ns3 build`
Expected: 编译通过（可能有未使用变量警告，后续修复）

---

## Task 2: 修改配置填充逻辑

**Files:**
- Modify: `scratch/leo-ntn-handover-baseline.cc:1981-1988`

**Step 1: 替换几何参数填充代码**

找到第 1981-1988 行，替换：
```cpp
// 旧代码（删除）
g_beamModelConfig.gMax0Dbi = cfg.beamMaxGainDbi;
g_beamModelConfig.alphaMaxRad = LeoOrbitCalculator::DegToRad(cfg.scanMaxDeg);
g_beamModelConfig.theta3dBRad = LeoOrbitCalculator::DegToRad(cfg.theta3dBDeg);
g_beamModelConfig.slaVDb = cfg.sideLobeAttenuationDb;

// 新代码（动态推导）
const double arrayGainDb = 10.0 * std::log10(cfg.gnbAntennaRows * cfg.gnbAntennaColumns);
g_beamModelConfig.gMax0Dbi = cfg.b00MaxGainDb + arrayGainDb;
g_beamModelConfig.alphaMaxRad = LeoOrbitCalculator::DegToRad(cfg.scanMaxDeg);
g_beamModelConfig.theta3dBRad = LeoOrbitCalculator::DegToRad(cfg.b00BeamwidthDeg);
g_beamModelConfig.slaVDb = cfg.b00MaxAttenuationDb;
```

**Step 2: 添加必要的头文件**

确保 `leo-ntn-handover-baseline.cc` 包含 `<cmath>`（用于 `std::log10`）。

**Step 3: 编译验证**

Run: `./ns3 build`
Expected: 编译通过

---

## Task 3: 更新 beam-link-budget.h 注释

**Files:**
- Modify: `scratch/handover/beam-link-budget.h:1-17`

**Step 1: 添加推导关系注释**

在文件头部添加：
```cpp
/*
 * 文件说明：
 * `beam-link-budget.h` 用于计算卫星波束指向地面 UE 时的简化链路预算。
 *
 * 参数推导关系（与真实 PHY b00-custom 参数一致）：
 * - gMax0Dbi = b00MaxGainDb + 10*log10(gnbAntennaRows * gnbAntennaColumns)
 * - theta3dBRad = b00BeamwidthDeg（阵元波束宽度）
 * - slaVDb = b00MaxAttenuationDb
 * 
 * 几何波束用于快速估算，不重复计算阵列因子。
 */
```

**Step 2: 删除旧的冗余注释**

删除原有注释中关于"波束中心最大增益"等的硬编码说明（第 36-38 行）。

**Step 3: 编译验证**

Run: `./ns3 build`
Expected: 编译通过

---

## Task 4: 运行验证

**Files:**
- Run: `scratch/leo-ntn-handover-baseline`

**Step 1: 运行 baseline 仿真**

Run: `./ns3 run scratch/leo-ntn-handover-baseline -- --handoverMode=baseline --simTime=40`
Expected: 仿真正常完成，无断言错误

**Step 2: 检查输出**

检查 `scratch/results/` 目录输出正常：
- `e2e_flow_metrics.csv`
- `handover_event_trace.csv`
- `sat_anchor_trace.csv`

**Step 3: 对比参数变化**

记录改动后的几何 RSRP 估算值（若有输出），对比改动前。

---

## Task 5: 更新相关文档

**Files:**
- Modify: `scratch/baseline-definition.md:129`
- Modify: `docs/current-task-memory.md`

**Step 1: 更新 baseline-definition.md**

修改第 129 行：
```cpp
// 旧
- 几何参数 `beamMaxGainDbi/theta3dBDeg/sideLobeAttenuationDb` 主要用于几何链路预算与观测口径

// 新
- 几何波束参数从真实 PHY 参数自动推导：gMax0Dbi = b00MaxGainDb + arrayGainDb
```

**Step 2: 更新 current-task-memory.md**

记录改动完成状态。

**Step 3: Commit**

```bash
git add scratch/handover/leo-ntn-handover-config.h
git add scratch/leo-ntn-handover-baseline.cc
git add scratch/handover/beam-link-budget.h
git add scratch/baseline-definition.md
git add docs/current-task-memory.md
git commit -m "refactor: unify geometric beam params with PHY params

- Delete manual beamMaxGainDbi/theta3dBDeg/sideLobeAttenuationDb
- Auto-derive gMax0Dbi from b00MaxGainDb + arrayGainDb
- Update beam-link-budget.h comments"
```

---

## 验收标准

1. 编译通过
2. baseline/improved 仿真正常运行
3. 几何 RSRP 估算值 ≈ 真实 PHY RSRP（差距 < 5 dB）
4. 门控逻辑正常（锚点选择、候选覆盖检查）