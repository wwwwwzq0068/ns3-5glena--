# Scratch 目录说明

## 目录用途
- 本目录存放当前毕设使用的 LEO-NTN 切换仿真代码、辅助头文件和分析脚本
- 当前主要仿真入口：`scratch/leo-ntn-handover-baseline.cc`
- 最近已发布稳定节点：`5.0`（Git tag：`research-v5.0`）
- 当前工作区与 `research-v5.0` 对齐；该版本在 `research-v4.3` 的定向天线与 `grid-anchor` 主线基础上，完成 thesis-mainline 清理、结果区收缩和 `19 UE r2-diagnostic` 诊断入口保留
- 这里的 `4.2.x` 是研究工作稳定节点，不是 ns-3 框架版本；ns-3 本身仍然是 `3.46`

## 当前主线
- 当前阶段先稳住基础组，再在此基础上验证和改进切换策略
- 当前工作区默认研究场景是：`2x4` 双轨、`25 UE`、带一圈空白邻区的 `seven-cell` 二维部署
- 当前 baseline 仍保持传统 `A3` 决策边界，默认 same-frequency 运行口径已固定为 `ofdma-rr + grid-anchor + K=64 + gNB 12x12`
- 当前还新增了 `ueLayoutType=r2-diagnostic` 诊断布局：围绕当前 `seven-cell` 中心 hex 的局部 `2-ring` 窗口放置 `19 UE` hex 中心点，用来观察连续地面轨迹、离散波束落点轨迹和局部坏 `UE/坏时窗`；它只用于诊断，不替代正式 baseline
- 当前 handover 主链已统一到标准 `PHY/RRC MeasurementReport`
- 平台上仍保留“信号质量 + 剩余可见时间 + 卫星负载”的 improved 路径，但当前近阶段主任务已经收口为业务与切换层 KPI 优化：`E2E delay`、`packet loss rate`、`SINR`、`ping-pong`、`load balance`
- 当前切换统计除 `HO start / HO success` 外，也会单独导出 `HO failure`、未闭合 `unresolved` 次数，以及底层 `RACH / timeout` 类失败原因明细
- 当前已接入基于 `FlowMonitor` 的下行业务端到端统计，可导出每个 `UE` 的 `E2E delay`、`packet loss rate` 与 `tx/rx/lost` 包数
- 当前已补充按 `UE` 聚合的 `SINR` 与 PHY 背景统计，但这些输出当前只作为解释业务与切换指标的辅助材料

## 当前版本判断
- 想确认“是不是进入新版本”，先看是否已打新的 `research-v5.x` 或 `research-v5.0` tag
- 目前最近已发布稳定节点是 `research-v5.0`
- 当前工作区中若继续补充结果、脚本或说明，默认按“`4.2` 之后的主阶段内继续迭代”理解，除非后续再打新 tag

## 当前进度整理
- 当前 `4.2` 已收口到统一真实测量驱动的 baseline / improved 主线，baseline 默认继续保持传统 `A3` 语义
- 当前 baseline 对照组保留 `handoverMode=baseline hoTttMs=160 hoHysteresisDb=2.0` 的 A3 决策语义，默认运行口径固定为 `ofdma-rr + grid-anchor + K=64 + gNB 12x12`
- 当前默认几何与业务口径已经固定为：`RAAN=-0.58 deg`、`plane0RaanOffset=1.09 deg`、`planeTimeOffset=7.5 s`、`plane0TimeOffset=-3.5 s`、`alignmentReferenceTime=6.5 s`、`overpassGap=6 s`、`lambda=250 pkt/s/UE`、`maxSupportedUesPerSatellite=5`
- 最新一轮 `30 s`、`3` 个随机种子的 improved 参数矩阵已经跑完，`I31` 继续保留为后续 baseline / improved 对照参考点
- 当前 thesis 口径已经不再把 `PHY DL TB error / TBler` 当成主目标，而是把这些内容降级成历史背景诊断；当前正式优化目标只保留：
  - `E2E delay`
  - `packet loss rate`
  - `SINR`
  - `ping-pong`
  - `load balance`
- 当前 `25 UE + 40 s` 默认口径下，baseline 与 improved 还没有拉开有效差异，因此接下来的重点不是继续扫 PHY 小开关，而是找到真正能改善这些最终指标的切换与负载机制
- `19 UE r2-diagnostic` 结果继续保留为辅助诊断入口，例如最新 `no-exclusion` 对照得到：
  - throughput `28.278 Mbps`
  - `E2E delay = 56.733 ms`
  - `packet loss = 25.584%`
  - `mean SINR = 7.912 dB`
  - completed HO `27`
  - ping-pong `0`
- 历史 PHY/阵元/波束诊断的主要价值已经完成：它们帮助把当前 default baseline 固定到更稳的 same-frequency 口径，但后续不再单独驱动研究顺序

