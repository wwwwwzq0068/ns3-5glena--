# 当前任务记忆

## 当前版本状态
- 最近已发布稳定节点：`5.1`（Git tag：`research-v5.1`）
- 当前工作区主线与 `research-v5.1` 对齐；该版本在 `research-v5.0` 的 thesis-mainline 清理基础上，完成主脚本继续瘦身、无线/场景/输出 helper 拆分、几何波束参数自动统一到真实 PHY 默认口径，以及 focused tests 补齐
- 当前主仿真入口：`scratch/leo-ntn-handover-baseline.cc`

## 当前 baseline 快照
- 当前默认场景：`2x4` 双轨、`25 UE`、带一圈空白邻区的 `seven-cell` 二维部署
- 当前 baseline：传统 `A3` 风格切换，但触发入口已经统一到标准 `PHY/RRC MeasurementReport`
- 当前 improved：复用同一批真实测量候选，在目标选择时联合考虑 `signal`、`remainingVisibility` 与 `loadScore`，并逐步加入联合领先持续时间门控、可见性门控、源站负载感知的负载覆写门槛，以及相对当前服务星的联合分数最小分差约束
- 原来的几何 `beam budget/custom A3` handover 代理链已从主线移除；几何计算只保留给轨道推进、地面锚点、初始接入以及真实候选接入门控
- 当前新增 `ueLayoutType = r2-diagnostic` 作为诊断场景：围绕当前 `seven-cell` 中心 hex 的局部 `2-ring` 窗口放置 `19 UE` hex 中心点，用来检查 `sat_ground_track`、`sat_anchor_trace` 与局部坏 `UE/坏时窗`；它不替代正式 baseline 定义
- 当前毕设收口目标已经调整为业务与切换层指标优先：`E2E delay`、`packet loss rate`、`SINR`、`ping-pong`、`load balance`
- `PHY DL TB error / mean TBler`、阵列规模、波束宽度、功率与 `MCS` 等项现在只保留为背景诊断口径，不再作为当前主优化目标

当前默认关键参数：
- `gNbNum = 8`
- `orbitPlaneCount = 2`
- `ueNum = 25`
- `ueLayoutType = seven-cell`
- `hexCellRadiusKm = 20`
- `enforceBeamExclusionRing = true`
- `beamExclusionCandidateK = 64`
- `enforceBeamCoverageForRealLinks = true`
- `enforceAnchorCellForRealLinks = true`
- `preferDemandAnchorCells = true`
- `anchorSelectionMode = demand-max-ue-near-nadir`
- `demandSnapshotMode = runtime-underserved-ue`
- `ueCenterSpacingMeters = 6000`
- `ueRingPointOffsetMeters = 5000`
- `interPlaneRaanSpacingDeg = -0.58 deg`
- `plane0RaanOffsetDeg = 1.09 deg`
- `interPlaneTimeOffsetSeconds = 7.5 s`
- `plane0TimeOffsetSeconds = -3.5 s`
- `alignmentReferenceTimeSeconds = 6.5 s`
- `overpassGapSeconds = 6 s`
- `plane1OverpassGapSeconds = 3 s`
- `simTime = 60 s`
- `updateIntervalMs = 100 ms`
- `lambda = 250 pkt/s/UE`
- `maxSupportedUesPerSatellite = 5`
- `hoHysteresisDb = 2.0 dB`
- `hoTttMs = 160 ms`
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
- `improvedEnableCrossLayerPhyAssist = false`
- `improvedCrossLayerTblerThreshold = 0.48`
- `improvedCrossLayerSinrThresholdDb = -5 dB`
- `improvedCrossLayerMinSamples = 50`
- `pingPongWindowSeconds = 1.5 s`
- `forceRlcAmForEpc = false`
- `disableUeIpv4Forwarding = true`
- `schedulerType = ofdma-rr`
- `gnbAntennaRows/Columns = 12x12`
- `gnbAntennaElement = b00-custom`（当前稳定默认；v4.3 引入）
- `ueAntennaElement = three-gpp`（当前稳定默认；v4.3 引入）
- `b00MaxGainDb = 20.0`
- `b00BeamwidthDeg = 4.0`
- `b00MaxAttenuationDb = 30.0`
- 几何波束 `BeamModelConfig` 的峰值、宽度和衰减由 `b00-*` 与 `gNB 12x12` 自动推导
- `earthFixedBeamTargetMode = grid-anchor`
- `phyDlTbIntervalSeconds = 1.0 s`

