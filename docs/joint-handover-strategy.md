# 联合信号-可见性-负载切换策略设计

## 文档定位
- 本文档定义当前 `improved` 的设计边界，不改变 baseline 的正式定义
- baseline 的正式边界仍以 `scratch/baseline-definition.md` 为准
- improved 必须复用当前真实 `PHY/RRC MeasurementReport` 入口，不能回退到旧的几何 handover 代理链

## 1. 算法定义
- 当前 improved 保持 baseline 场景口径不变，仍在 `2x4`、`25 UE`、`seven-cell` 条件下做对照
- baseline 保持"`MeasurementReport + RSRP + hysteresis + TTT`"语义，不把负载项回写到 baseline
- improved 的核心思路是：对同一批 A3 上报候选先做门控过滤，再计算"信号质量 + 可见性效用 + 负载效用"的联合得分，选择综合分数最高的目标星；当源站负载更高时，进一步增强负载导向，优先把 UE 送往更轻载的候选星
- improved 不再通过 `UpdateConstellation()` 中的几何观测链触发切换，而是在测量回调中完成目标选择
- 可见性因子只作为 improved 目标选择的辅助量，不回退到旧的几何触发链

## 2. 变量映射

### 2.1 当前代码里可直接复用的量
- `servingRsrp(t)`：当前服务小区的测量 `RSRP`
- `candidateRsrp_i(t)`：候选邻区 `i` 在 `MeasurementReport` 中上报的 `RSRP`
- `loadScore_i(t)`：候选星 `i` 的归一化负载分数，对应 `g_satellites[i].loadScore`；当前实现用“线性容量比 + 平滑饱和”的组合避免过早打满
- `admissionAllowed_i(t)`：候选星 `i` 的接纳状态观测量，对应 `g_satellites[i].admissionAllowed`
- `remainingVisibility_i(t)`：候选星 `i` 对当前 UE 的剩余可见时间，按当前轨道状态向前预测到其低于最小仰角门限的时刻

### 2.2 improved 目标选择量
- `signalScore_i(t)`：信号归一化效用
- `visibilityScore_i(t)`：可见性归一化效用，值越大表示还能稳定服务更久
- `loadUtility_i(t)`：负载归一化效用，值越大越好
- `jointUtility_i(t)`：联合总效用
- `bestJointCandidateIdx`：综合得分最高的候选星索引

### 2.3 门控相关量
- `lastSuccessfulHoTimeSeconds`：UE 上次切换成功的时刻
- `lastSuccessfulHoSourceCell`：UE 上次切换的源小区 ID
- `improvedReturnGuardSeconds`：短时回切保护窗口长度
- `improvedMaxSignalGapDb`：允许进入联合评分的最大信号差距
- `improvedMinLoadScoreDelta`：触发负载覆写所需的最小负载优势
- `improvedMinVisibilitySeconds`：允许候选继续参与目标选择的最小剩余可见时间
- `sourceLoadPressure`：源站负载压力的归一化量，当前实现会据此动态增强负载导向

## 3. 数学式

### 3.1 候选集合
\[
\mathcal{N}_{u}(t)=\{i \mid i \neq s,\ candidateRsrp_i(t)\ \text{finite}\}
\]

其中 `s` 为当前服务小区。候选集合只来自标准 `MeasurementReport`，不再额外拼接几何代理候选。

### 3.2 门控过滤

#### 3.2.1 过载候选过滤
\[
\mathcal{N}_{admit}(t) = \{i \in \mathcal{N}_{u}(t) \mid admissionAllowed_i(t) = true\}
\]

若 $\mathcal{N}_{admit}(t) = \emptyset$，则回退到原候选集：
\[
\mathcal{N}_{filtered}^{(1)}(t) = \mathcal{N}_{admit}(t) \text{ if } \mathcal{N}_{admit}(t) \neq \emptyset \text{, else } \mathcal{N}_{u}(t)
\]

#### 3.2.2 短时回切保护
定义保护窗口内标记：
\[
inReturnGuard = \left( t - lastSuccessfulHoTimeSeconds \leq T_{guard} \right) \land \left( lastSuccessfulHoSourceCell \neq 0 \right)
\]