## 当前默认口径
场景与参数：
- 卫星数：`8`
- 轨道面数：`2`
- UE 数：`25`
- UE 主布局：`seven-cell`
- `interPlaneRaanSpacingDeg`（轨道面 RAAN 间隔）=`-0.58 deg`
- `plane0RaanOffsetDeg`（仅 `plane 0` 的轨道面额外偏转）=`1.09 deg`
- `interPlaneTimeOffsetSeconds`（轨道面时间偏移）=`7.5 s`
- `plane0TimeOffsetSeconds`（仅 `plane 0` 额外物理相位偏移）=`-3.5 s`
- `alignmentReferenceTimeSeconds`（对齐参考时刻）=`6.5 s`
- `overpassGapSeconds`（主同轨过境间隔）=`6 s`
- `plane1OverpassGapSeconds`（用于保持 plane 1 初始诊断几何的同轨间隔）=`3 s`
- `simTime`（仿真时长）=`60 s`
- `updateIntervalMs`（主循环更新周期）=`100`
- `lambda`（业务流强度）=`250 pkt/s/UE`
- `maxSupportedUesPerSatellite`（每星默认容量口径）=`5`
- `hoHysteresisDb`（切换迟滞门限）=`2.0 dB`
- `hoTttMs`（切换触发时间）=`160 ms`
- `measurementReportIntervalMs`（测量上报周期）=`120 ms`
- `measurementMaxReportCells`（单次最多上报邻区数）=`8`
- `handoverMode`（切换模式）默认=`baseline`
- `improvedSignalWeight`（改进策略信号权重）=`0.7`
- `improvedRsrqWeight`（改进策略 `RSRQ` 权重）=`0.3`
- `improvedLoadWeight`（改进策略负载权重）=`0.3`
- `improvedVisibilityWeight`（改进策略可见性权重）=`0.2`
- `improvedMinLoadScoreDelta`（触发负载覆写所需的最小负载优势）=`0.2`
- `improvedMaxSignalGapDb`（允许负载覆写时相对最强信号候选的最大信号差）=`3.0 dB`
- `improvedMinStableLeadTimeSeconds`（同一目标联合领先后需持续保持的最小时间）=`0.12 s`
- `improvedMinVisibilitySeconds`（允许切入候选所需的最小剩余可见时间）=`1.0 s`
- `improvedVisibilityHorizonSeconds`（可见性评分归一化时间窗）=`8.0 s`
- `improvedVisibilityPredictionStepSeconds`（可见性向前预测步长）=`0.5 s`
- `improvedMinJointScoreMargin`（候选联合分数相对当前服务星至少需要高出的最小分差）=`0.03`
- `improvedMinCandidateRsrpDbm`（允许切入候选的最小 `RSRP`）=`-110 dBm`
- `improvedMinCandidateRsrqDb`（允许切入候选的最小 `RSRQ`）=`-17 dB`
- `improvedServingWeakRsrpDbm`（判定当前服务链路偏弱的 `RSRP` 门槛）=`-108 dBm`
- `improvedServingWeakRsrqDb`（判定当前服务链路偏弱的 `RSRQ` 门槛）=`-15 dB`
- `improvedEnableCrossLayerPhyAssist`（是否启用轻量跨层 PHY 辅助）默认=`false`
- `improvedCrossLayerPhyAlpha`（最近 PHY 指标平滑系数）=`0.02`
- `improvedCrossLayerTblerThreshold`（跨层判定弱链路的 `TBler` 门槛）=`0.48`
- `improvedCrossLayerSinrThresholdDb`（跨层判定弱链路的 `SINR` 门槛）=`-5 dB`
- `improvedCrossLayerMinSamples`（启用跨层判断前要求的最少 PHY 样本数）=`50`
- `pingPongWindowSeconds`（将 `A->B->A` 记为 `ping-pong` 的时间窗口）=`1.5 s`
- `shadowingEnabled`（是否启用 `ThreeGpp` 阴影衰落）=`true`
- `gnbAntennaRows/Columns`（真实 NR gNB 阵列规模）=`12x12`
- `ueAntennaRows/Columns`（真实 NR UE 阵列规模）=`1x2`
- `gnbAntennaElement`（gNB 阵列单元模型）默认=`b00-custom`（当前稳定默认；v4.3 引入；可选：`isotropic`, `three-gpp`）
- `ueAntennaElement`（UE 阵列单元模型）默认=`three-gpp`（当前稳定默认；v4.3 引入；可选：`isotropic`, `b00-custom`）
- `b00MaxGainDb`（b00-custom 阵元峰值增益）=`20.0 dBi`
- `b00BeamwidthDeg`（b00-custom 阵元方向图宽度参数）=`4.0°`
- `b00MaxAttenuationDb`（b00-custom 旁瓣衰减上限）=`30.0 dB`
- `beamformingMode`（阵列波束方法）默认=`ideal-earth-fixed`
- `earthFixedBeamTargetMode`（`ideal-earth-fixed` 的地面目标模式）默认=`grid-anchor`（可选：`nadir-continuous`）
- `beamformingPeriodicityMs`（ideal beamforming 更新周期）=`100`
- `realisticBfTriggerEvent`（realistic BF 触发方式）默认=`srs-count`
- `realisticBfUpdatePeriodicity`（realistic BF 更新周期，按 SRS 报告数计）=`3`
- `realisticBfUpdateDelayMs`（realistic BF 延迟更新）=`0`
- `phyDlTbIntervalSeconds`（PHY TB 分段统计窗口）默认=`1.0 s`
- `enablePhyDlTbTrace`（是否导出逐 TB 明细 trace）默认=`false`
- `phyDlTbTracePath`（逐 TB 明细 trace 路径）默认=`<outputDir>/phy_dl_tb_trace.csv`
- 当前固定载波口径：`2 GHz / 40 MHz / 1 CC / same-frequency`
- 当前固定 `NR` 下行调度器：`ofdma-rr`

