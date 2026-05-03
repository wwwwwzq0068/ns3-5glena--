# 当前任务记忆

## 当前版本状态
- 当前版本定位：`6.1`（建议 Git tag：`research-v6.1`）
- `research-v6.1` 定义为当前论文正式场景与结果数据版本；在 `research-v6.0` 输出链收口基础上，默认场景固定为 `2x4 + poisson-3ring + overlap-only + beam-only`
- 当前主仿真入口：`scratch/leo-ntn-handover-baseline.cc`

## 当前 baseline 快照
- 当前默认场景：`2x4` 双轨、`poisson-3ring` UE 空间分布、`overlap-only` 波束锚点排他、`beam-only` 真实链路候选门控
- 当前 baseline：传统 `A3` 风格切换，但触发入口已经统一到标准 `PHY/RRC MeasurementReport`
- 当前 improved：复用同一批真实测量候选，在目标选择时联合考虑 `signal`、`remainingVisibility` 与 `loadScore`，并逐步加入联合领先持续时间门控、可见性门控、源站负载感知的负载覆写门槛，以及相对当前服务星的联合分数最小分差约束
- 原来的几何 `beam budget/custom A3` handover 代理链已从主线移除；几何计算只保留给轨道推进、地面锚点、初始接入以及真实候选接入门控
- 当前 `ueLayoutType = poisson-3ring` 是唯一 UE 生成方式；旧固定 UE 布局入口已从活动代码中移除
- 当前 `2x4 + poisson-3ring + overlap-only + beam-only` 是唯一论文写作和正式结果方案；旧固定 UE、旧结果目录和旧门控组合只作为历史背景，不再进入主线结果
- 当前毕设收口目标已经调整为业务、切换与负载层指标优先：`E2E delay`、`packet loss rate`、`throughput`、`completed handovers`、`ping-pong`、`Jain load fairness`
- `SINR`、`PHY DL TB error / mean TBler`、阵列规模、波束宽度、功率、`MCS` 与信令开销不再进入正式论文主表；当前主表只保留业务、切换与负载层 KPI

当前默认关键参数：
- `gNbNum = 8`
- `orbitPlaneCount = 2`
- `ueNum = 30`
- `ueLayoutType = poisson-3ring`
- `hexCellRadiusKm = 20`
- `poissonLambda = 1.5`
- `maxUePerCell = 5`
- `ueLayoutRandomSeed = 42`
- `beamExclusionCandidateK = 64`
- `preferDemandAnchorCells = true`
- `anchorSelectionMode = demand-max-ue-near-nadir`
- `demandSnapshotMode = runtime-underserved-ue`
- `interPlaneRaanSpacingDeg = -0.58 deg`
- `plane0RaanOffsetDeg = 1.09 deg`
- `interPlaneTimeOffsetSeconds = 7.5 s`
- `plane0TimeOffsetSeconds = -3.5 s`
- `alignmentReferenceTimeSeconds = 6.5 s`
- `overpassGapSeconds = 6 s`
- `plane1OverpassGapSeconds = 3 s`
- `simTime = 40 s`
- `updateIntervalMs = 100 ms`
- `lambda = 250 pkt/s/UE`
- `maxSupportedUesPerSatellite = 5`
- `hoHysteresisDb = 2.0 dB`
- `hoTttMs = 160 ms`
- `measurementReportIntervalMs = 120 ms`
- `measurementMaxReportCells = 8`
- `handoverMode = baseline`
- `enableSatelliteStateTrace = true`
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
- `improvedMinCandidateRsrpDbm = -118 dBm`
- `improvedMinCandidateRsrqDb = -17 dB`
- `improvedServingWeakRsrpDbm = -118 dBm`
- `improvedServingWeakRsrqDb = -15 dB`
- `improvedEnableCrossLayerPhyAssist = false`
- `improvedCrossLayerTblerThreshold = 0.48`
- `improvedCrossLayerSinrThresholdDb = -5 dB`
- `improvedCrossLayerMinSamples = 50`
- `pingPongWindowSeconds = 1.5 s`
- `forceRlcAmForEpc = false`
- `disableUeIpv4Forwarding = true`
- `NR scheduler = ofdma-rr`（代码固定为 `NrMacSchedulerOfdmaRR`，不是当前论文场景调参项）
- `gnbAntennaRows/Columns = 12x12`
- `gnbAntennaElement = b00-custom`（当前稳定默认；v4.3 引入）
- `ueAntennaElement = three-gpp`（当前稳定默认；v4.3 引入）
- `b00MaxGainDb = 20.0`
- `b00BeamwidthDeg = 4.0`
- `b00MaxAttenuationDb = 30.0`
- 几何波束 `BeamModelConfig` 的峰值、宽度和衰减由 `b00-*` 与 `gNB 12x12` 自动推导
- `earthFixedBeamTargetMode = grid-anchor`

