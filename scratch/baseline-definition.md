# LEO-NTN Handover Baseline

## baseline 角色
- 当前 baseline 已经收口并固定为 `B00`，可以作为后续改进算法的对照组与接入平台
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
- `anchorGridHexRadiusKm = 20`
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
- `improvedRsrqWeight = 0.3`
- `improvedLoadWeight = 0.3`
- `improvedVisibilityWeight = 0.2`
- `improvedMinLoadScoreDelta = 0.2`
- `improvedMaxSignalGapDb = 3.0 dB`
- `improvedMinStableLeadTimeSeconds = 0.12 s`
- `improvedMinVisibilitySeconds = 1.0 s`
- `improvedVisibilityHorizonSeconds = 8.0 s`
- `improvedVisibilityPredictionStepSeconds = 0.5 s`
- `improvedMinJointScoreMargin = 0.03`
- `improvedMinCandidateRsrpDbm = -110 dBm`
- `improvedMinCandidateRsrqDb = -17 dB`
- `improvedServingWeakRsrpDbm = -108 dBm`
- `improvedServingWeakRsrqDb = -15 dB`
- `improvedEnableCrossLayerPhyAssist = false`
- `improvedCrossLayerPhyAlpha = 0.02`
- `improvedCrossLayerTblerThreshold = 0.48`
- `improvedCrossLayerSinrThresholdDb = -5 dB`
- `improvedCrossLayerMinSamples = 50`
- `pingPongWindowSeconds`（将 `A->B->A` 记为 `ping-pong` 的时间窗口）=`1.5 s`
- `shadowingEnabled = true`
- `gnbAntennaRows/Columns = 8x8`
- `ueAntennaRows/Columns = 1x2`
- `gnbAntennaElement = b00-custom`（v4.3 新默认；可选：`isotropic`, `three-gpp`）
- `ueAntennaElement = three-gpp`（v4.3 新默认；可选：`isotropic`, `b00-custom`）
- `b00MaxGainDb = 20.0`（b00-custom 阵元峰值增益）
- `b00BeamwidthDeg = 15.0`（b00-custom 阵元波束宽度）
- `b00MaxAttenuationDb = 30.0`（b00-custom 旁瓣衰减上限）
- `beamformingMode = ideal-direct-path`
- `beamformingPeriodicityMs = 100`
- `realisticBfTriggerEvent = srs-count`
- `realisticBfUpdatePeriodicity = 3`
- `realisticBfUpdateDelayMs = 0`
- `useWgs84HexGrid = true`
- `anchorGridSwitchGuardMeters = 0`
- `anchorGridHysteresisSeconds = 0`
- `forceRlcAmForEpc = false`
- `disableUeIpv4Forwarding = true`
- `carrierReuseMode = reuse1`  (default, PHY diagnosis: reuse2-plane, reuse4)
- `carrierFrequencySpacingHz = 60e6`
- `sameFrequencyHandoverOnly = true`
- `printCarrierPlan = true`
- `interFrequencyHandoverEnabled = false` (Phase 2: allow cross-carrier-group HO)
- `printInterFrequencyEvents = true` (Phase 2)