## 当前已确认实现
- 主脚本与运行时、统计、工具辅助头文件的拆分已经完成
- `main()` 当前已进一步把无线 bootstrap、UE 初始接入/业务安装、trace 输出生命周期分别下放到 `scratch/handover/leo-ntn-handover-radio.h`、`scratch/handover/leo-ntn-handover-scenario.h` 与 `scratch/handover/leo-ntn-handover-output.h`，主脚本继续朝“场景装配 + 时序调度 + 汇总收尾”收口
- 当前 `UE` 位置生成逻辑已收口为“两阶段”实现：先生成局部东-北平面偏移模板，再统一转换为 `WGS84` 地理点和 `ECEF`
- 当前默认 `UE` 布局为 `seven-cell`：中心小区 `3x3` 密集簇 `9 UE`，外围 `6` 个二跳小区共 `16 UE`，中心与外围之间保留一圈空白邻区以降低同频干扰
- 当前默认波束锚点启用一圈排他：一个卫星波束占用某个 hex 中心后，该中心及其周围一圈邻区都不会再被其它卫星选作最终落点
- 当前默认锚点选择模式已切到 `demand-max-ue-near-nadir`：先取星下点最近格作为主落点；若主落点有 `UE` 且合法，则直接使用；若主落点无 `UE`，只检查周围一圈邻格，只要存在合法且有 `UE` 的候选，就优先选择其中“运行时 demand 权重”最高的格子，若权重并列，再按驻留 `UE` 数和“更接近星下点/更低扫描代价”打破平局；仅当主落点和周围一圈都没有 `UE` 时，才允许回退到这个空主落点
- 当前默认 `demandSnapshotMode = runtime-underserved-ue`：需求格子不再只按启动时静态 UE 布局统计，而是在每个更新周期按 UE 所在地面格子重建 demand snapshot；若 UE 当前无服务、已偏离服务星锚点/主覆盖、服务星过载，或最近 PHY 已进入弱链路状态，则对应格子的 demand 权重会上调
- 若主落点或一圈内有需求的候选因重复/邻占排他或 beam/scan 约束而不合法，则当前周期不会硬抢非法格子，而是继续回退到现有合法候选链
- 若需回到旧口径，仍可显式设置 `anchorSelectionMode=demand-nearest` 与 `demandSnapshotMode=static-layout`
- 当前默认真实 NR beamforming 使用 `ideal-earth-fixed + earthFixedBeamTargetMode=grid-anchor`：`gNB` 侧真实 PHY 发射波束锁到当前卫星已分配的唯一合法 anchor hex 中心，不跟着单个 `UE` 跑；`UE` 侧仍按当前服务卫星做 direct-path 接收对准
- 当前默认真实 `MeasurementReport` 候选会同时受连续波束主覆盖与 anchor hex cell 门控约束，避免接入/切换到明显不属于当前波束落点的小区
- 当前默认 `b00BeamwidthDeg` 已从早期 `15.0` 收紧到 `4.0`，并将 `gNB` 阵列规模提升到 `12x12`；它们一起把真实 PHY 覆盖压回当前 hex 小区尺度附近，作为当前 same-frequency baseline 的默认物理口径
- 当前几何链路预算不再维护独立的 `beamMaxGainDbi/theta3dBDeg/sideLobeAttenuationDb` 手写参数，而是由 `b00MaxGainDb + 10*log10(gnbAntennaRows * gnbAntennaColumns)`、`b00BeamwidthDeg` 和 `b00MaxAttenuationDb` 推导，避免几何观测口径与真实 PHY 默认口径分叉
- 当前 `NrChannelHelper` 已配置 `NTN-Rural + LOS + ThreeGpp`，并开启 `ShadowingEnabled`
- baseline 与 improved 都通过 `NrLeoA3MeasurementHandoverAlgorithm` 注册标准 A3 测量，并在同一份 `MeasurementReport` 上做目标选择
- `handoverMode = baseline` 时，直接选择最强测量邻区
- `handoverMode = improved` 时，直接在同一批测量邻区上按“信号质量 + 可见性效用 + 负载效用”联合打分，并对过载候选、联合领先持续时间、最小剩余可见时间、过小负载优势和过小联合分差做额外门控
- 当前 improved 可选启用轻量跨层 PHY 辅助：若最近下行 TB 已持续出现 `TBler` 偏高或 `SINR <= -5 dB`，则把当前服务星判为弱 PHY，并放宽部分等待门控
- 周期更新中已经会计算每星 `attachedUeCount`、`offeredPacketRate`、`loadScore`、`admissionAllowed`
- 当前默认关闭高噪声的 `KPI`、`GRID-ANCHOR` 输出，保留切换、进度和最终汇总日志
- 当前已支持按成功切换序列自动统计短时 `ping-pong`
- 当前默认关闭 `SRS` 调度相关项，避免与 handover 主线无关的 `PHY fatal`
- 旧的 inter-frequency / carrier-reuse / beamwidth / sidelobe 等诊断入口已经从活动工作区移除，当前仓库只保留 thesis 主线和 `19 UE` 诊断入口
- focused tests 当前除 `grid-anchor / earth-fixed target / plane0 offset / r2-diagnostic layout / baseline defaults` 外，还新增 `test-baseline-config-helpers.cc`，用于保护输出路径重定向和派生位置配置逻辑