## 当前已确认实现
- 主脚本与运行时、统计、工具辅助头文件的拆分已经完成
- `main()` 当前已进一步把无线 bootstrap、UE 初始接入/业务安装、trace 输出生命周期分别下放到 `scratch/handover/leo-ntn-handover-radio.h`、`scratch/handover/leo-ntn-handover-scenario.h` 与 `scratch/handover/leo-ntn-handover-output.h`，主脚本继续朝“场景装配 + 时序调度 + 汇总收尾”收口
- 当前 `UE` 位置生成逻辑已收口为“两阶段”实现：先生成局部东-北平面偏移模板，再统一转换为 `WGS84` 地理点和 `ECEF`
- 当前默认且唯一的 `UE` 布局为 `poisson-3ring`：在中心及两圈共 `19` 个 hex cell 内按截断泊松权重分配 `UE`，并在各 cell 内随机撒点；实现保持总数等于 `ueNum`，以维持现有节点、业务和统计管线
- 当前默认波束锚点使用固定 overlap-only 排他：一个卫星波束占用某个 hex 中心后，其它卫星不能复用同一个 anchor cell，但允许相邻 anchor 参与竞争
- 当前默认锚点选择模式已切到 `demand-max-ue-near-nadir`：先取星下点最近格作为主落点；若主落点有 `UE` 且合法，则直接使用；若主落点无 `UE`，只检查周围一圈邻格，只要存在合法且有 `UE` 的候选，就优先选择其中“运行时 demand 权重”最高的格子，若权重并列，再按驻留 `UE` 数和“更接近星下点/更低扫描代价”打破平局；仅当主落点和周围一圈都没有 `UE` 时，才允许回退到这个空主落点
- 当前默认 `demandSnapshotMode = runtime-underserved-ue`：需求格子不再只按启动时静态 UE 布局统计，而是在每个更新周期按 UE 所在地面格子重建 demand snapshot；若 UE 当前无服务、已偏离服务星锚点/主覆盖、服务星过载，或最近 PHY 已进入弱链路状态，则对应格子的 demand 权重会上调
- 若主落点或一圈内有需求的候选因重复/邻占排他或 beam/scan 约束而不合法，则当前周期不会硬抢非法格子，而是继续回退到现有合法候选链
- 若需回到旧口径，仍可显式设置 `anchorSelectionMode=demand-nearest` 与 `demandSnapshotMode=static-layout`
- 当前默认真实 NR beamforming 使用 `ideal-earth-fixed + earthFixedBeamTargetMode=grid-anchor`：`gNB` 侧真实 PHY 发射波束锁到当前卫星已分配的唯一合法 anchor hex 中心，不跟着单个 `UE` 跑；`UE` 侧仍按当前服务卫星做 direct-path 接收对准
- 当前默认真实 `MeasurementReport` 候选只受连续波束主覆盖门控，anchor hex cell gate 已从正式口径中放开，避免候选集合被地面格子边界过度收窄
- 当前默认 `b00BeamwidthDeg` 已从早期 `15.0` 收紧到 `4.0`，并将 `gNB` 阵列规模提升到 `12x12`；它们一起把真实 PHY 覆盖压回当前 hex 小区尺度附近，作为当前 same-frequency baseline 的默认物理口径
- 当前几何链路预算不再维护独立的 `beamMaxGainDbi/theta3dBDeg/sideLobeAttenuationDb` 手写参数，而是由 `b00MaxGainDb + 10*log10(gnbAntennaRows * gnbAntennaColumns)`、`b00BeamwidthDeg` 和 `b00MaxAttenuationDb` 推导，避免几何观测口径与真实 PHY 默认口径分叉
- 当前 `NrChannelHelper` 已配置 `NTN-Rural + LOS + ThreeGpp`，并开启 `ShadowingEnabled`
- baseline 与 improved 都通过 `NrLeoA3MeasurementHandoverAlgorithm` 注册标准 A3 测量，并在同一份 `MeasurementReport` 上做目标选择
- `handoverMode = baseline` 时，直接选择最强测量邻区，不再额外计算 `remainingVisibility`
- `handoverMode = improved` 时，直接在同一批测量邻区上按“信号质量 + 可见性效用 + 负载效用”联合打分，并对过载候选、联合领先持续时间、最小剩余可见时间、过小负载优势和过小联合分差做额外门控
- 当前 `handoverMode` 已收口为 `baseline` 与 `improved` 两种正式路径；旧 `improved-score-only` 诊断入口已移除
- 当前 improved 可选启用轻量跨层 PHY 辅助：若最近下行 TB 已持续出现 `TBler` 偏高或 `SINR <= -5 dB`，则把当前服务星判为弱 PHY，并放宽部分等待门控
- 周期更新中已经会计算每星 `attachedUeCount`、`offeredPacketRate`、`loadScore`、`admissionAllowed`
- 当前默认输出 `satellite_state_trace.csv`，用于保留每星每时刻原始负载状态并计算 Jain load fairness
- 当前默认关闭高噪声的 `KPI`、`GRID-ANCHOR` 输出，保留切换、进度和最终汇总日志
- 当前运行时已缓存每个静态 `UE` 的 `homeGridId`，并预建 `gridId -> cell/first-ring-neighbors` 查表，避免 `100 ms` 周期里重复做相同 hex 查询
- 当前已支持按成功切换序列自动统计短时 `ping-pong`
- 当前默认关闭 `SRS` 调度相关项，避免与 handover 主线无关的 `PHY fatal`
- 旧的 inter-frequency / carrier-reuse / beamwidth / sidelobe / 固定 UE 布局诊断入口已经从活动工作区移除，当前仓库保留 thesis 主线和泊松 UE 生成入口
- focused tests 当前除 `grid-anchor / earth-fixed target / plane0 offset / poisson-3ring layout / baseline defaults` 外，还新增 `test-baseline-config-helpers.cc`，用于保护输出路径重定向和派生位置配置逻辑

