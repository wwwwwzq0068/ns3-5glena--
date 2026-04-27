# 几何波束参数自动统一设计

## 日期
2026-04-27

## 问题背景
当前代码存在两套波束参数体系：
1. **几何波束参数**（`beam-link-budget.h`）：手动配置的 `beamMaxGainDbi=38`, `theta3dBDeg=4`, `sideLobeAttenuationDb=30`
2. **真实 PHY 参数**（`b00-equivalent-antenna-model.h`）：`b00MaxGainDb=20`, `b00BeamwidthDeg=4`, `gnbAntennaRows=12`, `gnbAntennaColumns=12`

两套参数概念分离，维护时容易混淆。

## 设计目标
消除手动参数分离，让几何波束参数从真实 PHY 参数自动推导。

## 推导关系

### 标定分析
根据 `b00-equivalent-antenna-model.h` 注释：
> Initial estimate: 38 - 18 (array gain) = 20 dBi

当前标定关系：
- 目标总增益 38 dBi = 阵元增益 20 dBi + 阵列增益 18 dB（估计值）
- 阵元波束宽度 4° ≈ 总波束宽度

### 动态推导公式
采用更精确的动态计算：
```cpp
const double arrayGainDb = 10.0 * std::log10(gnbAntennaRows * gnbAntennaColumns);
gMax0Dbi = b00MaxGainDb + arrayGainDb;  // 例：20 + 21.58 ≈ 41.58 dB
theta3dBRad = DegToRad(b00BeamwidthDeg); // 例：4°
slaVDb = b00MaxAttenuationDb;            // 例：30 dB
```

### 参数变化
| 参数 | 改动前 | 改动后 | 变化 |
|------|--------|--------|------|
| `gMax0Dbi` | 38 dB | 41.58 dB | +3.58 dB |
| `theta3dBRad` | 4° | 4° | 无变化 |
| `slaVDb` | 30 dB | 30 dB | 无变化 |

几何 RSRP 估算值会更接近真实 PHY 行为。

## 改动清单

### 1. 删除几何参数字段
在 `leo-ntn-handover-config.h` 中删除：
```cpp
// 删除
double beamMaxGainDbi = 38.0;
double theta3dBDeg = 4.0;
double sideLobeAttenuationDb = 30.0;

// 保留（非天线参数）
double scanMaxDeg = 60.0;      // 物理限制
double ueRxGainDbi = 0.0;      // UE 接收增益
double atmLossDb = 0.5;        // 链路损耗
```

### 2. 修改配置填充逻辑
在 `leo-ntn-handover-baseline.cc` 第 1981-1988 行：
```cpp
// 删除旧代码
g_beamModelConfig.gMax0Dbi = cfg.beamMaxGainDbi;
g_beamModelConfig.theta3dBRad = DegToRad(cfg.theta3dBDeg);
g_beamModelConfig.slaVDb = cfg.sideLobeAttenuationDb;

// 新增动态推导
const double arrayGainDb = 10.0 * std::log10(cfg.gnbAntennaRows * cfg.gnbAntennaColumns);
g_beamModelConfig.gMax0Dbi = cfg.b00MaxGainDb + arrayGainDb;
g_beamModelConfig.theta3dBRad = LeoOrbitCalculator::DegToRad(cfg.b00BeamwidthDeg);
g_beamModelConfig.slaVDb = cfg.b00MaxAttenuationDb;
```

### 3. 更新注释
在 `beam-link-budget.h` 头部添加推导关系说明。

## 验证方案

### 编译验证
- 确保删除字段后编译通过

### 运行验证
- 运行 baseline 仿真，检查锚点选择和候选门控日志正常
- 几何 RSRP 估算值更接近真实 PHY RSRP

### 回归验证
- baseline/improved 仿真流程不受影响

## 影响评估

### 低风险
- 参数推导逻辑简单
- 不改变门控逻辑（`beamLocked`, `offBoresight` 检查）
- 不改变真实 PHY beamforming 实现

### 需注意
- 几何 RSRP 估算值提高约 3.58 dB
- 若依赖几何 RSRP 的阈值（如 `improvedMinCandidateRsrpDbm=-110`），可能需要调整

## 后续工作
- 实现改动
- 运行验证
- 更新文档