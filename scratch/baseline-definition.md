# LEO-NTN Handover Baseline

## baseline 角色
- 当前 baseline 已经从早期 `B00` 收口到新的 same-frequency 默认物理/调度口径，可以作为后续改进算法的对照组与接入平台
- 它的任务不是追求最优，而是提供一个清楚、稳定、可复现的比较起点
- 当前正式比较时，优先看业务与切换层 KPI：`E2E delay`、`packet loss rate`、`SINR`、`ping-pong`、`load balance`
- 当前 baseline 与后续改进算法的边界是：
  - baseline：只基于信号质量做切换判决
  - improved：在同一场景下引入负载、剩余可见时间或联合代价项

## baseline 定义
当前建议将 baseline 定义为：

> 在固定 `2x4` 双轨、`25 UE`、中等时长仿真场景下，采用基于标准 `PHY/RRC MeasurementReport` 的传统 `A3` 风格切换基线；UE 使用带一圈空白邻区的 `seven-cell`（中心 1 小区 + 二跳外围 6 小区）二维部署，baseline 决策仅依赖 `RSRP`、`hysteresis` 与 `TTT`，不引入负载感知、预测优化或学习型决策。

这一定义强调三件事：
- 场景边界固定
- 决策边界清楚
- 后续所有改进都必须相对于它做对照

补充边界：
- `ueLayoutType=r2-diagnostic` 是新增的诊断路径，不属于正式 baseline 定义
- 该诊断路径仅用于在当前 `seven-cell` 中心 hex 周围的局部 `2-ring` 窗口中放置 `19 UE` hex 中心点，以检查 `sat_ground_track`、`sat_anchor_trace` 与局部坏 `UE/坏时窗`
- `PHY DL TB error / TBler`、阵元、波束宽度、功率等结果现在只保留为辅助解释项，不再作为 baseline 主优化目标

## 当前默认场景口径
场景：
- `gNbNum = 8`
- `orbitPlaneCount = 2`
- `ueNum = 25`
- `ueLayoutType = seven-cell`
- `hexCellRadiusKm = 20`
- `anchorGridHexRadiusKm = 20`
- `enforceBeamExclusionRing = true`
- `beamExclusionCandidateK = 64`
- `enforceBeamCoverageForRealLinks = true`
- `enforceAnchorCellForRealLinks = true`
- `preferDemandAnchorCells = true`
- `anchorSelectionMode = demand-max-ue-near-nadir`
- `demandSnapshotMode = runtime-underserved-ue`
- `ueCenterSpacingMeters = 6000`
- `ueRingPointOffsetMeters = 5000`
- `satAltitudeMeters = 600000`
- `orbitInclinationDeg = 53`
- `interPlaneRaanSpacingDeg`（轨道面 RAAN 间隔）=`-0.58`
- `plane0RaanOffsetDeg`（仅 `plane 0` 的轨道面额外偏转）=`1.09`
- `interPlaneTimeOffsetSeconds`（轨道面时间偏移）=`7.5`
- `plane0TimeOffsetSeconds`（仅 `plane 0` 额外物理相位偏移）=`-3.5`
- `alignmentReferenceTimeSeconds`（对齐参考时刻）=`6.5`
- `overpassGapSeconds`（主同轨过境间隔）=`6`
- `plane1OverpassGapSeconds`（用于保持 plane 1 初始诊断几何的同轨间隔）=`3`

运行与切换：
- `simTime`（仿真时长）=`60 s`
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
- `gnbAntennaRows/Columns = 12x12`
- `ueAntennaRows/Columns = 1x2`
- `gnbAntennaElement = b00-custom`（当前稳定默认；v4.3 引入；可选：`isotropic`, `three-gpp`）
- `ueAntennaElement = three-gpp`（当前稳定默认；v4.3 引入；可选：`isotropic`, `b00-custom`）
- `b00MaxGainDb = 20.0`（b00-custom 阵元峰值增益）
- `b00BeamwidthDeg = 4.0`（b00-custom 阵元方向图宽度参数）
- `b00MaxAttenuationDb = 30.0`（b00-custom 旁瓣衰减上限）
- `beamformingMode = ideal-earth-fixed`
- `earthFixedBeamTargetMode = grid-anchor`
- `beamformingPeriodicityMs = 100`
- `realisticBfTriggerEvent = srs-count`
- `realisticBfUpdatePeriodicity = 3`
- `realisticBfUpdateDelayMs = 0`
- `phyDlTbIntervalSeconds = 1.0 s`
- `enablePhyDlTbTrace = false`
- `phyDlTbTracePath = <outputDir>/phy_dl_tb_trace.csv`
- `useWgs84HexGrid = true`
- `anchorGridSwitchGuardMeters = 0`
- `anchorGridHysteresisSeconds = 0`
- `forceRlcAmForEpc = false`
- `disableUeIpv4Forwarding = true`
- 当前固定载波口径：`2 GHz / 40 MHz / 1 CC / same-frequency`
- 当前固定 `NR` 下行调度器：`ofdma-rr`

