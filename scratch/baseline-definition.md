# LEO-NTN Handover Baseline

## baseline 角色
- 当前 baseline 已经基本收口，可以作为后续改进算法的对照组与接入平台
- 它的任务不是追求最优，而是提供一个清楚、稳定、可复现的比较起点
- 当前 baseline 与后续改进算法的边界是：
  - baseline：只基于信号质量做切换判决
  - improved：在同一场景下引入负载、剩余可见时间或联合代价项

## baseline 定义
当前建议将 baseline 定义为：

> 在固定 `2x4` 双轨、`25 UE`、中等时长仿真场景下，采用基于标准 `PHY/RRC MeasurementReport` 的传统 `A3` 风格切换基线；UE 使用 `seven-cell`（中心 1 小区 + 周围 6 小区）二维部署，baseline 决策仅依赖 `RSRP`、`hysteresis` 与 `TTT`，不引入负载感知、预测优化或学习型决策。

这一定义强调三件事：
- 场景边界固定
- 决策边界清楚
- 后续所有改进都必须相对于它做对照

## 当前默认场景口径
场景：
- `gNbNum = 8`
- `orbitPlaneCount = 2`
- `ueNum = 25`
- `ueLayoutType = seven-cell`
- `hexCellRadiusKm = 20`
- `ueCenterSpacingMeters = 6000`
- `ueRingPointOffsetMeters = 5000`
- `satAltitudeMeters = 600000`
- `orbitInclinationDeg = 53`
- `interPlaneRaanSpacingDeg`（轨道面 RAAN 间隔）=`-1`
- `interPlaneTimeOffsetSeconds`（轨道面时间偏移）=`3.0`
- `alignmentReferenceTimeSeconds`（对齐参考时刻）=`15`
- `overpassGapSeconds`（同轨过境间隔）=`3`

运行与切换：
- `simTime`（仿真时长）=`40 s`
- `appStartTime = 1 s`
- `updateIntervalMs`（主循环更新周期）=`100`
- `minElevationDeg`（最小仰角）=`10`
- `lambda`（业务流强度）=`250 pkt/s/UE`
- `maxSupportedUesPerSatellite = 5`
- `bandwidth = 40 MHz`
- `hoHysteresisDb`（切换迟滞门限）=`2.0 dB`
- `hoTttMs`（切换触发时间）=`160 ms`
- `measurementReportIntervalMs = 120 ms`
- `measurementMaxReportCells = 8`
- `handoverMode = baseline`
- `improvedSignalWeight = 0.7`
- `improvedLoadWeight = 0.3`
- `improvedVisibilityWeight = 0.2`
- `improvedMinLoadScoreDelta = 0.2`
- `improvedMaxSignalGapDb = 3.0 dB`
- `improvedMinStableLeadTimeSeconds = 0.12 s`
- `improvedMinVisibilitySeconds = 1.0 s`
- `improvedVisibilityHorizonSeconds = 8.0 s`
- `improvedVisibilityPredictionStepSeconds = 0.5 s`
- `improvedMinJointScoreMargin = 0.03`
- `pingPongWindowSeconds`（将 `A->B->A` 记为 `ping-pong` 的时间窗口）=`1.5 s`
- `useWgs84HexGrid = true`
- `forceRlcAmForEpc = false`
- `disableUeIpv4Forwarding = true`

说明：
- 这套参数代表当前工作区的默认 baseline 口径；最近已发布稳定节点为 `research-v4.2`
- 当前先不通过继续扩星来放大现象，而是优先通过七小区 `UE` 占位来增强跨小区竞争与空间代表性
- 当前 `UE` 生成实现已收口为“局部东-北平面偏移模板 + 统一 `WGS84/ECEF` 转换”的两阶段写法
- 当前默认布局为：中心小区 `3x3` 密集簇 `9 UE`，外围 `6` 个相邻小区共 `16 UE`
- 当前 PHY 信道保留 `ThreeGpp` 路径并开启 `ShadowingEnabled`，baseline 与 improved 都直接消费标准 `MeasurementReport`
- 当前已移除原来的几何 `beam budget/custom A3` handover 判决链；几何计算只保留给轨道推进、地面锚点与初始接入
- 当前默认关闭 `UE IPv4 forwarding`，避免异常包被 UE 重新走上行 `NAS/TFT` 分类路径
- 当前保留 `forceRlcAmForEpc` 作为可选稳定性开关，但默认不改变 helper 的 `RLC` 选择