切换口径：
- 当前算法 baseline 为传统 `A3` 风格切换
- baseline 与 improved 共用同一条 `MeasurementReport -> target selection -> TriggerHandover` 主链
- `handoverMode=baseline`：在 A3 上报候选中选择最强邻区
- `handoverMode=improved`：在同一批真实测量候选中联合考虑 `RSRP`、`RSRQ`、`remainingVisibility` 与 `loadScore` 选目标，并加入过载候选过滤、最小联合领先持续时间、最小剩余可见时间门控、源站负载感知的负载优势门槛，以及“候选联合分数必须明显优于当前服务星”的最小分差约束
- 当前 improved 还支持一层轻量弱链路保护：可对候选施加最小 `RSRP/RSRQ` 门槛，并在当前服务链路已经明显偏弱时，优先回到更保守的强信号切换
- 当前 improved 还保留一层可选的轻量跨层 PHY 辅助：当最近 PHY `TBler/SINR` 持续恶化时，可临时放宽部分等待门控
- 当前 baseline 不使用负载做决策，但运行时已保留负载观测字段；`loadScore` 本身采用更平滑的压力近似，避免少量 UE 时过早打满
- 当前 PHY 信道已开启 `ShadowingEnabled`，切换判决直接读取真实 PHY/RRC 测量
- v4.3 起，真实 NR PHY 默认天线单元改为 `gNB b00-custom + UE three-gpp`，阵列规模升为 `gNB 12x12`、`UE 1x2`，波束方法默认切为 `ideal-earth-fixed`
- 当前默认 `earthFixedBeamTargetMode=grid-anchor`：`gNB` 侧真实 PHY 发射波束锁到当前卫星已分配的唯一合法 anchor hex 中心，不跟着单个 `UE` 跑；`UE` 侧仍按当前服务卫星做 direct-path 接收对准
- 几何参数 `beamMaxGainDbi/theta3dBDeg/sideLobeAttenuationDb` 继续用于几何链路预算和真实接入/切换候选的主覆盖门控；当前默认 `grid-anchor` 下，`anchorCell` gate 会参与真实接入/切换候选过滤
- 当前默认 `b00BeamwidthDeg=4.0` 是按真实 PHY 总方向图收紧后的口径；配合 `gNB 12x12 UPA + b00-custom + ideal-earth-fixed` 后，默认物理口径更偏向收窄发射端空间泄漏，而不是单纯加大发射功率
- 当前 `8` 颗卫星默认共享同一个 `2 GHz / 40 MHz / 1 CC` operation band，因此高 `PHY DL TB error rate` 的下一步解释应优先考虑同频干扰；当前默认已通过外围 UE 二跳部署和波束锚点一圈排他降低同频波束过密。旧的 carrier-reuse / inter-frequency 诊断入口已经从活动工作区移除，不再作为当前 thesis 主线保留。
- 当前 baseline 固定使用 `ofdma-rr`；旧的 scheduler 对照入口已经从活动工作区移除。
- v4.3 已将默认天线切换为定向口径；历史 `isotropic` 仍可通过命令行参数切换，供对照与诊断使用
- 当前默认控制台输出已收紧为研究导向摘要；详细切换事件、执行时延和失败原因优先写入 `handover_event_trace.csv`
- 当前默认结果目录中还会额外导出 `e2e_flow_metrics.csv`，记录每个 `UE` 的下行业务端到端时延与丢包统计
- 当前默认结果目录中还会额外导出 `phy_dl_tb_metrics.csv`，记录每个 `UE` 的 PHY 下行 `TB` 统计，用于区分 PHY 误块与端到端丢包
- 当前默认结果目录中还会额外导出 `phy_dl_tb_interval_metrics.csv`，按时间窗汇总全局 PHY 下行 `TB` 统计，用于观察误块率随时间的波动
- 若显式设置 `enablePhyDlTbTrace=true`，还会导出 `phy_dl_tb_trace.csv`，逐 TB 记录 `MCS/TB size/RV/CQI/SINR/TBler/corrupt`，用于定位 PHY 误块集中在哪类传输事件中
- 原来的几何 `beam budget/custom A3` handover 代理链已经移除
- 当前默认关闭 `UE IPv4 forwarding`，避免异常下行包被 UE 误判为待转发上行包重新送回 `NAS`
- 当前保留 `forceRlcAmForEpc` 作为可选稳定性开关，但默认不覆盖 helper 的 `RLC` 映射

当前默认 UE 紧凑度：
- `hexCellRadiusKm`（小区 hex 半径）=`20`
- `anchorGridHexRadiusKm`（地面锚点网格 hex 半径）=`20`
- `ueCenterSpacingMeters`（中心 `3x3` 间距）=`6000`
- `ueRingPointOffsetMeters`（外围 `6` 小区内局部散点偏移）=`5000`
- `enforceBeamExclusionRing`（波束锚点是否启用一圈排他）=`true`
- `beamExclusionCandidateK`（启用排他时搜索的最近锚点候选数）=`64`
- `enforceBeamCoverageForRealLinks`（真实接入/切换候选是否强制落在当前真实波束主覆盖内）=`true`
- `enforceAnchorCellForRealLinks`（是否启用额外的锚点 hex cell 门控；仅 `earthFixedBeamTargetMode=grid-anchor` 时生效）=`true`
- `preferDemandAnchorCells`（锚点选择是否优先覆盖实际有 `UE` 的需求 hex cell）=`true`
- `anchorSelectionMode`（锚点选择模式）默认=`demand-max-ue-near-nadir`；可选=`demand-nearest`
- `demandSnapshotMode`（需求格子快照模式）默认=`runtime-underserved-ue`；可选=`static-layout`
- `anchorGridSwitchGuardMeters`（锚点切换最小距离优势）=`0`
- `anchorGridHysteresisSeconds`（锚点持续领先时间门控）=`0`

当前默认 UE 生成实现：
- 先在局部东-北平面生成 `seven-cell` 偏移模板
- 中心小区放置 `3x3` 密集簇，共 `9 UE`
- 六个外围二跳小区共放置 `16 UE`，按 `3/3/3/3/2/2` 分配并在各自小区中心附近散开；中心小区与外围 UE 之间保留一圈空白邻区
- 诊断口径下可显式设置 `ueLayoutType=r2-diagnostic --ueNum=19`：在当前 `seven-cell` 中心 hex 周围生成 `2-ring`、共 `19` 个 hex 中心点，角色分为 `r2-center / r2-ring1 / r2-ring2`
- 再统一将偏移模板转换为 `WGS84` 地理点和 `ECEF` 位置

当前锚点网格实验口径：
- 默认仍保持 `anchorGridHexRadiusKm = hexCellRadiusKm = 20 km`
- 默认启用波束锚点一圈排他：其它卫星不能把最终落点选到已占锚点或其一圈邻区内，避免同频波束过密或重叠
- 默认启用需求感知锚点选择：`anchorSelectionMode=demand-max-ue-near-nadir` 时，先取星下点最近格作为主落点；若主落点有 `UE` 且合法，则直接使用；若主落点无 `UE`，则只检查周围一圈邻格，只要存在合法且有 `UE` 的候选，就优先选择其中“运行时 demand 权重”最高的格子，若权重并列再按驻留 `UE` 数和“更接近星下点 + 更低扫描代价”打破平局
- 默认 `demandSnapshotMode=runtime-underserved-ue`：需求格子不再只按启动时静态 UE 布局统计，而是在每个更新周期按 UE 所在地面格子重建；若 UE 当前无服务、已偏离服务星锚点/主覆盖、服务星过载，或最近 PHY 已进入弱链路状态，则对应格子的 demand 权重会上调
- 若主落点和周围一圈都没有 `UE`，则允许回退到这个空主落点；但若周围一圈明明存在 `UE` 候选、却都因重复/邻占排他或 beam/scan 约束而不合法，则不会强行占用非法格子，而是继续走现有合法 fallback
- 这套 grid anchor 逻辑现在默认把真实 PHY 发射目标落到已分配唯一 hex 的中心；`nadir-continuous` 保留为旧口径对照
- 若需回到旧口径，可显式设置 `anchorSelectionMode=demand-nearest --demandSnapshotMode=static-layout`；其中 `demand-nearest` 会优先从实际有 `UE` 的 hex cell 中选择可扫描且未被排他的锚点，并按“更接近星下点 + 更低扫描代价”排序
- 当前还支持给锚点切换加入轻量门控：只有新锚点相对当前锚点具备足够距离优势，并持续领先至少一段时间，才更新当前 grid anchor 状态（legacy `cellAnchorEcef`）
- 若需单独诊断锚点平滑，可调整 `anchorGridSwitchGuardMeters`、`anchorGridHysteresisSeconds` 或关闭 `enforceBeamExclusionRing` 做对照