说明：
- 这套参数代表当前工作区的默认 baseline 口径；最近已发布稳定节点为 `research-v5.1`
- 当前后续一段时间不再通过继续扩星来放大现象，而是固定当前 `2x4` 双轨、`25 UE`、`seven-cell` 场景
- 当前 `UE` 生成实现已收口为“局部东-北平面偏移模板 + 统一 `WGS84/ECEF` 转换”的两阶段写法
- 当前默认布局为：中心小区 `3x3` 密集簇 `9 UE`，外围 `6` 个二跳小区共 `16 UE`，中间一圈相邻小区不放置 `UE`
- 当前默认还给 `plane 0` 增加了 `+1.09 deg` 的额外 `RAAN` 偏转，并把双轨 `RAAN` 间隔收紧到 `-0.58 deg`，用来把 plane 0 压到接近 `73-63-53` 这条线、把 plane 1 压到接近 `97-87-77` 这条线；当前 `plane0TimeOffsetSeconds=-3.5 s` 用于把 plane 0 的 sat0 初始落点调到 `hex73` 附近，`plane1OverpassGapSeconds=3 s` 用于在主同轨间隔改为 `6 s` 后保持 plane 1 初始诊断几何不变
- 当前还支持显式设置 `ueLayoutType=r2-diagnostic --ueNum=19` 做局部诊断：在当前 `seven-cell` 中心 hex 周围生成 `2-ring`、共 `19` 个 hex 中心点；该口径只用于诊断，不替代正式 baseline 场景
- 当前默认仍保持 `anchorGridHexRadiusKm = hexCellRadiusKm`，并启用波束锚点一圈排他：其它卫星不能把最终落点选到已占锚点或其一圈邻区内
- 当前默认锚点选择模式为 `demand-max-ue-near-nadir`：先取星下点最近格作为主落点；若主落点有 `UE` 且合法，则直接使用；若主落点无 `UE`，则只检查周围一圈邻格，只要存在合法且有 `UE` 的候选，就优先选择其中“运行时 demand 权重”最高的格子；若权重并列，再按驻留 `UE` 数以及“更接近星下点 + 更低扫描代价”打破平局
- 当前默认 `demandSnapshotMode = runtime-underserved-ue`：需求格子在每个更新周期按 UE 所在地面格子重建；若 UE 当前无服务、已偏离服务星锚点/主覆盖、服务星过载，或最近 PHY 已进入弱链路状态，则对应格子的 demand 权重会上调
- 若主落点和周围一圈都没有 `UE`，则允许回退到这个空主落点；但若周围一圈存在 `UE` 候选、却都因重复/邻占排他或 beam/scan 约束而不合法，则不会为了追 `UE` 去破坏排他规则，而是继续走合法 fallback
- 若需回到旧口径，可显式设置 `anchorSelectionMode=demand-nearest` 与 `demandSnapshotMode=static-layout`
- 当前实现仍允许给锚点切换增加距离优势与持续领先门控；若要做不带排他的旧口径对照，可显式关闭 `enforceBeamExclusionRing`
- 当前 PHY 信道保留 `ThreeGpp` 路径并开启 `ShadowingEnabled`，baseline 与 improved 都直接消费标准 `MeasurementReport`
- 当前稳定默认真实 NR PHY 为 `gNB b00-custom + UE three-gpp`，阵列规模 `gNB 12x12`、`UE 1x2`，该口径自 `v4.3` 起引入，当前 `research-v5.1` 继续沿用
- 当前默认 `earthFixedBeamTargetMode=grid-anchor`：真实 gNB 发射波束锁到当前卫星已分配的唯一合法 anchor hex 中心，不跟随单个 `UE`；`anchorCell` gate 默认参与真实接入/切换候选过滤
- 当前默认 `b00BeamwidthDeg=4.0` 已按真实 PHY 总方向图收紧，并配合 `gNB 12x12 UPA + b00-custom + ideal-earth-fixed` 进一步提高发射端空间定向性，使默认口径更接近当前 hex 小区尺度
- 几何波束参数现在从真实 PHY 默认口径自动推导：`gMax0Dbi = b00MaxGainDb + 10*log10(gnbAntennaRows * gnbAntennaColumns)`，`theta3dBRad = DegToRad(b00BeamwidthDeg)`，`slaVDb = b00MaxAttenuationDb`
- 当前所有卫星默认共享同一个 `2 GHz / 40 MHz / 1 CC` operation band，因此若需要验证高 `PHY DL TB error rate` 是否主要来自同频干扰，应优先从当前 same-frequency 场景内解释。旧的 carrier-reuse / inter-frequency 诊断入口已经从活动工作区移除。
- 当前代码已额外开放 `gnbAntennaElement`、`ueAntennaElement`、`beamformingMode` 与 `earthFixedBeamTargetMode` 作为诊断参数；默认真实接入/切换候选同时要求落在当前主覆盖与 anchor hex cell 约束内，`nadir-continuous` 保留为旧口径对照
- 当前稳定默认已切换为定向天线口径；历史 `isotropic` 仍可通过命令行参数切换，供 `LEGACY-ISO` 对照与诊断使用
- 当前默认控制台输出已收紧为研究导向摘要
- 当前保留 `forceRlcAmForEpc` 作为可选稳定性开关，但默认不改变 helper 的 `RLC` 选择
- 当前 baseline 固定使用 same-frequency 单载波口径，不再保留 carrier orthogonalization 或 inter-frequency handover 的活动代码入口
- 当前 baseline 固定使用 `ofdma-rr`，旧的 scheduler 对照入口已经从活动工作区移除

