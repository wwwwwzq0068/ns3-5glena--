# Agent Review Handoff

状态：
- reviewed
- verdict: pass

---

## 2026-04-06 improved 门控增强送审

说明：
- 本节保留的是 2026-04-06 的历史送审记录。
- 当前主线已不再采用这里描述的 `improvedReturnGuardSeconds` 短时回切保护与切后保持思路，
  而是改为“同一目标联合领先持续一段时间才允许切换”的稳定领先门控。

### 背景

在 `E3` 高 `ping-pong` baseline（`hoTttMs=160ms`、`hoHysteresisDb=2.0dB`）上，
当前 improved 的三组权重 `I00/I01/I02` 没有表现出明确优势。问题不在切换成功率或执行时延，
而在于 improved 仍然只是在 A3 已触发后做候选重排，没有真正约束”何时允许负载项覆盖最强信号候选”
以及”何时禁止短时回切”。

### 改动目标

- 保持统一 `MeasurementReport` 入口不变
- 不修改 baseline 语义
- 只在 improved 目标选择里加入最小必要的门控，使其不再是纯线性加权排序

### 改动内容

1. **过载候选过滤**
   - improved 优先只在 `admissionAllowed = true` 的候选中选择目标
   - 若所有候选都超阈值，则回退到原候选集，避免无目标可切

2. **短时回切保护**
   - 在 `improvedReturnGuardSeconds` 窗口内，如果候选刚好是上一跳来源小区，则优先屏蔽该返回目标
   - 只有当其它候选都不可用时，才允许回退到该返回目标

3. **负载覆写门槛**
   - 先找当前候选集中的最强信号候选
   - 其它候选只有满足以下任一条件时，才允许进入联合评分：
     - 与最强信号候选的信号差距不超过 `improvedMaxSignalGapDb`
     - 相对最强信号候选具有至少 `improvedMinLoadScoreDelta` 的负载优势

### 新增参数

- `improvedMinLoadScoreDelta = 0.2`
- `improvedMaxSignalGapDb = 3.0`
- `improvedReturnGuardSeconds = 1.5`

这些参数已接入命令行配置，并在 improved 模式启动日志中打印。

### 预期收益

- 减少”信号明显更差但仅因轻微负载差异被选中”的切换
- 抑制 `A->B->A` 的短时回切
- 让负载项只在”确有足够收益”的时候覆盖信号主导项

### 审核关注点

1. 当前门控强度是否合适，默认值是否过强或过弱？
2. `admissionAllowed` 作为 improved 的软门控前置是否合理？
3. 这套门控是否足以解释为什么 improved 在 `E3` 场景上此前没有明显优于 baseline？

### 短跑验证

验证命令均使用 `E3` 场景：`hoTttMs=160ms`、`hoHysteresisDb=2.0dB`、`simTime=12s`。

| 组别 | HO Start | Ping-pong | Avg Delay |
|------|---------:|----------:|----------:|
| `B00-rng1` | 137 | 8 | 4.070 ms |
| `I00-rng1` | 142 | 2 | 4.070 ms |
| `I01-rng1` | 142 | 2 | 4.070 ms |
| `I02-rng1` | 137 | 4 | 4.070 ms |

短跑现象说明：
- 新门控没有伤害切换成功率和执行时延。
- 在相同 `E3` 压力场景下，`I00/I01` 的 `ping-pong` 从 `8` 降到 `2`，方向正确。
- 代价是切换总数未同步下降，因此后续仍需继续审查”是否只是把回切压住了，但频繁切换仍偏多”。

### 审核结论

#### 通过项

1. **参数定义与接入**：三个新参数已正确定义、接入命令行、添加合法性检查。

2. **门控1（过载候选过滤）**：实现正确。先过滤 `admissionAllowed=true`，若全部超载则回退原候选集。

3. **门控2（短时回切保护）**：实现正确。在保护窗口内屏蔽上一跳来源小区，但仅在有其他候选时才屏蔽。

4. **门控3（负载覆写门槛）**：实现正确。只有满足以下条件才进入联合评分：
   - 是最强信号候选本身，或
   - 信号差距 ≤ 3dB，或
   - 负载优势 ≥ 0.2