## 当前负载接口状态
- `SatelliteRuntime` 已具备 `attachedUeCount`、`offeredPacketRate`、`loadScore`、`admissionAllowed`
- `loadScore` 当前使用更平滑的容量压力近似，避免在每星 `5 UE` 容量口径下过早饱和
- 这些量已进入 `handoverMode = improved` 的目标选择，且 improved 会根据源站负载压力动态偏向轻载目标
- baseline 仍不使用负载做决策，因此对照边界保持清楚

## 当前输出与脚本
- 默认结果目录：`scratch/results/`
- 当前默认输出：
  - `e2e_flow_metrics.csv`
  - `handover_event_trace.csv`
  - `handover_dl_throughput_trace.csv`
  - `hex_grid_cells.csv`
  - `hex_grid_cells.svg`（当 `runGridSvgScript = true`）
  - `hex_grid_cells.html`（当 `runGridSvgScript = true`）
  - `ue_layout.csv`
  - `sat_anchor_trace.csv`
  - `sat_ground_track.csv`
  - `phy_dl_tb_metrics.csv`（保留作 `SINR` 与坏链路背景诊断）
  - `phy_dl_tb_interval_metrics.csv`（保留作时间窗背景诊断）
- 当前常用分析脚本：
  - `scratch/plotting/plot_hex_grid_svg.py`
    - 现已同时输出静态 `SVG` 与交互式 `HTML`；`HTML` 默认会叠加同目录下的 `ue_layout.csv`，并支持 `8` 星筛选、最终波束落点轨迹与真实连续地面轨迹双层对照

## 当前研究问题
- 继续验证传统 `A3 baseline` 在当前双轨场景下是否能稳定暴露：
  - 频繁切换
  - 潜在 `ping-pong`
  - 触发过早或过晚
  - 吞吐连续性问题
- 继续核实当前 `2x4` 双轨场景在新几何口径下是否持续保持双轨竞争，而不是只在单次短跑里偶然对齐
- 区分 `hex grid`（地面锚点/服务区域目录）与真实 `cell`（协议栈中的卫星小区）语义，避免把 `hex` 数量直接等同为可接入小区数量
- 评估当前二跳外围 `6` 个小区中的 `UE` 分布是否足够均衡，是否在降低同频干扰后仍形成可观察的跨小区竞争
- 在保持当前场景口径不变的前提下，优先改善和解释以下最终指标：
  - `E2E delay`
  - `packet loss rate`
  - `SINR`
  - `ping-pong`
  - `load balance`