## 文档分工
- `docs/research-context.md`
  - 研究范围、目标、评估指标和稳定上下文
- `docs/current-task-memory.md`
  - 当前稳定节点、默认口径、已确认实现和近期工作边界
- `docs/joint-handover-strategy.md`
  - 后续“信号质量 + 可见性 + 卫星负载”联合策略的设计说明、变量映射与数学表达
- `docs/link-budget-parameters.md`
  - 当前链路预算口径与主要参数说明
- `docs/research-workflow.md`
  - 版本、分支、提交和结果管理规则
- `scratch/baseline-definition.md`
  - baseline 的正式定义、默认参数口径、验证清单和改进边界
- `scratch/midterm-report/README.md`
  - 中期材料清理后的归档说明

当前目录收纳约定：
- `scratch/leo-ntn-handover-baseline.cc`
  - 主仿真入口，继续留在 `scratch/` 根目录
- `scratch/handover/`
  - handover 主线相关辅助头文件
- `scratch/plotting/`
  - 分析与绘图脚本

## 开发与运行流程
这部分只回答一个问题：在当前 `ns-3.46 + contrib/nr + scratch/leo-ntn-handover-baseline.cc` 工作流里，什么情况下该 `configure`，什么情况下只要 `build`，什么情况下可以直接 `run --no-build`。

### 1. 推荐的两套工作模式
建议长期固定成两套模式，不要混着用：

- `debug` 模式
  - 用途：查崩溃、断言失败、异常日志、切换逻辑是否跑偏
  - 特点：更容易排障，但运行慢
- `optimized` 模式
  - 用途：正式跑 baseline、扫参数、导出结果、长时间仿真
  - 特点：明显更快，适合实验

建议默认使用：

- 开发排障时：`debug`
- 正式实验时：`optimized`

### 2. 什么情况下要重新 `configure`
只有当“构建配置”发生变化时，才需要重新 `configure`。

常见触发条件：

- 在 `debug` 和 `optimized/release` 之间切换
- 想打开或关闭 `tests/examples`
- 修改了 `CMake` 相关配置
- 更换编译器、生成器或构建目录
- 构建缓存明显异常，需要重建配置

常用命令：

```bash
./ns3 configure -d debug
./ns3 configure -d optimized --disable-tests --disable-examples
```

说明：

- `-d debug`：保留断言和日志，适合排障
- `-d optimized`：关闭大量调试成本并打开优化，适合正式仿真
- `--disable-tests --disable-examples`：主要减少编译负担，不是直接改变仿真逻辑

### 3. 什么情况下只要 `build`
如果你改了 `.cc`、`.h` 里的实现逻辑，但没有改构建档位或 `CMake` 配置，通常只要重新编译，不用再次 `configure`。

典型例子：

- 改了 `leo-ntn-handover-baseline.cc`
- 改了 `leo-ntn-handover-update.h`
- 改了 `leo-ntn-handover-runtime.h`
- 改了 `contrib/nr/` 下的 `C++` 代码
- 改了 `leo-ntn-handover-config.h` 中的默认参数值

常用命令：

```bash
./ns3 build
```

然后运行：

```bash
./ns3 run --no-build "leo-ntn-handover-baseline"
```

这里推荐带上 `--no-build`，原因很简单：既然刚刚已经成功 `build` 过，就不要在 `run` 时再隐式触发一轮构建。

### 4. 什么情况下可以直接 `run --no-build`
如果你没有改任何 `C++` 源码，只是想换一组运行参数，那么可以直接运行，不需要重新编译。

典型例子：

- 想把 `simTime` 从 `30` 改到 `10`
- 想把 `lambda` 从 `1000` 改到 `100`
- 想测试不同的 `hoTttMs`
- 想临时关闭 `runGridSvgScript`

常用命令：

```bash
./ns3 run --no-build "leo-ntn-handover-baseline --simTime=10 --lambda=100"
./ns3 run --no-build "leo-ntn-handover-baseline --hoTttMs=400 --hoHysteresisDb=3.0"
./ns3 run --no-build "leo-ntn-handover-baseline --handoverMode=improved --runGridSvgScript=0"
```

适用前提：

- 之前已经成功编译过当前构建档位
- 这次没有改 `C++` 代码

### 5. 面向当前仓库的推荐操作流程
下面这套流程足够覆盖大多数日常工作。

#### 大范围改代码之后
适用情况：

- 改了多个 `.cc/.h`
- 改了 `scratch/` 和 `contrib/nr/` 的实现
- 不确定缓存是否仍然干净

建议流程：

```bash
./ns3 configure -d optimized --disable-tests --disable-examples
./ns3 build
./ns3 run --no-build "leo-ntn-handover-baseline"
```

如果当前目的是查 bug，而不是跑正式结果，把第一步换成：

```bash
./ns3 configure -d debug
```

#### 小范围改逻辑之后
适用情况：

- 只改了少量 `C++` 逻辑
- 没有切换构建档位
- 没动 `CMake`

建议流程：

```bash
./ns3 build
./ns3 run --no-build "leo-ntn-handover-baseline"
```

#### 只微调参数之后
适用情况：

- 只改命令行参数
- 不改源码

建议流程：

```bash
./ns3 run --no-build "leo-ntn-handover-baseline --simTime=10 --lambda=100"
```

