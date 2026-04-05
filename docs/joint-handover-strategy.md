# 联合信号-负载切换策略设计

## 文档定位
- 本文档定义当前 `improved` 的设计边界，不改变 baseline 的正式定义
- baseline 的正式边界仍以 `scratch/baseline-definition.md` 为准
- improved 必须复用当前真实 `PHY/RRC MeasurementReport` 入口，不能回退到旧的几何 handover 代理链

## 1. 算法定义
- 当前 improved 保持 baseline 场景口径不变，仍在 `2x4`、`25 UE`、`seven-cell` 条件下做对照
- baseline 保持“`MeasurementReport + RSRP + hysteresis + TTT`”语义，不把负载项回写到 baseline
- improved 的核心思路是：对同一批 A3 上报候选计算“信号质量 + 负载效用”的联合得分，选择综合分数最高的目标星
- improved 不再通过 `UpdateConstellation()` 中的几何观测链触发切换，而是在测量回调中完成目标选择

## 2. 变量映射

### 2.1 当前代码里可直接复用的量
- `servingRsrp(t)`：当前服务小区的测量 `RSRP`
- `candidateRsrp_i(t)`：候选邻区 `i` 在 `MeasurementReport` 中上报的 `RSRP`
- `loadScore_i(t)`：候选星 `i` 的归一化负载分数，对应 `g_satellites[i].loadScore`
- `admissionAllowed_i(t)`：候选星 `i` 的接纳状态观测量，对应 `g_satellites[i].admissionAllowed`

### 2.2 improved 目标选择量
- `signalScore_i(t)`：信号归一化效用
- `loadUtility_i(t)`：负载归一化效用，值越大越好
- `jointUtility_i(t)`：联合总效用
- `bestJointCandidateIdx`：综合得分最高的候选星索引
- `jointQualified`：综合候选星是否满足最终切换触发条件

## 3. 数学式

### 3.1 候选集合
\[
\mathcal{N}_{u}(t)=\{i \mid i \neq s,\ candidateRsrp_i(t)\ \text{finite}\}
\]

其中 `s` 为当前服务小区。候选集合只来自标准 `MeasurementReport`，不再额外拼接几何代理候选。

### 3.2 信号效用
\[
signalScore_i(t)=\frac{candidateRsrp_i(t)-\min_{j \in \mathcal{N}_{u}(t)} candidateRsrp_j(t)}
{\max_{j \in \mathcal{N}_{u}(t)} candidateRsrp_j(t)-\min_{j \in \mathcal{N}_{u}(t)} candidateRsrp_j(t)+\varepsilon}
\]

### 3.3 负载效用
\[
loadUtility_i(t)=1-loadScore_i(t)
\]

说明：
- 当前 `loadScore` 已经是 `[0,1]` 归一化量，因此第一版 improved 可直接用 `1-loadScore`
- `admissionAllowed` 当前保留为观测量，不作为 improved 的硬前置筛选条件

### 3.4 联合效用函数
\[
jointUtility_i(t)=w_{sig}\cdot signalScore_i(t)+w_{load}\cdot loadUtility_i(t)
\]

其中：

\[
w_{sig}+w_{load}>0,\quad w_{sig}, w_{load}\ge 0
\]

### 3.5 目标选择
\[
i^{*}=\arg\max_{i \in \mathcal{N}_{u}(t)} jointUtility_i(t)
\]

### 3.6 触发边界
建议保留当前 baseline 的 A3 触发边界，不直接改成“只看综合分数就切”，即：

\[
jointQualified=
\left(i^{*}\ \text{valid}\right)\land
\left(
\neg servingUsable\ \lor\ candidateRsrp_{i^{*}}(t)>servingRsrp(t)+hoHysteresisDb
\right)
\]

TTT 由标准 A3 测量配置管理，不再使用旧的手工 `manualHoCandidateSince` 机制。

## 4. 与当前代码的映射
- 候选观测入口是 `NrLeoA3MeasurementHandoverAlgorithm` 暴露出的 `MeasurementReport` trace
- 基础负载量继续复用 `UpdateSatelliteLoadStats()` 输出的 `loadScore`
- improved 的落点是测量回调中的目标选择函数，而不是轨道周期更新函数
- 第一版实现建议只改变目标选择逻辑，不改变 baseline 默认参数，不改变 baseline 文档定义
- 文档表述必须明确：当前 baseline 与 improved 共享真实测量入口，而不是几何 `RSRP` 代理

## 5. 当前实现建议
- 第一版先使用 `loadScore` 作为负载代理，不直接引入 `PRB` 占用率
- 第一版先实现“联合目标选择”，不同时改场景口径、baseline 参数和日志体系
- 第一版优先观察：
  - handover success rate
  - handover delay
  - throughput continuity
  - ping-pong count
  - load balance