若处于保护窗口，屏蔽上一跳来源小区：
\[
\mathcal{N}_{filtered}^{(2)}(t) =
\begin{cases}
\{i \in \mathcal{N}_{filtered}^{(1)}(t) \mid cellId_i \neq lastSuccessfulHoSourceCell\} & \text{if } inReturnGuard \land \exists j \neq lastSuccessfulHoSourceCell \\
\mathcal{N}_{filtered}^{(1)}(t) & \text{otherwise}
\end{cases}
\]

#### 3.2.3 负载覆写门槛
找出当前候选集中最强信号候选：
\[
i_{best} = \arg\max_{i \in \mathcal{N}_{filtered}^{(2)}(t)} candidateRsrp_i(t)
\]

定义候选是否有资格进入联合评分：
\[
qualified_i =
\begin{cases}
true & \text{if } i = i_{best} \\
true & \text{if } candidateRsrp_{i_{best}}(t) - candidateRsrp_i(t) \leq \Delta_{signal} \\
true & \text{if } loadScore_{i_{best}}(t) - loadScore_i(t) \geq \Delta_{load} \\
false & \text{otherwise}
\end{cases}
\]

其中：
- $\Delta_{signal} = improvedMaxSignalGapDb$（默认 3.0 dB）
- $\Delta_{load} = improvedMinLoadScoreDelta$（默认 0.2）

当前实现还会根据源站负载压力动态调整这两个门限：源站越忙，允许进入联合评分的候选越多，负载优势门槛越容易被触发。

有资格进入联合评分的候选集：
\[
\mathcal{N}_{qualified}(t) = \{i \in \mathcal{N}_{filtered}^{(2)}(t) \mid qualified_i = true\}
\]

若 $\mathcal{N}_{qualified}(t) = \emptyset$，则回退到最强信号候选：
\[
\mathcal{N}_{final}(t) = \mathcal{N}_{qualified}(t) \text{ if } \mathcal{N}_{qualified}(t) \neq \emptyset \text{, else } \{i_{best}\}
\]

#### 3.2.4 最小剩余可见时间门控
\[
\mathcal{N}_{visible}(t)=\{i \in \mathcal{N}_{final}(t) \mid remainingVisibility_i(t)\ge T_{minVis}\}
\]

若 $\mathcal{N}_{visible}(t)=\emptyset$，则回退到上一层候选集：
\[
\mathcal{N}_{score}(t)=\mathcal{N}_{visible}(t) \text{ if } \mathcal{N}_{visible}(t)\neq\emptyset \text{, else } \mathcal{N}_{final}(t)
\]

其中 `T_minVis = improvedMinVisibilitySeconds`。

### 3.3 信号效用
\[
signalScore_i(t)=\frac{candidateRsrp_i(t)-\min_{j \in \mathcal{N}_{score}(t)} candidateRsrp_j(t)}
{\max_{j \in \mathcal{N}_{score}(t)} candidateRsrp_j(t)-\min_{j \in \mathcal{N}_{score}(t)} candidateRsrp_j(t)+\varepsilon}
\]

### 3.4 可见性效用
\[
visibilityScore_i(t)=\min\left(1,\frac{remainingVisibility_i(t)}{T_{vis}}\right)
\]

其中 `T_vis = improvedVisibilityHorizonSeconds`。

### 3.5 负载效用
\[
loadUtility_i(t)=1-loadScore_i(t)
\]

说明：
- 当前 `loadScore` 已经是 `[0,1]` 归一化量，因此直接用 `1-loadScore`
- `loadUtility` 越大表示负载越轻
- 当前实现还会额外引入源站负载压力，让接近拥塞的服务星更积极地把 UE 送往轻载候选

### 3.6 联合效用函数
\[
jointUtility_i(t)=w_{sig}\cdot signalScore_i(t)+w_{vis}\cdot visibilityScore_i(t)+w_{load}\cdot loadUtility_i(t)
\]

其中：

\[
w_{sig}+w_{vis}+w_{load}>0,\quad w_{sig}, w_{vis}, w_{load}\ge 0
\]

### 3.7 目标选择
\[
i^{*}=\arg\max_{i \in \mathcal{N}_{score}(t)} jointUtility_i(t)
\]

若存在多个候选联合效用相等，优先选信号更强者：
\[
i^{*}=\arg\max_{i \in \mathcal{N}_{score}(t)} \left( jointUtility_i(t), candidateRsrp_i(t) \right)
\]