#### 正式实验批量跑参数时
适用情况：

- 想扫 `TTT`、`hysteresis`、`lambda`
- 想连续多次运行
- 想控制 wall-clock

建议流程：

```bash
./ns3 configure -d optimized --disable-tests --disable-examples
./ns3 build
./ns3 run --no-build "leo-ntn-handover-baseline --simTime=30 ..."
```

原则：

- 用 `optimized`
- 尽量复用同一轮编译结果
- 运行阶段优先使用 `--no-build`

### 6. 构建缓存异常时怎么处理
如果你遇到下面这些症状，优先怀疑缓存或构建状态有问题：

- 明明改了代码，运行结果像没生效
- 构建目标名或库链接状态异常
- 切换过 `debug/optimized` 后行为很混乱
- `build` 或 `run` 反复出现难以解释的旧错误

## 当前正式实验入口
当前活动工作区只保留论文主线的正式对照入口：

| ID | 目标 | 关键参数 | 主要用途 |
| --- | --- | --- | --- |
| `B00` | baseline 对照组 | `handoverMode=baseline hoTttMs=160 hoHysteresisDb=2.0` | 正式 baseline |
| `I31` | improved 参考组 | `handoverMode=improved` + `stableLead=0.12 s` + `jointScoreMargin=0.03` | 当前 improved 参考点 |

当前批量脚本只保留这两组：

```bash
scratch/run_handover_experiment_matrix.sh --list
scratch/run_handover_experiment_matrix.sh --group formal-compare --repeat 3 --rng-run-start 11
```

建议记录：
- 版本
- 运行命令
- `RngRun`
- `E2E delay`
- `packet loss rate`
- `SINR`
- `ping-pong`
- load balance 结论

建议流程：

```bash
./ns3 clean
./ns3 configure -d optimized --disable-tests --disable-examples
./ns3 build
```

如果当前是深度排障，再改为：

```bash
./ns3 configure -d debug
```

### 7. 为什么正式实验更建议用 `optimized`
当前 baseline 默认就是高事件量场景：

- `25 UE`
- `lambda = 250 pkt/s/UE`
- `updateIntervalMs = 100`
- 默认走 `MeasurementReport` 驱动的 `A3` 触发链

因此：

- `debug` 更适合查错
- `optimized` 更适合出结果

如果使用 `debug` 跑长时间 baseline，wall-clock 明显变慢是正常现象，不要把这种放慢直接误判成算法回归。

### 8. VS Code 里怎么用更顺手
当前工作区已经有 `.vscode/tasks.json`，其中比较实用的是：

- `ns3: build`
- `ns3: run target`
- `ns3: show targets`

因此在 VS Code 中，推荐优先使用：

- Terminal 直接执行 `./ns3 ...`
- 或者用现成的 `Tasks`

原因：

- `./ns3` 是 ns-3 官方 wrapper，最懂当前目标名和参数传递方式
- 对 `scratch` 程序来说，`./ns3 run "target --args"` 通常比直接找底层二进制路径更稳

### 9. CMake Tools 的 `Run in Terminal` 什么时候可用
如果你安装了 `CMake Tools` 扩展，`Run in Terminal` 通常要在下面条件满足后才会变得稳定可用：

- 工作区已经成功 `Configure`
- CMake 已经识别到一个可执行 `launch target`
- 当前不是只选中了一个库目标

如果它不可用，常见原因通常是：

- 还没成功 `configure`
- 当前没有选中可执行目标
- CMake Tools 还没识别到 `launch target`

对这个仓库，判断标准可以简单记成：

- 只要 `./ns3 build` 和 `./ns3 run ...` 已经正常工作，就不必强依赖 `Run in Terminal`
- `Run in Terminal` 更像 IDE 便捷入口，不是这个仓库的主工作流

### 10. 最推荐记住的 6 条命令
调试版重新配置：

```bash
./ns3 configure -d debug
```

实验版重新配置：

```bash
./ns3 configure -d optimized --disable-tests --disable-examples
```

重新编译：

```bash
./ns3 build
```

运行默认 baseline：

```bash
./ns3 run --no-build "leo-ntn-handover-baseline"
```

运行一组临时参数：

```bash
./ns3 run --no-build "leo-ntn-handover-baseline --simTime=10 --lambda=100"
```

缓存异常时重来一轮：

```bash
./ns3 clean
./ns3 configure -d optimized --disable-tests --disable-examples
./ns3 build
```

## 当前代码组织
- `leo-ntn-handover-baseline.cc`
  - 场景搭建、主流程控制和仿真运行入口
- `leo-ntn-handover-config.h`
  - 默认参数、命令行参数注册、路径收口和参数合法性检查
- `leo-ntn-handover-runtime.h`
  - 卫星和 UE 运行时状态，以及 UE 布局偏移模板到地理坐标的生成逻辑
- `leo-ntn-handover-update.h`
  - 周期更新、观测、邻区维护和切换驱动
- `leo-ntn-handover-reporting.h`
  - 最终统计与结果汇总输出
- `leo-ntn-handover-utils.h`
  - 通用辅助函数

## 日志与结果
日志偏好：
- 默认只保留周期性进度和研究导向的最终摘要
- 单次切换开始/结束、服务星变化和详细 setup 信息默认不再打印到控制台
- 详细切换事件与吞吐恢复信息优先通过 `handover_event_trace.csv`、`handover_dl_throughput_trace.csv` 离线分析
- 默认不强调 `OVERPASS`、`GRID-ANCHOR` 等高噪声信息

结果目录：
- 当前默认仿真输出统一写入 `scratch/results/`
- 常见结果包括：
  - `hex_grid_cells.csv`
  - `hex_grid_cells.svg`（当 `runGridSvgScript = true`）
  - `hex_grid_cells.html`（当 `runGridSvgScript = true`）
  - `ue_layout.csv`
  - `sat_anchor_trace.csv`
  - `sat_ground_track.csv`
  - `handover_dl_throughput_trace.csv`
  - `handover_event_trace.csv`