## 当前切换语义
- 当前 baseline 仍属于传统 `A3` 风格切换
- 判决主线为：标准 `MeasurementReport` 上报的 `RSRP` 比较、`hysteresis` 与 `TTT`
- `handoverMode = baseline` 时，直接在 A3 上报候选中选择最强邻区
- `handoverMode = improved` 时，仍使用同一批真实测量候选，但在目标选择时联合考虑 `RSRP`、`RSRQ`、`remainingVisibility` 与 `loadScore`，并在源站接近拥塞时进一步偏向轻载候选
- improved 允许先用最小剩余可见时间做硬门控，再在保留下来的候选中做联合评分；只有当同一目标的联合领先状态持续至少 `improvedMinStableLeadTimeSeconds`，且最佳候选的联合分数相对当前服务星至少高出 `improvedMinJointScoreMargin` 时，才触发切换
- 当前 improved 还支持一层轻量弱链路保护：可对候选施加最小 `RSRP/RSRQ` 门槛，并在当前服务链路已经明显偏弱时优先回到更保守的强信号切换
- 当前 improved 还保留一层可选的轻量跨层 PHY 辅助：当最近 PHY `TBler/SINR` 持续恶化时，可临时放宽部分等待门控
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
- 若需检查 `SINR` 的总体表现或解释某些异常 `UE`，再补看 `phy_dl_tb_metrics.csv`
- 若需做时间窗背景解释，再补看 `phy_dl_tb_interval_metrics.csv`

### 2. 研究行为验证
目标：确认当前 baseline 是否具备研究分析价值。

重点检查：
- 是否出现非零切换，而不是全程无切换
- 切换流程是否可正常闭环，以及切换次数、执行时延和业务丢包是否可统计
- 若有失败，是否能在 `handover_event_trace.csv` 中区分 `noPreamble / maxRach / leaving / joining / unknown` 等失败原因
- 切换附近是否能观察吞吐扰动，并优先结合 `handover_dl_throughput_trace.csv` 与 `handover_event_trace.csv` 对齐 `HO Start / HO Success`
- 下行业务流的 `E2E delay` 与 `packet loss rate` 是否能在 `e2e_flow_metrics.csv` 中形成可比较差异
- `SINR` 是否能在 `phy_dl_tb_metrics.csv` 中形成可比较差异，并能解释 E2E 表现
- 外围小区 UE 是否出现频繁切换或潜在 `ping-pong`
- 最终摘要中的自动 `ping-pong` 计数是否为非零，且与事件 trace 中的短时回切现象一致
- 星间 `attachedUeCount`、`offeredPacketRate`、`loadScore` 是否出现可观察的不均衡
- 若启用轨迹可视化，`sat_anchor_trace.csv` 是否能正确反映最终波束锚点的小区变化路径，`sat_ground_track.csv` 是否能正确反映 `8` 颗卫星的真实连续地面轨迹
- 若启用交互式检查页，`hex_grid_cells.html` 是否能支持 `8` 星总览、单星筛选、`UE` 分布叠加，以及真实轨迹/波束落点双层对照

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
- `E2E delay`
- `packet loss rate`
- `mean SINR`
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
- 固定当前 same-frequency 默认组作为 baseline 主对照组，不再把“重新寻找 baseline”作为默认下一步
- 后续所有改动优先回答是否改善了：`E2E delay`、`packet loss rate`、`SINR`、`ping-pong`、`load balance`
- `schedulerType=ofdma-rr`、`grid-anchor`、`beamExclusionCandidateK=64`、`gNB 12x12` 继续作为默认 baseline 口径保留，不再把它们作为当前主要调参对象
- 后续 baseline / improved 对照应更多围绕：
  - 目标选择是否更稳
  - 负载导向是否更有效
  - 是否减少无效切换和 `ping-pong`
  - 是否在不破坏 `SINR` 的前提下降低丢包和时延
- `r2-diagnostic` 继续保留给辅助定位最差 `UE` 与坏时窗，但不替代正式 thesis 对照场景
- 保持 baseline 与改进算法的对比边界清楚，避免同时改动场景口径、随机信道扰动和决策逻辑