### 3.8 触发边界
保留当前 baseline 的 A3 触发边界，不直接改成"只看综合分数就切"，即：

\[
jointQualified=
\left(i^{*}\ \text{valid}\right)\land
\left(
\neg servingUsable\ \lor\ candidateRsrp_{i^{*}}(t)>servingRsrp(t)+hoHysteresisDb
\right)
\]

TTT 由标准 A3 测量配置管理，不再使用旧的手工 `manualHoCandidateSince` 机制。

## 4. 门控设计意图

### 4.1 过载候选过滤
- **目的**：避免将 UE 切换到已过载的卫星
- **回退**：若所有候选都过载，仍允许从中选择，避免无目标可切
- **参数**：`admissionAllowed` 由 `loadScore < loadCongestionThreshold` 决定

### 4.2 短时回切保护
- **目的**：抑制 `A -> B -> A` 形式的 ping-pong
- **回退**：若保护窗口内只有上一跳来源可选，仍允许回切
- **参数**：`improvedReturnGuardSeconds` 默认 1.5s

### 4.3 负载覆写门槛
- **目的**：防止"信号明显更差但仅因微小负载差异被选中"的非预期切换
- **条件**：只有满足以下之一才允许进入联合评分：
  - 是最强信号候选本身
  - 与最强信号候选的信号差距不超过 `improvedMaxSignalGapDb`
  - 相对最强信号候选具有至少 `improvedMinLoadScoreDelta` 的负载优势
- **参数**：
  - `improvedMaxSignalGapDb` 默认 3.0 dB
  - `improvedMinLoadScoreDelta` 默认 0.2

### 4.4 最小剩余可见时间门控
- **目的**：避免把 UE 切到“马上就要离开可视范围”的候选卫星
- **回退**：若所有候选剩余可见时间都不足，则保留上一层候选集，避免无目标可切
- **参数**：
  - `improvedMinVisibilitySeconds` 默认 1.0 s
  - `improvedVisibilityHorizonSeconds` 默认 8.0 s
  - `improvedVisibilityPredictionStepSeconds` 默认 0.5 s
- **补充**：当前实现会根据源站负载压力动态增强负载项，避免低负载时过度干预，高负载时响应不够快

## 5. 与当前代码的映射
- 候选观测入口是 `NrLeoA3MeasurementHandoverAlgorithm` 暴露出的 `MeasurementReport` trace
- 基础负载量继续复用 `UpdateSatelliteLoadStats()` 输出的 `loadScore`
- 可见性量通过当前轨道参数和 UE 地面点在线预测得到，不从旧几何 handover 链回收触发语义
- improved 的落点是测量回调中的目标选择函数 `SelectMeasurementDrivenTarget()`
- 门控逻辑在联合评分之前执行，确保只有有意义的候选才进入评分
- 文档表述必须明确：当前 baseline 与 improved 共享真实测量入口，而不是几何 `RSRP` 代理

## 6. 参数汇总

| 参数名 | 默认值 | 说明 |
|--------|--------|------|
| `improvedSignalWeight` | 0.7 | 联合评分中信号权重 |
| `improvedLoadWeight` | 0.3 | 联合评分中负载权重 |
| `improvedVisibilityWeight` | 0.2 | 联合评分中可见性权重 |
| `improvedMinLoadScoreDelta` | 0.2 | 触发负载覆写所需的最小负载优势 |
| `improvedMaxSignalGapDb` | 3.0 dB | 允许进入联合评分的最大信号差距 |
| `improvedReturnGuardSeconds` | 1.5 s | 短时回切保护窗口 |
| `improvedMinVisibilitySeconds` | 1.0 s | 候选切入所需的最小剩余可见时间 |
| `improvedVisibilityHorizonSeconds` | 8.0 s | 可见性评分归一化时间窗 |
| `improvedVisibilityPredictionStepSeconds` | 0.5 s | 可见性向前预测步长 |

## 7. 当前实现建议
- 第一版先使用 `loadScore` 作为负载代理，不直接引入 `PRB` 占用率
- 第一版先实现"带门控的联合目标选择"，不同时改场景口径、baseline 参数和日志体系
- 第一版先用剩余可见时间表达候选的稳定服务能力，不把它单独改造成新的触发主链
- 第一版优先观察：
  - handover success rate
  - handover delay
  - throughput continuity
  - ping-pong count
  - load balance
