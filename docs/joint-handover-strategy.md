# 联合信号-负载切换策略设计

## 文档定位
- 本文档用于定义后续 improved 策略，不改变当前 baseline 的正式定义
- 本文档服务于算法设计、变量命名、数学表达和后续代码接入
- baseline 的正式边界仍以 `scratch/baseline-definition.md` 为准
- 当前建议先完成 `shadowing / Rician` 与自定义 `beam budget` 判决链关系的澄清，再正式推进本策略

## 1. 算法定义
- 后续 improved 策略保持当前 baseline 场景口径不变，仍在 `2x4`、`25 UE`、`seven-cell` 条件下做对照
- baseline 继续保持“`RSRP + hysteresis + TTT + 基本可见性/beam lock`”语义，不把负载项回写到 baseline
- improved 策略的核心思路是：先筛掉明显无效候选星，再对剩余候选星计算“信号质量 + 剩余可见时间 + 负载”的联合效用，选择综合得分最高的目标星
- improved 策略仍复用当前自定义切换执行路径，不优先改成 `NrHandoverAlgorithm` 子类；当前更合适的接入点仍是 `UpdateConstellation()` 周期观测链

## 2. 建议变量名

### 2.1 当前代码里可直接复用的量
- `rsrpDbm_i(t)`：候选星 `i` 对当前 `UE` 的信号质量代理，对应 `observation.beamBudgets[i].rsrpDbm`
- `visible_i(t)`：候选星 `i` 是否高于最小仰角门限，对应 `observation.states[i].visible`
- `beamLocked_i(t)`：候选星 `i` 当前波束是否有效锁定，对应 `observation.beamBudgets[i].beamLocked`
- `loadScore_i(t)`：候选星 `i` 的归一化负载分数，对应 `g_satellites[i].loadScore`
- `admissionAllowed_i(t)`：候选星 `i` 是否允许继续接纳切入，对应 `g_satellites[i].admissionAllowed`

### 2.2 建议为 improved 新增的量
- `remainingVisibilitySeconds_i(t)`：候选星 `i` 对当前 `UE` 的剩余可见时间
- `signalScore_i(t)`：信号归一化效用
- `visibilityScore_i(t)`：剩余可见时间归一化效用
- `loadUtility_i(t)`：负载归一化效用，值越大越好
- `jointUtility_i(t)`：联合总效用
- `bestJointCandidateIdx`：综合得分最高的候选星索引
- `jointQualified`：综合候选星是否满足最终切换触发条件

## 3. 数学式

### 3.1 候选集合
\[
\mathcal{N}_{u}(t)=\{i \mid i \neq s,\ visible_i(t)=1,\ beamLocked_i(t)=1,\ rsrpDbm_i(t)\ \text{finite}\}
\]

其中 `s` 为当前服务星索引。若后续需要更强门控，可在 improved 中附加：

\[
admissionAllowed_i(t)=1
\]

### 3.2 剩余可见时间
\[
\tau_i(t)=\inf \{\Delta t \ge 0 \mid elevation_i(t+\Delta t)<\theta_{\min}\}
\]

其中 `\theta_{\min}` 对应当前 `minElevationDeg`。当前仓库中更容易落地的实现方式是：在未来短时域内按固定步长外推轨道状态，找到第一次低于仰角门限的时刻，近似得到 `remainingVisibilitySeconds_i(t)=\tau_i(t)`。

### 3.3 信号效用
\[
signalScore_i(t)=\frac{rsrpDbm_i(t)-\min_{j \in \mathcal{N}_{u}(t)} rsrpDbm_j(t)}
{\max_{j \in \mathcal{N}_{u}(t)} rsrpDbm_j(t)-\min_{j \in \mathcal{N}_{u}(t)} rsrpDbm_j(t)+\varepsilon}
\]

### 3.4 可见时间效用
\[
visibilityScore_i(t)=\frac{\tau_i(t)-\min_{j \in \mathcal{N}_{u}(t)} \tau_j(t)}
{\max_{j \in \mathcal{N}_{u}(t)} \tau_j(t)-\min_{j \in \mathcal{N}_{u}(t)} \tau_j(t)+\varepsilon}
\]

### 3.5 负载效用
\[
loadUtility_i(t)=1-loadScore_i(t)
\]

说明：
- 当前 `loadScore` 已经是 `[0,1]` 归一化量，因此第一版 improved 可直接用 `1-loadScore`
- 如果后续切到 `PRB` 占用率或更细粒度负载指标，再改成统一的 `Min-Max` 归一化即可

### 3.6 联合效用函数
\[
jointUtility_i(t)=w_{sig}\cdot signalScore_i(t)+w_{vis}\cdot visibilityScore_i(t)+w_{load}\cdot loadUtility_i(t)
\]

其中：

\[
w_{sig}+w_{vis}+w_{load}=1,\quad w_{sig}, w_{vis}, w_{load}\ge 0
\]

### 3.7 目标选择
\[
i^{*}=\arg\max_{i \in \mathcal{N}_{u}(t)} jointUtility_i(t)
\]

### 3.8 触发边界
建议保留当前 baseline 的触发边界，不直接改成“只看综合分数就切”，即：

\[
jointQualified=
\left(i^{*}\ \text{valid}\right)\land
\left(
\neg servingUsable\ \lor\ rsrpDbm_{i^{*}}(t)>rsrpDbm_{s}(t)+hoHysteresisDb
\right)
\]

如需更强抑制即将离场卫星，可增加剩余可见时间硬门限：

\[
\tau_{i^{*}}(t)\ge T_{vis,min}
\]

TTT 语义继续沿用当前 `manualHoCandidateSince` 机制，只是把原来的“最强邻星”替换为“联合效用最优候选星”。

## 4. 与当前代码的映射
- 候选观测入口仍是 `BuildUeObservationSnapshot()`；`RSRP`、`visible`、`beamLocked` 已经在这里齐备
- 基础负载量继续复用 `UpdateSatelliteLoadStats()` 输出的 `loadScore`
- improved 更合适的第一落点是在 `UpdateConstellation()` 中替换 `bestNeighbourIdx / bestNeighbourRsrp` 的“纯最强邻星”选择逻辑
- 第一版实现建议只新增“联合评分目标选择”函数，不改 `TriggerHandover` 执行链，不改 baseline 默认参数，不改 baseline 文档定义
- 文档表述上应明确：当前 `rsrpDbm` 是几何 `beam budget`/`RSRP` 代理，不是完整真实 `MeasurementReport`

## 5. 当前实现建议
- 第一版先使用 `loadScore` 作为负载代理，不直接引入 `PRB` 占用率
- 第一版先实现“联合目标选择”，不同时改场景口径、baseline 参数和日志体系
- 第一版优先观察：
  - handover success rate
  - handover delay
  - throughput continuity
  - ping-pong count
  - load balance