## 当前负载接口状态
- `SatelliteRuntime` 已具备 `attachedUeCount`、`offeredPacketRate`、`loadScore`、`admissionAllowed`
- `loadScore` 当前使用更平滑的容量压力近似，避免在每星 `5 UE` 容量口径下过早饱和
- 这些量已进入 `handoverMode = improved` 的目标选择，且 improved 会根据源站负载压力动态偏向轻载目标
- baseline 仍不使用负载做决策，因此对照边界保持清楚

## 当前输出与脚本
- 默认结果目录：`scratch/results/`
- 当前默认输出：
  - `handover_event_trace.csv`
  - `handover_dl_throughput_trace.csv`
  - `satellite_state_trace.csv`
  - `hex_grid_cells.csv`
  - `hex_grid_cells.html`（当 `runGridHtmlScript = true` 且正式示意图复跑显式启用至少一个卫星轨迹 overlay）
  - `ue_layout.csv`
  - `e2e_flow_metrics.csv`（当 `enableFlowMonitor = true`，正式 KPI 跑再显式开启）
  - `sat_anchor_trace.csv`（当 `enableSatAnchorTrace = true`）
  - `sat_ground_track.csv`（当 `enableSatGroundTrackTrace = true`）
- 当前常用分析脚本：
  - `scratch/plotting/plot_hex_grid_svg.py`
    - 当前只输出交互式 `HTML`；`HTML` 默认会叠加同目录下的 `ue_layout.csv`，并支持 `8` 星筛选、最终波束落点轨迹与真实连续地面轨迹双层对照
  - `scratch/plotting/summarize_thesis_results.py`
    - 读取 `scratch/results/formal/v6.1-poisson3ring-overlap-beamonly-40s/{baseline,improved}/seed-*`，输出 `run_summary.csv`、`paper_kpi_summary.csv` 与 `paper_kpi_comparison.csv`；不读取 PHY/SINR/TBler 文件

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
  - `throughput`
  - `completed handovers`
  - `ping-pong`
  - `Jain load fairness`