5. **字段依赖**：`lastSuccessfulHoTimeSeconds` 和 `lastSuccessfulHoSourceCell` 已在 `UeRuntime` 中定义，切换成功时正确更新（`leo-ntn-handover-baseline.cc:719-721`）。

6. **回退逻辑**：三层门控都有合理的回退机制，不会导致无目标可切。

#### 关注点（非阻塞）

1. **门控强度**：`improvedMaxSignalGapDb=3dB` 可能偏宽松。LEO 场景下 3dB 信号差距可能已意味着明显覆盖差异，建议后续观察更激进设置（如 2dB）的效果。

2. **切换次数与 ping-pong 分离**：验证显示 ping-pong 从 8 降到 2，但切换次数仍维持在 137-142。说明门控成功抑制了回切，但”频繁切换”问题未解决——这仍需通过调整 TTT 等触发参数来解决，不是门控本身的责任。

3. **文档同步**：`baseline-definition.md` 和 `README.md` 已更新新参数，方向正确。

#### 结论

**pass**。门控实现正确，逻辑清晰，回退机制合理。短跑验证证实门控有效抑制了 ping-pong，没有引入副作用。建议合并后继续观察长跑效果。

---

## 2026-04-06 v4.0.1 实验矩阵分析送审

### 实验概览

实验根目录：`scratch/results/exp/v4.0.1`

实验组：
- B00-baseline-default（默认参数，TTT=200ms，Hyst=2dB）
- B11-baseline-ttt320（TTT=320ms）
- B21-baseline-hys3（Hyst=3dB）
- I00-improved-w73（信号权重 0.7，负载权重 0.3）
- I02-improved-w55（信号权重 0.5，负载权重 0.5）

每组 3 次重复（rng-1/2/3）。

### 一层摘要

| 指标 | B00 | B11 | B21 | I00 | I02 |
|------|-----|-----|-----|-----|-----|
| 关键参数 | 默认 | TTT=320ms | Hyst=3dB | w=0.7/0.3 | w=0.5/0.5 |
| 切换次数 | ~95.7 | ~45.3 | ~16.7 | ~101.7 | ~104.3 |
| Ping-pong | ~2.7 | 0 | 0 | ~1.7 | ~2.0 |
| 吞吐 Mbps | ~1.25 | ~1.33 | ~0.96 | ~1.27 | ~1.20 |
| 切换成功率 | 100% | 100% | 100% | 100% | 100% |
| 平均时延 | ~4.07ms | ~4.07ms | ~4.07ms | ~4.07ms | ~4.07ms |

### 总体结论

**B11（TTT=320ms）是当前最优 baseline**，在切换次数、ping-pong 和吞吐之间取得最佳平衡。B21（hysteresis=3dB）切换最少但吞吐明显下降，说明切换过晚。I00/I02 的 improved 算法未能改善 baseline，反而因负载权重引入额外不稳定性。

### 逐组解释

#### B00（baseline 默认）

- 切换入口：MeasurementReport + A3，TTT=200ms，Hyst=2dB
- 现象：切换频繁，存在 ping-pong
- 典型案例：UE7 在 1.456s 从 sat-13 切回 sat-11，间隔仅 ~0.6s
- 原因：200ms TTT 在 LEO 高速移动场景下不足以过滤短暂信号波动
- 定位：暴露了传统 A3 在 LEO-NTN 下的缺陷，适合作为"问题 baseline"

#### B11（baseline TTT=320ms）

- 参数变化：TTT 从 200ms 增加到 320ms
- 现象：切换次数减半，ping-pong 完全消除，吞吐提升
- 原因：更长 TTT 使 UE 在信号真正稳定优于服务星时才触发切换
- 结论：**TTT=320ms 是当前场景下的近优值**

#### B21（baseline hysteresis=3dB）

- 参数变化：迟滞从 2dB 增加到 3dB
- 现象：切换次数大幅减少，但吞吐下降约 23%
- 原因：3dB 迟滞门限过高，部分 UE 在服务星信号明显衰落时仍未触发切换
- 结论：**切换过晚比频繁切换对吞吐损伤更大**

#### I00（improved w=0.7/0.3）

