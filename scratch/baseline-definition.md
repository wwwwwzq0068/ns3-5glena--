# LEO-NTN Handover Baseline

## baseline 角色
- 当前 baseline 已经基本收口，可以作为后续改进算法的对照组与接入平台
- 它的任务不是追求最优，而是提供一个清楚、稳定、可复现的比较起点
- 当前 baseline 与后续改进算法的边界是：
  - baseline：只基于信号质量做切换判决
  - improved：在同一场景下引入负载或联合代价项

## baseline 定义
当前建议将 baseline 定义为：

> 在固定 `2x4` 双轨、`25 UE`、中等时长仿真场景下，采用仅基于信号质量的 `A3` 风格切换基线；UE 使用 `seven-cell`（中心 1 小区 + 周围 6 小区）二维部署，决策仅依赖 `RSRP`、`hysteresis`、`TTT` 以及基本的可见性/波束锁定约束，不引入严格邻区守卫、负载感知、预测优化或学习型决策。

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
- `interPlaneRaanSpacingDeg`（轨道面 RAAN 间隔）=`3`
- `interPlaneTimeOffsetSeconds`（轨道面时间偏移）=`0.3`
- `alignmentReferenceTimeSeconds`（对齐参考时刻）=`20`
- `overpassGapSeconds`（同轨过境间隔）=`2`

运行与切换：
- `simTime`（仿真时长）=`40 s`
- `appStartTime = 1 s`
- `updateIntervalMs`（主循环更新周期）=`100`
- `minElevationDeg`（最小仰角）=`10`
- `lambda`（业务流强度）=`1000 pkt/s/UE`
- `bandwidth = 40 MHz`
- `hoHysteresisDb`（切换迟滞门限）=`0.3 dB`
- `hoTttMs`（切换触发时间）=`100 ms`
- `strictNrtGuard = false`
- `strictNrtMarginDb = hoHysteresisDb`
- `useWgs84HexGrid = true`

说明：
- 这套参数代表当前 `3.2.0` 的默认 baseline 口径，不代表最优参数
- 当前先不通过继续扩星来放大现象，而是优先通过七小区 `UE` 占位来增强跨小区竞争与空间代表性
- 当前 `UE` 生成实现已收口为“局部东-北平面偏移模板 + 统一 `WGS84/ECEF` 转换”的两阶段写法
- 当前默认布局为：中心小区 `3x3` 密集簇 `9 UE`，外围 `6` 个相邻小区共 `16 UE`
- 当前 PHY 信道保留 `ThreeGpp` 路径并开启 `ShadowingEnabled`，但 baseline 判决仍以自定义几何 `beam budget/rsrpDbm` 为准

## 当前切换语义
- 当前 baseline 仍属于传统 `A3` 风格切换
- 判决主线为：`RSRP` 比较、`hysteresis`、`TTT`、基本可见性/波束锁定约束
- 当前多 UE 场景使用自定义执行器来复现 `A3` 触发语义
- 只要决策依据不包含负载权重，这条路径仍属于 baseline

说明：
- `strictNrtGuard`（严格邻区表守卫）继续保留在平台中，但不再作为 baseline 默认项
- 后续若启用 `strictNrtGuard`，应将其视为对 baseline 的增强机制，而不是 baseline 本体

## baseline 验证清单
建议把验证分成三层，而不是只看“能不能跑”。

### 1. 运行路径验证
目标：确认默认 baseline 主流程稳定可运行。

最少检查：
- 程序能启动、推进并正常结束
- 无断言、fatal 或异常退出
- `scratch/results/` 能生成基础输出
- 能看到 `Progress`、`HO` 或最终 summary

### 2. 研究行为验证
目标：确认当前 baseline 是否具备研究分析价值。

重点检查：
- 是否出现非零切换，而不是全程无切换
- 切换流程是否可正常闭环，成功率和执行时延是否可统计
- 切换附近是否能观察吞吐扰动
- 外围小区 UE 是否出现频繁切换或潜在 `ping-pong`
- 星间 `attachedUeCount`、`offeredPacketRate`、`loadScore` 是否出现可观察的不均衡

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
- 总切换次数、成功率、平均时延
- 是否观察到 `ping-pong`
- 是否观察到明显负载失衡
- 对吞吐连续性的简短结论
- 一句话说明：这次结果是否足以作为后续改进算法对照组

## 负载接口边界
当前 baseline 不使用负载做决策，但运行时已经保留最小接口：
- 观测量：`attachedUeCount`、`offeredPacketRate`
- 决策辅助量：`loadScore`、`admissionAllowed`
- 扩展方向：后续可继续加入 `estimatedPrbUsage`、`loadState` 等量

当前实现状态：
1. `SatelliteRuntime` 已包含基础负载字段
2. 周期更新中已经会计算每星基础负载状态
3. 这些量当前只用于观测和 trace 输出
4. 下一步才是在改进算法中把 `RSRP` 与 `loadScore` 组合

## 下一步
- 用当前默认参数完成一轮 `seven-cell baseline` 验证
- 保持 `A3` 触发语义不变，先独立评估 `shadowing / Rician` 扰动是否以及如何接入自定义判决链
- 保持 baseline 与改进算法的对比边界清楚，避免同时改动场景口径、随机信道扰动和决策逻辑