- 后续定位最差 `UE`、最差时间窗和服务星/候选星竞争关系时，以这些最终指标的变化为准

## 当前工作边界
- 当前不把“继续扩大星座规模”作为默认主线，除非现有 `2x4` 场景已无法体现算法差异
- 当前不再把“代码结构整理”作为第一优先级，除非算法实现需要新的结构调整
- 当前 baseline 的 `UE` 场景口径已经固定为 `poisson-3ring`
- 修改 `scratch/` 目录下的重要代码后，同步检查：
  - `scratch/README.md`
  - `scratch/baseline-definition.md`
  - `scratch/midterm-report/README.md`

## 当前优先方向
- 稳住 `2x4 + poisson-3ring + overlap-only + beam-only` baseline 场景
- 当前 same-frequency 默认 baseline 固定为：`reuse1 + ofdma-rr + earthFixedBeamTargetMode=grid-anchor + overlap-only + beam-only + beamExclusionCandidateK=64 + gNB antenna array 12x12`
- 当前最优先的工作不是继续扫 PHY 小开关，而是在固定场景和固定 baseline 上，让 `baseline / improved` 在以下指标上拉开差异：
  - 更低 `E2E delay`
  - 更低 `packet loss rate`
  - 更高 `throughput`
  - 更少不必要的 completed handovers
  - 更少 `ping-pong`
  - 更高 `Jain load fairness`
- 后续正式分析入口统一看 `scratch/results/formal/v6.1-poisson3ring-overlap-beamonly-40s/{baseline,improved}/seed-*`；历史固定 UE 与旧门控结果目录不再作为论文主线结果

## 当前收口结论
- `research-v6.1` 的核心收口是正式论文场景与结果输出路径；baseline / full improved 固定在同一 `2x4 + poisson-3ring + overlap-only + beam-only` 下比较
- 正式论文输出现在固定为：`E2E delay`、`packet loss rate`、`throughput`、`completed handovers`、`ping-pong`、`Jain load fairness`
- 新增 `satellite_state_trace.csv` 作为每星每时刻原始负载状态；`scratch/plotting/summarize_thesis_results.py` 负责生成 `run_summary.csv`、`paper_kpi_summary.csv` 和 `paper_kpi_comparison.csv`
- `plot_hex_grid_svg.py` 已收紧为 HTML-only 场景/轨迹示意脚本；旧的吞吐绘图脚本和批量实验矩阵脚本已从活动工作区移除
- 历史固定 UE 和旧参数矩阵筛选只作为背景；`6.1` 正式论文数据统一从 `scratch/results/formal/v6.1-poisson3ring-overlap-beamonly-40s/{baseline,improved}/seed-*` 生成
- 当前 same-frequency 默认 baseline 已经固定为：`reuse1 + ofdma-rr + grid-anchor + overlap-only + beam-only + beamExclusionCandidateK=64 + gNB antenna array 12x12`
- 之前的大量 PHY / 阵元 / 波束 / 干扰诊断已经完成，它们的作用主要是帮助选定当前 baseline 默认口径；这些结果现在保留为历史背景，不再单独驱动后续优化顺序
- 当前更重要的正式结论是：
  - baseline 与 improved 必须回到同一固定场景下，比对 `E2E delay`、`packet loss rate`、`throughput`、`completed handovers`、`ping-pong` 与 `Jain load fairness`
  - 历史固定 UE 口径下，baseline 与 improved 还没有拉开有效差异；后续应在泊松 UE 口径下寻找能真正改善这些指标的 improved 机制
  - 旧固定 UE 诊断口径不再作为活动代码入口
- 当前正式 improved 默认 RSRP 保护门槛已收口到 `-118 dBm`：候选最小 RSRP 和服务链路弱判定都使用 `-118 dBm`，用于保留更弱但可能有负载/可见性优势的真实测量候选。
- 当前统一研究口径已经改成：以业务、切换与负载层效果为主，以 PHY 细项为诊断；后续任何新方案都优先回答“是否改善了 `E2E delay / loss / throughput / completed handovers / ping-pong / Jain load fairness`”

## 当前汇报准备入口
- 版本收口说明优先参考本文件和 `scratch/README.md`
- 中期材料是否已清理，优先参考 `scratch/midterm-report/README.md`