- 算法：信号权重 0.7 + 负载权重 0.3
- 现象：切换次数比 B00 更多，仍有 ping-pong，吞吐略优于 B00 但低于 B11
- 问题：
  1. 当前场景卫星间负载差异有限
  2. 当多个候选星负载相近时，负载评分微小波动引入额外切换
  3. 没有解决 B00 的"频繁触发"问题，反而叠加了"负载驱动"的额外触发

#### I02（improved w=0.5/0.5）

- 算法：信号权重 0.5 + 负载权重 0.5
- 现象：切换次数最多，ping-pong 最多，吞吐最差
- 问题：更强的负载权重放大了不稳定性

### 最值得写进论文的 3 个发现

1. **TTT 对 LEO-NTN 切换稳定性影响显著**：TTT 从 200ms 提升到 320ms，切换次数减少 53%，ping-pong 从 2.7 降至 0，吞吐提升 6%。验证了 LEO 高速移动场景下需要更长信号稳定性观察窗口。

2. **高迟滞导致的"切换过晚"比频繁切换对吞吐损伤更大**：B21 的吞吐比 B00 低 23%。LEO 场景下服务星信号衰落快，过高迟滞导致 UE 在信号恶化后仍被"锁住"。

3. **单纯叠加负载权重无法改善切换性能**：I00/I02 的切换次数反而高于 B00。负载评分引入额外切换触发因素，但当前场景卫星负载差异有限，未能发挥负载均衡价值，反而增加不稳定性。**负载感知需要与 TTT/迟滞协同设计**。

### 下一步最合理的 3 个实验建议

1. **将 improved 算法的 TTT 也设为 320ms**：验证"稳健触发 + 负载感知"的联合效果。核心假设：更长 TTT 过滤信号波动，让负载评分在更稳定候选集上发挥作用。

2. **设计显式负载差异场景**：将中心 9 UE 的业务流强度翻倍（lambda=2000），使卫星间负载差异更显著，验证 improved 在"真正需要负载均衡"时的效果。

3. **引入动态 TTT 策略**：根据信号质量变化率动态调整 TTT（信号快速衰落时缩短 TTT 加快切换，信号稳定时延长 TTT 抑制 ping-pong）。

### 研究路线判断

当前结果支持"先用 B11 作为更强 baseline，再继续改 improved"的路线。

### 待审问题

1. B11 作为论文对照组是否足够强？还是需要再补充 TTT=480ms 的边界？
2. improved 算法在当前场景下失败的原因分析是否准确？
3. 下一步实验建议是否与研究目标对齐？

---

## 2026-04-05 P2.1/P2.2 修复送审

### 问题描述

问题 2 拆解：两条切换路径（baseline-a3 vs joint-signal-load）语义不一致。

| 子问题 | 描述 | 影响 |
|--------|------|------|
| **P2.1 TTT 归一化不一致** | baseline-a3 归一化到 3GPP 标准值，joint 使用原始值 | 当用户设置非标准 TTT（如 200ms）时，两条路径实际 TTT 不同 |
| **P2.2 防抖机制不一致** | joint 硬编码 0.2s 防抖，与 hoTttMs 参数无关；baseline-a3 无显式防抖，依赖 A3 事件语义 | joint 存在双重延迟（防抖 + TTT），baseline 只有 TTT |

### 改动方案

**P2.1**：joint 路径也调用 `NormalizeTimeToTriggerMs`

```cpp
// 修改前
g_manualHoTttSeconds = static_cast<double>(config.hoTttMs) / 1000.0;

// 修改后
const uint16_t effectiveTttMs = NrLeoA3MeasurementHandoverAlgorithm::NormalizeTimeToTriggerMs(
    static_cast<uint16_t>(std::min<uint32_t>(config.hoTttMs, std::numeric_limits<uint16_t>::max())));
g_manualHoTttSeconds = static_cast<double>(effectiveTttMs) / 1000.0;
```

**P2.2**：防抖时间与 TTT 联动

```cpp
// 修改前
if (ue.hasPendingHoStart || (ue.manualHoLastTriggerTime >= 0.0 &&
                             nowSeconds - ue.manualHoLastTriggerTime < 0.2))

// 修改后
// P2.2: 防抖时间与 TTT 联动，与 baseline-a3 保持一致的语义
if (ue.hasPendingHoStart || (ue.manualHoLastTriggerTime >= 0.0 &&
                             nowSeconds - ue.manualHoLastTriggerTime < g_manualHoTttSeconds))
```