分析脚本：
- `plotting/plot_hex_grid_svg.py`
  - 读取六边形网格 `CSV`
  - 生成对应 `SVG` 与交互式 `HTML`
  - 默认会尝试读取同目录下的 `ue_layout.csv`，用于导出 `grid + UE` 视图
  - 当前支持叠加 `sat_anchor_trace.csv`，用于导出“最终波束落点轨迹”
  - 当前支持叠加 `sat_ground_track.csv`，用于导出 `8` 颗卫星的真实连续地面轨迹
  - 可选使用 `--plane-north-offsets-m "0:-12000"` 这类参数，仅在绘图层对指定轨道面的轨迹做南北方向显示偏移；它不会改动仿真几何、`UE` 位置或六边形网格本身
  - `hex_grid_cells.html` 默认支持 `8` 星总览、单星筛选、`UE` 分布叠加、真实轨迹/波束落点双层显示与时间滑块检查
- `plotting/plot_handover_throughput.py`
  - 默认读取 `scratch/results/handover_dl_throughput_trace.csv`
  - 默认读取 `scratch/results/handover_event_trace.csv`
  - 面向单个 `UE` 的单次切换窗口，输出 `HO Start / HO Success` 对齐的吞吐连续性图

## 当前已完成的关键收口
- 主脚本与运行时、统计、工具辅助头文件的拆分已经完成
- 切换相关实时日志与最终汇总格式已经完成一轮收敛
- 当前已去掉旧的几何 `custom A3` handover 代理链，baseline 与 improved 统一走 `MeasurementReport`
- 当前已将周期更新中的卫星公共轨道传播与 handover 判决主链彻底分开
- 当前已将默认参数和合法性检查集中到 `leo-ntn-handover-config.h`
- 当前已接入 `loadScore`（负载评分）相关运行时字段与逐时刻 trace 输出
- 当前已将默认 `UE` 布局切换为带一圈空白邻区的 `seven-cell`，并补齐导出 `CSV/SVG` 的脚本链

## 接下来
- 先用当前默认参数完成一轮带一圈空白邻区的 `seven-cell baseline` 测量驱动切换验证
- 在相同 `MeasurementReport` 入口下，对比 baseline 与 improved 的目标选择差异
- 最后再根据实验结果细化联合目标函数和负载指标

## 维护规则
- 修改 `scratch/` 目录下的重要代码后，同步检查：
  - `scratch/README.md`
  - `scratch/baseline-definition.md`
  - `scratch/midterm-report/README.md`
- 文档优先写清当前口径和决策，不堆叠过长历史说明
- 当文档中提到关键参数时，优先采用“英文参数名（中文释义）”写法
- 版本、分支和提交命名规则以 `docs/research-workflow.md` 为准

## 版本演进记录
本节保留研究过程中的主要版本收口信息。写法上不再逐条堆叠零散改动，而是按版本归档关键变化，便于后续回看“什么时候改了什么、为什么改”。
本节中的旧参数、旧输出文件或旧脚本名称仅用于历史追溯，不代表当前默认实现。

### `2.5` 之前的基础工作
- 三维物理建模底座
  - 建立 `WGS84` 地理锚定，用经纬高描述地面 `UE` 位置
  - 梳理 `ECI -> ECEF` 转换，把地球自转纳入卫星相对地面的几何计算
  - 建立斜距、仰角、方位角、多普勒等关键星地几何量的计算框架
- 轨道可控性设计
  - 引入轨道对齐与过顶时序控制思路
  - 为后续多星过境和切换事件提供可解释、可重复的几何组织
- `Earth-fixed` 覆盖设计
  - 明确“固定地面区域 + 运动卫星波束服务”的建模思路
  - 梳理波束与小区的关系，使切换分析具有网络规划语义
- 局部地面网格与锚点机制
  - 建立基于 `WGS84` 的局部六边形网格思路
  - 为后续地面锚点选择、覆盖映射和切换边界观察提供基础

### `2.5`
- 初始版本
  - 记录了毕设基础组的总体方向
  - 记录了当时“目标是 `3 UE`，但代码仍是单 UE”的差异
  - 建立了日志与维护规则
- 多 UE 基础组更新
  - 将 `leo-ntn-handover-baseline.cc` 从单 UE 路径改为可配置的多 UE 基线路径
  - 将基础组默认配置设为 `5` 颗卫星、`3` 个 UE
  - 默认关闭高噪声的 `KPI`、`OVERPASS`、`GRID-ANCHOR` 日志
  - 收紧当时的逐时刻导出格式，使多 UE 结果更便于分析
- 切换日志与汇总优化
  - 默认切换日志改为以 `ue` 序号作为主标识，不再强调原始 `IMSI/RNTI`
  - 切换完成日志增加执行时延显示
  - 最终切换汇总改为输出总切换次数、成功率和平均执行时延
- 主脚本结构整理
  - 新增 `leo-ntn-handover-runtime.h`
  - 将运行时结构体和通用辅助函数移到外部头文件
  - 让主脚本更聚焦于仿真流程、场景搭建和结果输出
- 统计输出与头文件注释整理
  - 新增 `leo-ntn-handover-reporting.h`
  - 将最终吞吐、几何校验和切换汇总输出从主脚本抽到独立头文件
  - 为自定义头文件补充中文文件头说明、结构体注释和函数注释
- 结果目录整理
  - 默认将仿真输出写入 `scratch/results/`
  - 运行前自动创建结果目录，避免输出散落在仓库根目录
  - 当时的分析脚本默认输入输出路径同步迁移到 `scratch/results/`

### `3.0`
- 研究版本规则建立
  - 明确区分研究版本 `3.0` 与 ns-3 框架版本 `3.46`
  - 提交、分支和 `tag` 规则统一收敛到 `docs/research-workflow.md`
- 阶段重点切换
  - 明确文件整理和切换日志收敛已完成当前阶段目标
  - 将后续主任务切换为扩大星座规模和验证更大场景下的切换行为
- 双轨星座扩展起步
  - 将基础组默认配置提升为 `8` 颗卫星、`2` 个轨道面、`6` 个 UE
  - 新增双轨道面参数，用于控制 `RAAN` 间隔和轨道面间过境时序偏移
- 双轨切换事件整定
  - 保持 `2x4` 双轨规模不变，先不引入二维 UE 分布
  - 通过减小 `RAAN` 间隔、轨间时序偏移和同轨过境间隔增强候选星重叠
  - 通过拉长仿真时长提高观察到更多切换事件的概率