- 后续定位最差 `UE`、最差时间窗和服务星/候选星竞争关系时，以这些最终指标的变化为准；`PHY TB` 统计只作为辅助解释

## 当前工作边界
- 当前不把“继续扩大星座规模”作为默认主线，除非现有 `2x4` 场景已无法体现算法差异
- 当前不再把“代码结构整理”作为第一优先级，除非算法实现需要新的结构调整
- 当前 baseline 的 `UE` 场景口径已经固定为带一圈空白邻区的 `seven-cell`
- 修改 `scratch/` 目录下的重要代码后，同步检查：
  - `scratch/README.md`
  - `scratch/baseline-definition.md`
  - `scratch/midterm-report/README.md`

## 当前优先方向
- 稳住 `2x4` 双轨、`25 UE`、带一圈空白邻区的 `seven-cell` baseline 场景
- 当前 same-frequency 默认 baseline 固定为：`reuse1 + schedulerType=ofdma-rr + earthFixedBeamTargetMode=grid-anchor + beamExclusionCandidateK=64 + gNB 12x12`
- 当前最优先的工作不是继续扫 PHY 小开关，而是在固定场景和固定 baseline 上，让 `baseline / improved` 在以下指标上拉开差异：
  - 更低 `E2E delay`
  - 更低 `packet loss rate`
  - 更稳 `SINR`
  - 更少 `ping-pong`
  - 更均衡 `load balance`
- `I31` 继续保留为 improved 对照参考点，但后续更应围绕目标选择、负载感知、稳定性门控和切换竞争关系来优化
- 当前最直接的正式分析入口优先看 `thesis-default40-baseline-rerun`、`thesis-default40-improved-rerun` 以及相关 `19 UE` 诊断结果

## 当前收口结论
- 当前 `4.2` 阶段的 baseline / improved 对照已经完成一轮 `30 s`、`3` 个随机种子的参数矩阵筛选
- 当前 same-frequency 默认 baseline 已经固定为：`reuse1 + ofdma-rr + grid-anchor + beamExclusionCandidateK=64 + gNB 12x12`
- 之前的大量 PHY / 阵元 / 波束 / 干扰诊断已经完成，它们的作用主要是帮助选定当前 baseline 默认口径；这些结果现在保留为历史背景，不再单独驱动后续优化顺序
- 当前更重要的正式结论是：
  - baseline 与 improved 必须回到同一固定场景下，比对 `E2E delay`、`packet loss rate`、`SINR`、`ping-pong` 与 `load balance`
  - 当前默认 `25 UE + 40 s` 口径下，baseline 与 improved 还没有拉开有效差异，因此下一步应优先寻找能真正改善这些指标的 improved 机制
  - `r2-diagnostic + 19 UE` 口径继续保留为辅助诊断入口，但它不替代正式 thesis 对照场景
- 新近补充的 `19 UE + 40 s + no-exclusion` 结果可作为一条场景对照：
  - throughput `28.278 Mbps`
  - `E2E delay = 56.733 ms`
  - `packet loss = 25.584%`
  - `mean SINR = 7.912 dB`
  - completed HO `27`
  - ping-pong `0`
  - 这说明单独去掉一圈排他并没有自动把业务和切换指标推到更优区间，因此后续仍要以整体 KPI 为准，而不是只看单个场景开关
- 当前统一研究口径已经改成：以业务与切换层效果为主，以 PHY 细项为辅；后续任何新方案都优先回答“是否改善了 `E2E delay / loss / SINR / ping-pong / load balance`”

## 当前汇报准备入口
- 版本收口说明优先参考本文件和 `scratch/README.md`
- 中期材料是否已清理，优先参考 `scratch/midterm-report/README.md`
