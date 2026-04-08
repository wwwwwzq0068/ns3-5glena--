# 联合切换策略参数审核清单

## 审核目标
核实当前联合切换策略参数设置是否与 LEO-NTN 场景的时间尺度匹配。

说明：
- 本文件是“审核问题清单”，不是当前默认参数的正式来源
- 精确默认值请以 `scratch/handover/leo-ntn-handover-config.h` 和 `scratch/baseline-definition.md` 为准

## 审核依据
- LEO 卫星运动速度：约 7.5 km/s
- 卫星高度：600 km
- 单次过境可见时间：通常几分钟
- 当前开启 Shadowing（阴影衰落）

---

## 问题清单

### P1: 测量上报周期与TTT关系不合理

**现状**：
- `measurementReportIntervalMs = 120 ms`
- `hoTttMs = 200 ms`

**问题**：
- TTT 期间最多只有 1~2 次测量报告
- 第一次报告触发条件，第二次报告确认持续满足
- 边界情况下 TTT 判断非常脆弱，不够稳健

**建议**：
- TTT 应为测量周期的整数倍
- TTT 期间至少需要 3 次以上报告确认
- 建议：`measurementReportIntervalMs = 100 ms`，`hoTttMs = 300~400 ms`

**涉及文件**：
- `scratch/handover/leo-ntn-handover-config.h`
- `docs/current-task-memory.md`
- `scratch/baseline-definition.md`

---

### P2: 可见性预测步长过粗

**现状**：
- `improvedVisibilityPredictionStepSeconds = 0.2 s`
- `improvedMinVisibilitySeconds = 1.0 s`

**问题**：
- 预测步长 0.2s 时，时间分辨率约为 `±0.1 s`
- 最小可见门槛 1.0s 仍然不算宽裕，门槛与预测步长只差 5 个步长
- 1秒内卫星移动 7.5 km，可见性变化快
- 因此仍需验证“1.0 s 门槛 + 0.2 s 步长”在边界切换时是否足够稳健

**建议**：
- 预测步长缩小到 0.1~0.2s
- 或最小可见门槛提高到 2.0s 以上

**涉及文件**：
- `scratch/handover/leo-ntn-handover-config.h`
- `scratch/leo-ntn-handover-baseline.cc`

---

### P3: 回切保护时间配置混乱

**现状**：
- 当前代码与当前主文档都已统一为 `improvedReturnGuardSeconds = 0.5 s`
- `pingPongWindowSeconds = 1.5 s`

**问题**：
- 历史上曾出现过 `1.5 s` 与 `0.5 s` 的文档漂移，说明这一项很容易在代码和文档之间失同步
- 当前真正需要核实的是“0.5 s 是否足够”，而不是继续沿用已经过期的 `1.5 s` 旧值
- 回切保护当前只限制“短时回到上一跳来源小区”，语义上不等于全局 `ping-pong` 窗口，因此不能简单写成“必须大于 ping-pong window”

**建议**：
- 继续以 `0.5 s` 作为当前代码/文档一致值
- 若要判断是否偏短，应通过 `0.5 / 1.0 / 1.5 s` 的实验对照来决定，而不是先验固定为“大于 ping-pong 窗口”

**涉及文件**：
- `scratch/handover/leo-ntn-handover-config.h`
- `docs/current-task-memory.md`

---

### P4: 主循环与测量上报周期不同步

**现状**：
- `updateIntervalMs = 100 ms`（主循环更新周期）
- `measurementReportIntervalMs = 120 ms`（测量上报周期）

**问题**：
- 两个周期不同步
- 100ms 更新负载状态，120ms 上报测量
- 负载信息可能滞后 20ms

**建议**：
- 统一为 100 ms
- 或让测量周期是主循环的整数倍

**涉及文件**：
- `scratch/handover/leo-ntn-handover-config.h`

---

### P5: 信号差距门槛在LEO+Shadowing场景偏小

**现状**：
- `improvedMaxSignalGapDb = 3.0 dB`

**问题**：
- LEO 卫星运动快，信号变化快
- 已开启 Shadowing，信号波动更大
- 3 dB 差距可能过于严格，排除潜在优质候选

**建议**：
- 提高到 4~5 dB

**涉及文件**：
- `scratch/handover/leo-ntn-handover-config.h`

---

### P6: 权重配置文件与文档不一致

**现状**：
- `leo-ntn-handover-config.h` 中权重：signal=0.7, load=0.3, visibility=0.2
- `current-task-memory.md` 中相同，无问题

**但需核实**：
- 实际运行时是否正确读取这些权重
- 动态权重调整逻辑是否合理

**涉及文件**：
- `scratch/handover/leo-ntn-handover-config.h`
- `scratch/leo-ntn-handover-baseline.cc`（动态调整）

---

## 审核要求

1. **逐项核实**：每个问题是否真实存在于代码中
2. **追溯根因**：为什么会有这样的设置（历史原因/误解/遗漏）
3. **确认影响**：这些设置对仿真结果的实际影响
4. **提出修正**：给出具体的修正方案和修正后需验证的内容

---

## 时间尺度参考

| 时间尺度 | 物理含义 |
|----------|----------|
| 1 ms | 几乎静止 |
| 10 ms | 卫星移动 ~75 m |
| 100 ms | 卫星移动 ~750 m |
| 1 s | 卫星移动 ~7.5 km |
| 10 s | 卫星移动 ~75 km，可能半个波束宽度 |

LEO-NTN 切换决策窗口通常在 **秒级**，参数设置应与此匹配。

---

## 附录：当前全部参数

| 参数 | 代码值 | 文档值 | 状态 |
|------|--------|--------|------|
| `updateIntervalMs` | 100 | 100 | 一致 |
| `measurementReportIntervalMs` | 120 | 120 | 一致 |
| `hoHysteresisDb` | 2.0 | 2.0 | 一致 |
| `hoTttMs` | 200 | 200 | 一致 |
| `improvedSignalWeight` | 0.7 | 0.7 | 一致 |
| `improvedLoadWeight` | 0.3 | 0.3 | 一致 |
| `improvedVisibilityWeight` | 0.2 | 0.2 | 一致 |
| `improvedMinLoadScoreDelta` | 0.2 | 0.2 | 一致 |
| `improvedMaxSignalGapDb` | 3.0 | 3.0 | 一致 |
| `improvedReturnGuardSeconds` | 0.5 | 0.5 | 一致 |
| `improvedMinVisibilitySeconds` | 1.0 | 1.0 | 一致 |
| `improvedVisibilityHorizonSeconds` | 8.0 | 8.0 | 一致 |
| `improvedVisibilityPredictionStepSeconds` | 0.2 | 0.2 | 一致 |
| `pingPongWindowSeconds` | 1.5 | 1.5 | 一致 |
| `loadCongestionThreshold` | 0.8 | 0.8 | 一致 |
| `maxSupportedUesPerSatellite` | 5.0 | 5.0 | 一致 |
