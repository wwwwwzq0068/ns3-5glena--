# LEO-NTN Handover Baseline Draft

## 当前判断
- 现在还不能说 baseline 已经设计完成。
- 当前已有一个可运行的 baseline 候选平台：
  - `2x4` 双轨、`6 UE`、持续业务流；
  - `RSRP + hysteresis + TTT` 的 A3 风格切换骨架；
  - 基本邻区可达性约束。
- 当前仍缺两件关键事：
  - 场景还未证明能稳定暴露频繁切换或 ping-pong；
  - 代码还没有真正的负载能力建模与统一负载接口。

## baseline 候选定义
当前建议把 baseline 定义为：

“在固定 `2x4` 双轨、`6 UE`、中等时长仿真场景下，采用仅基于信号质量的 A3 风格切换基线；决策只依赖 `RSRP` 比较、`hysteresis`、`TTT` 和邻区可达性约束，不引入负载感知、预测优化或学习型决策。”

说明：
- 研究语义上，它属于传统 A3 baseline。
- 工程实现上，当前多 UE 场景允许使用自定义执行器来复现 A3 触发语义。
- 只要决策依据仍然不包含负载权重，这条路径仍算 baseline，而不是改进算法。

## 默认参数草案
以下参数先作为当前 baseline 主配置候选：

- `simTime = 40s`
- `appStartTime = 1s`
- `gNbNum = 8`
- `orbitPlaneCount = 2`
- `ueNum = 6`
- `ueSpacingMeters = 10000`
- `satAltitudeMeters = 600000`
- `orbitInclinationDeg = 53`
- `interPlaneRaanSpacingDeg = 12`
- `interPlaneTimeOffsetSeconds = 2`
- `overpassGapSeconds = 4`
- `updateIntervalMs = 200`
- `minElevationDeg = 10`
- `lambda = 250 pkt/s/UE`
- `bandwidth = 40 MHz`
- `hoHysteresisDb = 0.2`
- `hoTttMs = 1200`
- `strictNrtGuard = true`
- `strictNrtMarginDb = hoHysteresisDb`
- `useWgs84HexGrid = true`

这套参数的定位是“当前主配置候选”，不是最优解。

## 负载接口草案
当前 baseline 不使用负载做决策，但后续代码应预留最小接口。

建议只先保留三类量：
- 观测量：`attachedUeCount`、`offeredPacketRate`、`estimatedPrbUsage`
- 能力参数：`maxSupportedUes`、`congestionThreshold`
- 决策输出：`loadScore`、`loadState`、`admissionAllowed`

建议实现顺序：
1. 先在 `SatelliteRuntime` 增加负载字段。
2. 再在周期更新中计算每星负载状态。
3. 最后在联合策略中把 `RSRP` 和 `loadScore` 组合。

## 下一步
当前最优先的是：
1. 明确 baseline 是否必须稳定呈现频繁切换，还是只需具备暴露该问题的潜力。
2. 冻结一套 baseline 主配置参数。
3. 给 `SatelliteRuntime` 预留最小负载字段，但先不改变切换行为。