- 运行进度与性能优化起步
  - 新增周期性仿真进度输出
  - 一度将 `updateIntervalMs` 调整为 `200 ms`、业务流强度从 `1000` 下调为 `250 pkt/s/UE`
  - 默认关闭波束跟踪 `CSV` 写出和衰减后处理脚本，降低调参开销
- 几何共享计算优化
  - 将 `UpdateConstellation()` 中重复的轨道传播提取为共享公共状态
  - 在每个时间步先统一计算卫星 `ECEF/速度`，再按 UE 派生斜距、仰角、方位角和多普勒

### `3.0.1`
- 基线稳定性与性能收敛
  - 纳入 `NR scheduler` 侧陈旧 `HARQ/CQI` 反馈防御，解决切换后崩溃问题
  - 纳入周期性仿真进度输出与第一轮工程性能优化
- 毕设主线收口
  - 明确当前 `2x4` 双轨场景的定位是传统 `A3 baseline` 缺陷暴露平台
  - 明确后续主线从“继续扩星”切换为“baseline 验证 + 改进策略设计”
  - 明确后续改进算法需要对齐任务书中的“信号质量 + 卫星负载”要求

### `3.1.0`
- baseline 观测与输出收口
  - 纳入 `loadScore`（负载评分）相关运行时字段和逐时刻 `trace` 导出
  - 收口当时的辅助导出脚本，使结果更适合阶段性分析
  - 保留周期性仿真进度输出，便于长时间运行时观察仿真推进情况

### `3.1.1`
- baseline 暴露性增强第一步
  - 完成从单轨局部过境到双轨起步场景的扩展
  - 明确当前不再把继续扩星作为主任务，而是优先面向毕设任务书定义 baseline 与改进策略

### `3.1.2`
- UE 场景代表性增强
  - 默认 UE 部署从一维线性排布切换为 `25 UE` 的 `hotspot-boundary` 二维布局
  - 场景按 `9` 个热点 UE、`10` 个边界 UE、`6` 个背景 UE 组织
  - 保留 `line` 布局作为对照，但默认优先服务负载不均衡与 `ping-pong` 研究
  - 默认将 `interPlaneRaanSpacingDeg`（轨道面 `RAAN` 间隔）从 `8 deg` 下调到 `6 deg`
  - 当前这一步只增强跨轨空间重叠，不改 `simTime`、`hoTttMs`、`hoHysteresisDb`

### `3.1.3`
- 默认口径收口
  - 明确当前默认 UE 主场景仍为 `25 UE` 的 `hotspot-boundary` 二维布局
  - 明确保留 `line` 布局作为对照入口，但默认研究场景优先使用 `hotspot-boundary`
  - 明确当前代码默认值为 `updateIntervalMs = 100`、`lambda = 250 pkt/s/UE`、`maxSupportedUesPerSatellite = 5`
  - 明确当时默认会生成额外的候选轨迹与中间分析文件
  - 明确可在 `runGridSvgScript = true` 时继续生成 `hex_grid_cells.svg`，并额外导出 `hex_grid_cells.html`
  - 同步收口当前崩溃防御链说明，补齐 `SN Status Transfer`、`NrPdcp::DoReceivePdu()`、`UdpServer::HandleRead()` 的描述
- 配置入口收口
  - 新增 `leo-ntn-handover-config.h`
  - 将默认参数、命令行参数注册、输出路径收口和参数合法性检查从主脚本抽离
  - 本次调整不改变 `2x4` 双轨、`25 UE`、传统 `A3 baseline` 的场景口径
- 减法式清理
  - 去掉单 UE 兼容触发链，统一多 UE 主执行路径
  - 删除若干只写不读状态，收紧运行时样板代码
  - 将卫星公共轨道状态统一为一次计算，`UE` 侧观测改为基于公共 `ECEF/速度` 派生
  - 将 `lockCellAnchorToUe` 和 `lockGridCenterToUe` 的派生逻辑收口到配置层
  - `main()` 改为直接消费 `BaselineSimulationConfig`

### `3.2.0`
- 首次稳定节点收口
  - 将当前研究仓库稳定节点提升为 `3.2.0`
  - 收口 `docs/`、`scratch/` 和 `midterm-report/` 的版本与 baseline 口径
  - 保留版本演进历史，同时重组 README 结构，减少文档冗余

### `3.2.1`
- 稳定节点
  - 对应 tag：`research-v3.2.1`
  - 对应提交：`c692e68 chore(v3.2.1): snapshot tightened baseline and joint strategy docs`
  - 重点是收紧 baseline 与联合策略文档口径，而不是再定义一个新的场景版本

### `3.3.0`
- 最近已发布稳定节点
  - 对应 tag：`research-v3.3.0`
  - 当前保持 `3.2.2` baseline 语义与默认参数不变
  - 将主脚本保留在 `scratch/` 根目录，辅助头文件集中到 `scratch/handover/`
  - 将分析与绘图脚本集中到 `scratch/plotting/`
  - 清理历史结果、缓存、示例 scratch 目录与中期阶段性派生资产

### `4.1`
  - 对应 tag：`research-v4.1`
  - 对应提交：本次 `4.1` 发布提交
  - 当前正式收口“信号 + 可见性 + 负载”联合策略与源站负载感知负载均衡主线
  - improved 目标选择支持剩余可见时间门控、平滑负载压力与源站负载导向卸载
  - 将 `docs/`、`scratch/`、`baseline-definition` 与 `midterm-report/` 的当前口径同步到 `4.1`

### `4.2`
  - 对应 tag：`research-v4.2`
  - 对应提交：本次 `4.2` 发布提交
  - 当前正式收口新默认几何：`RAAN=-0.58 deg`、`plane0RaanOffset=1.09 deg`、`planeTimeOffset=7.5 s`、`plane0TimeOffset=0 s`、`alignmentReferenceTime=6.5 s`、`overpassGap=3 s`
  - 当前正式收口中等负载默认口径：`lambda=250 pkt/s/UE`、`maxSupportedUesPerSatellite=5`
  - 当前 improved 默认起点切到 `I31`：`stableLead=0.12 s`、`jointScoreMargin=0.03`
  - 当前后续实验口径固定使用 `B00` 作为 baseline 主对照组
  - 修复关闭吞吐 trace 时 `handover_event_trace.csv` 不再输出的问题
  - 将 `docs/`、`scratch/`、`baseline-definition` 与 `midterm-report/` 的当前口径同步到 `4.2`