说明：
- 这套参数代表当前工作区的默认 baseline 口径；最近已发布稳定节点为 `research-v4.2`
- 当前后续一段时间不再通过继续扩星来放大现象，而是固定当前 `2x4` 双轨、`25 UE`、`seven-cell` 场景
- 当前 `UE` 生成实现已收口为“局部东-北平面偏移模板 + 统一 `WGS84/ECEF` 转换”的两阶段写法
- 当前默认布局为：中心小区 `3x3` 密集簇 `9 UE`，外围 `6` 个相邻小区共 `16 UE`
- 当前默认仍保持 `anchorGridHexRadiusKm = hexCellRadiusKm`，因此 baseline 口径下地面锚点网格与 `UE` 七小区布局尺度一致
- 若需单独研究“波束中心跳格子”，当前实现允许在不改 `UE` 七小区布局的前提下，把锚点网格进一步细化，并给锚点切换增加距离优势与持续领先门控；这类设置应视为可选实验参数，不替代默认 baseline 定义
- 当前 PHY 信道保留 `ThreeGpp` 路径并开启 `ShadowingEnabled`，baseline 与 improved 都直接消费标准 `MeasurementReport`
- v4.3 起，真实 NR PHY 默认天线改为 `gNB b00-custom + UE three-gpp`，阵列规模 `gNB 8x8`、`UE 1x2`，波束方法 `ideal-direct-path`
- 几何参数 `beamMaxGainDbi/theta3dBDeg/sideLobeAttenuationDb` 主要用于几何链路预算与观测口径，不代表 v4.3 PHY 默认天线的真实阵元参数
- 当前所有卫星默认共享同一个 `2 GHz / 40 MHz / 1 CC` operation band，因此若需要验证高 `PHY DL TB error rate` 是否主要来自同频干扰，可将载波正交化视为诊断性实验；Phase 2 已补上跨频候选生成与触发尝试，但真实执行仍受当前 NR Ideal RRC 栈限制（不支持 RLF），尚未形成稳定可运行的 inter-frequency HO 方案
- 当前代码已额外开放 `gnbAntennaElement`、`ueAntennaElement` 与 `beamformingMode` 作为诊断参数，可在不改变 `B00` 场景定义的前提下，单独验证更真实阵列单元和 beamforming 对 `PHY DL TB` 误块率的影响
- v4.3 已将默认天线切换为定向口径；历史 `isotropic` 仍可通过命令行参数切换，供 `LEGACY-ISO` 对照与诊断使用
- 当前默认控制台输出已收紧为研究导向摘要
- 当前保留 `forceRlcAmForEpc` 作为可选稳定性开关，但默认不改变 helper 的 `RLC` 选择
- 当前默认 `carrierReuseMode = reuse1`，即所有卫星共享同一 operation band；`reuse2-plane` 和 `reuse4` 是为缓解同频干扰而加入的诊断模式，不替代正式 baseline 定义
- 当 `carrierReuseMode != reuse1` 时，不同卫星按载波复用规则使用不同中心频率；Phase 2 新增 `interFrequencyHandoverEnabled` 参数：默认 `false` 保持 Phase 1 行为（`sameFrequencyHandoverOnly=true`），设为 `true` 时检测跨频候选并记录触发尝试日志；Phase 2 不实现完整 3GPP inter-frequency measurement，真实执行仍受 NR Ideal RRC 限制（不支持 RLF），当前更适合作为 PHY 干扰缓解诊断路径

## 当前切换语义
- 当前 baseline 仍属于传统 `A3` 风格切换
- 判决主线为：标准 `MeasurementReport` 上报的 `RSRP` 比较、`hysteresis` 与 `TTT`
- `handoverMode = baseline` 时，直接在 A3 上报候选中选择最强邻区
- `handoverMode = improved` 时，仍使用同一批真实测量候选，但在目标选择时联合考虑 `RSRP`、`RSRQ`、`remainingVisibility` 与 `loadScore`，并在源站接近拥塞时进一步偏向轻载候选
- improved 允许先用最小剩余可见时间做硬门控，再在保留下来的候选中做联合评分；只有当同一目标的联合领先状态持续至少 `improvedMinStableLeadTimeSeconds`，且最佳候选的联合分数相对当前服务星至少高出 `improvedMinJointScoreMargin` 时，才触发切换
- 当前 improved 还支持一层轻量弱链路保护：可对候选施加最小 `RSRP/RSRQ` 门槛，并在当前服务链路已经明显偏弱时优先回到更保守的强信号切换
- 当前 improved 还支持一层可选的轻量跨层 PHY 辅助：当最近 PHY `TBler/SINR` 持续恶化时，可临时跳过部分等待门控并优先逃离当前差链路
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
- 若需检查下行业务端到端时延与丢包率，优先查看 `e2e_flow_metrics.csv`
- 若需检查 PHY 下行误块表现，优先查看 `phy_dl_tb_metrics.csv`

### 2. 研究行为验证
目标：确认当前 baseline 是否具备研究分析价值。

重点检查：
- 是否出现非零切换，而不是全程无切换
- 切换流程是否可正常闭环，以及切换次数、执行时延和业务丢包是否可统计
- 若有失败，是否能在 `handover_event_trace.csv` 中区分 `noPreamble / maxRach / leaving / joining / unknown` 等失败原因
- 切换附近是否能观察吞吐扰动，并优先结合 `handover_dl_throughput_trace.csv` 与 `handover_event_trace.csv` 对齐 `HO Start / HO Success`
- 下行业务流的 `E2E delay` 与 `packet loss rate` 是否能在 `e2e_flow_metrics.csv` 中形成可比较差异
- PHY 下行 `corrupt TB rate`、`mean TBler` 与 `SINR` 是否能在 `phy_dl_tb_metrics.csv` 中形成可比较差异
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
- 固定 `B00` 作为当前 baseline 主对照组，不再把“重新寻找 baseline”作为默认下一步
- 在同一 `MeasurementReport` 入口下，对比 `B00` 与 `improved` 的目标选择差异
- 以 `I31` 为 improved 默认起点继续细调权重与轻量门控
- 保持 baseline 与改进算法的对比边界清楚，避免同时改动场景口径、随机信道扰动和决策逻辑