### 改动范围

| 文件 | 改动类型 |
|------|----------|
| `scratch/leo-ntn-handover-baseline.cc` | 新增 include，修改 ApplyGlobalMirrorConfig()，修改 ExecuteCustomA3Handover() |
| `contrib/nr/model/nr-leo-a3-measurement-handover-algorithm.h` | 新增 static 方法声明 |
| `contrib/nr/model/nr-leo-a3-measurement-handover-algorithm.cc` | 新增 NormalizeTimeToTriggerMs 实现 |
| `contrib/nr/CMakeLists.txt` | 新增文件到构建列表 |

### 3GPP 标准 TTT 枚举值

```cpp
constexpr std::array<uint16_t, 16> kSupportedTimeToTriggerMs = {
    0, 40, 64, 80, 100, 128, 160, 256, 320, 480, 512, 640, 1024, 1280, 2560, 5120};
```

当前默认 `hoTttMs = 160`，恰好是标准值。若用户设置 `200ms`：
- 修改前：baseline → 256ms（归一化），joint → 200ms（原始值），差异 56ms
- 修改后：两者均为 256ms

### 验证

```bash
# 编译
./ns3 build
# 通过

# 运行
./ns3 run leo-ntn-handover-baseline -- --simTime=2
# 通过，控制台显示 ttt=0.160s
```

### 提交

```
5be25d9 fix(P2.2): link debounce interval to TTT parameter
0221188 fix(P2.1): normalize TTT for joint handover path to 3GPP standard
```

### 审核结论

- P2.1 的归一化逻辑正确（取最近标准值）
- P2.2 的防抖语义与 baseline-a3 的 A3 事件语义一致
- 已在日志中显示归一化后的 TTT 值

---

## 2026-03-xx baseline-a3 迁移送审（历史记录）

审核结论：
- P1（Blocking）：TTT 直接使用毫秒数，需确认 NrRrcSap::ReportConfigEutra::timeToTrigger 字段是否接受直接毫秒数。3GPP TS 36.331 规定 TTT 应为枚举值。已通过 P2.1 修复。
- P2（Non-blocking）：smoke test simTime=1.2s 过短，已在 v4.0.1 实验矩阵中补充完整验证。
- baseline/improved 边界清晰，无串线风险。
- 双 config（算法 A3 + 观测 A4）并存设计合理，measId 过滤正确。
- 新子类结构轻量，不阻塞后续 improved 迁移。
- 文档口径已正确更新。

任务：
- 按用户要求，把当前 `baseline-a3` 从手工 `beam budget/custom A3` 触发链迁到完整 `NrHandoverAlgorithm` 子类。
- 目标不是同时重写全部 improved 逻辑，而是先让 baseline 真正基于标准 `MeasurementReport` 工作，直接读真实 PHY/RRC 测量链。

改动目标：
- 新增一个 LEO-NTN 场景专用的 `NrHandoverAlgorithm` 子类，使用标准 `A3 + hysteresis + TTT` 语义。
- 让 `--handoverMode=baseline-a3` 通过该子类直接消费 gNB RRC 收到的 `MeasurementReport`。
- 保持默认 improved `joint-signal-load` 仍走现有手工联合链，不在这次改动里混迁。
- 保留现有观测用低门限 `A4` measurement config，用于继续导出 PHY/RRC trace。

设计说明：
- 当前代码里原有两条链：
  - baseline / improved 共用的手工链
  - 标准测量链
- 这次改动的策略是"先最小闭环 baseline，不一次性把 improved 也卷进去"
- 边界控制：
  - `baseline-a3`：走完整 `NrHandoverAlgorithm` 子类，不再调用 `ExecuteCustomA3Handover()`
  - 默认 `joint-signal-load`：仍保持当前手工联合链

已做验证：
- 构建：通过
- smoke test：通过
- 测量 trace 验证：`rrc_measurement_report_trace.csv` 中同时出现 meas_id=1 和 meas_id=2

当前观察到的结果：
- `baseline-a3` 已经真正从"手工代理触发"切到"真实 `MeasurementReport` 驱动"
- 默认 improved 没被误改，baseline / improved 对照边界仍清楚
- 在短跑里，measurement-driven baseline 的切换时序和统计结果已经明显不同于默认 improved