### `4.3`
  - 对应 tag：`research-v4.3`
  - 当前正式收口定向天线默认口径：`gnbAntennaElement=b00-custom`，`ueAntennaElement=three-gpp`
  - 新增 b00-custom 参数暴露：`b00MaxGainDb=20.0`，`b00BeamwidthDeg=4.0`，`b00MaxAttenuationDb=30.0`
  - 几何参数（`beamMaxGainDbi/theta3dBDeg/sideLobeAttenuationDb`）保留用于链路预算，但不代表 PHY 默认天线阵元参数
  - smoke 验证表明：`B00-V43` SINR 显著改善（约 +10 dB vs LEGACY-ISO），证明定向性生效
  - 同步 `docs/`、`scratch/`、`baseline-definition` 与 `midterm-report/` 的当前口径到 `4.3`

### `5.0`
  - 对应 tag：`research-v5.0`
  - 在 `research-v4.3` 定向天线默认口径基础上，完成 thesis-mainline 清理和正式收口
  - 主线只保留 same-frequency baseline / improved、`B00/I31` 正式对照和 `19 UE r2-diagnostic` 诊断入口
  - 移除 inter-frequency、carrier-reuse、scheduler 对照、beamwidth/sidelobe/plane-offset/overpass-gap 扫描等历史诊断入口
  - 补齐 `grid-anchor`、`earth-fixed beam target`、`plane0 offset`、`r2-diagnostic layout` 和 plotting report 的 focused tests
  - 清理中期材料、代理协作文档和结果区临时残留，并同步 `docs/`、`scratch/`、`baseline-definition` 与 `midterm-report/` 口径

### `4.0.1`
  - 当前稳定节点
  - 对应 tag：`research-v4.0.1`
  - 对应提交：本次 `4.0.1` 发布提交
  - 当前正式收口 measurement-driven baseline / improved 对照主线
  - 统一使用标准 `PHY/RRC MeasurementReport` 作为 handover 候选入口
  - 移除旧的几何 `beam budget/custom A3` handover 代理链及其派生 beam trace/report 输出
  - 周期更新主循环收紧为轨道推进、服务关系观测与负载统计
  - 将 `TTT` 归一化和 debounce 语义对齐到当前标准 A3 配置
  - 同步 `docs/`、`scratch/`、`baseline-definition` 与 `midterm-report/` 的版本和主线口径

## `4.1` 已发布包
- `research-v4.1` 代表当前 measurement-driven baseline / improved 主线下的最新稳定收口快照，重点是把 improved 收紧为“信号 + 可见性 + 负载”的联合策略，并增强源站负载感知的负载均衡能力

`v4.1` 已纳入的内容：
- `baseline` 与 `improved` 继续共用 `MeasurementReport -> target selection -> TriggerHandover` 主链
- `improved` 当前同时使用 `signal`、`remainingVisibility`、`loadScore` 做联合目标选择
- `loadScore` 从单纯人数比提升为“线性容量比 + 平滑饱和”的平滑压力分数
- 源站负载压力会动态增强负载项、放宽负载覆写门槛，并奖励能够减轻源站压力的候选
- 同步 `docs/`、`scratch/` 与中期汇报文档的版本和主线口径

## `4.2` 已发布包
- `research-v4.2` 代表当前 measurement-driven baseline / improved 主线下的新稳定收口快照，重点是稳定双轨竞争几何、把业务口径收回到中等负载，并为后续以 `B00` 为对照的 improved 细化提供固定场景口径

`v4.2` 已纳入的内容：
- 默认几何调整为 `interPlaneRaanSpacingDeg=-0.58 deg`、`plane0RaanOffsetDeg=1.09 deg`、`interPlaneTimeOffsetSeconds=7.5 s`、`plane0TimeOffsetSeconds=0 s`、`alignmentReferenceTimeSeconds=6.5 s`、`overpassGapSeconds=3 s`
- 默认业务与容量口径调整为 `lambda=250 pkt/s/UE`、`maxSupportedUesPerSatellite=5`
- improved 默认参数调整为 `improvedMinStableLeadTimeSeconds=0.12 s`、`improvedMinJointScoreMargin=0.03`
- 当前后续实验默认使用 `B00` 作为 baseline 主对照组
- `run_handover_experiment_matrix.sh` 默认结果路径推进到 `scratch/results/exp/v4.2`，并加入围绕 `I31` 的优化矩阵入口
- `handover_event_trace.csv` 输出链从吞吐 trace 开关中解耦，避免关闭吞吐 trace 时丢失事件 trace

## `4.0.1` 已发布包
- `research-v4.0.1` 代表当前 measurement-driven baseline / improved 主线的第一版稳定收口快照，重点是统一测量驱动切换入口、清理旧代理链，并把文档口径同步到同一条研究主线上

`v4.0.1` 已纳入的内容：
- `baseline` 与 `improved` 共用 `MeasurementReport -> target selection -> TriggerHandover` 主链
- `NrLeoA3MeasurementHandoverAlgorithm` 暴露标准测量回调，并支持场景侧自定义目标选择
- 删除 `sat_beam_trace.csv`、`sat_beam_report.csv` 及其派生脚本，保留更贴近当前主线的事件与吞吐输出
- 周期更新中保留每星 `attachedUeCount`、`offeredPacketRate`、`loadScore`、`admissionAllowed` 统计
- 同步 `docs/`、`scratch/` 与中期汇报文档的路径、参数和版本口径

`v4.0.1` 发布前检查已收口为：
- 默认参数下跑通一轮 `seven-cell baseline`
- 确认 `scratch/results/` 输出链完整
- 确认 baseline 文档、README 与中期汇报技术总结的参数口径一致
- 确认 measurement-driven baseline / improved 的当前代码与文档描述一致

当前若需要回溯已清理的中期材料，只保留：
- `scratch/midterm-report/README.md`