## 当前切换语义
- 当前 baseline 仍属于传统 `A3` 风格切换
- 判决主线为：标准 `MeasurementReport` 上报的 `RSRP` 比较、`hysteresis` 与 `TTT`
- `handoverMode = baseline` 时，直接在 A3 上报候选中选择最强邻区
- `handoverMode = improved` 时，仍使用同一批真实测量候选，但在目标选择时联合考虑 `signal`、`remainingVisibility` 与 `loadScore`，并在源站接近拥塞时进一步偏向轻载候选
- improved 允许先用最小剩余可见时间做硬门控，再在保留下来的候选中做联合评分；只有当同一目标的联合领先状态持续至少 `improvedMinStableLeadTimeSeconds`，且最佳候选的联合分数相对当前服务星至少高出 `improvedMinJointScoreMargin` 时，才触发切换
- 只要决策依据不包含负载权重，这条路径仍属于 baseline

## baseline 验证清单
建议把验证分成三层，而不是只看“能不能跑”。

### 1. 运行路径验证
目标：确认默认 baseline 主流程稳定可运行。

最少检查：
- 程序能启动、推进并正常结束
- 无断言、fatal 或异常退出
- `scratch/results/` 能生成基础输出
- 能看到 `Progress` 或最终研究摘要
- 若需检查失败切换、失败原因或未闭合事件，优先查看 `handover_event_trace.csv`

### 2. 研究行为验证
目标：确认当前 baseline 是否具备研究分析价值。

重点检查：
- 是否出现非零切换，而不是全程无切换
- 切换流程是否可正常闭环，以及切换次数与吞吐恢复是否可统计
- 若有失败，是否能在 `handover_event_trace.csv` 中区分 `noPreamble / maxRach / leaving / joining / unknown` 等失败原因
- 切换附近是否能观察吞吐扰动，并优先结合 `handover_dl_throughput_trace.csv` 与 `handover_event_trace.csv` 对齐 `HO Start / HO Success`
- 外围小区 UE 是否出现频繁切换或潜在 `ping-pong`
- 最终摘要中的自动 `ping-pong` 计数是否为非零，且与事件 trace 中的短时回切现象一致
- 星间 `attachedUeCount`、`offeredPacketRate`、`loadScore` 是否出现可观察的不均衡
- 若启用轨迹可视化，`sat_anchor_trace.csv` 是否能正确反映两轨波束锚点的小区变化路径

### 3. 对照价值验证
目标：确认它是否能作为后续联合策略的可信对照组。

最终判定标准：
- 场景边界不变：仍是 `2x4` 双轨、`25 UE`、`seven-cell`
- 决策边界不变：baseline 不使用 `loadScore`
- 现象边界清楚：能回答频繁切换、`ping-pong`、吞吐连续性和负载失衡是否存在

## 建议记录模板
每次 baseline 验证至少记录：
- `commit id`
- 运行命令
- 是否保持默认参数
- 总切换次数
- 是否观察到 `ping-pong`
- 自动 `ping-pong` 计数及其判定窗口
- 是否观察到明显负载失衡
- 对吞吐连续性的简短结论
- 一句话说明：这次结果是否足以作为后续改进算法对照组

## 负载接口边界
当前 baseline 不使用负载做决策，但运行时已经保留最小接口：
- 观测量：`attachedUeCount`、`offeredPacketRate`
- 决策辅助量：`loadScore`、`admissionAllowed`
- 当前 `loadScore` 采用更平滑的压力近似，避免在每星 `5 UE` 容量口径下过早饱和，便于区分 `2/3/4/5 UE` 的负载差异
- 扩展方向：后续可继续加入 `estimatedPrbUsage`、`loadState` 等量

当前实现状态：
1. `SatelliteRuntime` 已包含基础负载字段
2. 周期更新中已经会计算每星基础负载状态
3. 这些量在 `handoverMode = improved` 下已经进入目标选择
4. baseline 仍不使用 `loadScore` 或 `remainingVisibility` 做决策

## 下一步
- 用当前默认参数完成一轮 `seven-cell baseline` 验证
- 在同一 `MeasurementReport` 入口下，对比 `baseline` 与 `improved` 的目标选择差异
- 保持 baseline 与改进算法的对比边界清楚，避免同时改动场景口径、随机信道扰动和决策逻辑
