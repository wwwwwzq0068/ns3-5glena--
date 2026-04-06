# 当前任务记忆

## 当前版本状态
- 最近已发布稳定节点：`4.1`（Git tag：`research-v4.1`）
- 当前工作区已收口到 `4.1` 快照，底层框架仍为 `ns-3.46`
- 当前主仿真入口：`scratch/leo-ntn-handover-baseline.cc`

## 当前 baseline 快照
- 当前默认场景：`2x4` 双轨、`25 UE`、`seven-cell` 二维部署
- 当前 baseline：传统 `A3` 风格切换，但触发入口已经统一到标准 `PHY/RRC MeasurementReport`
- 当前 improved：复用同一批真实测量候选，在目标选择时联合考虑 `signal`、`remainingVisibility` 与 `loadScore`，并逐步加入回切保护、可见性门控与源站负载感知的负载覆写门槛
- 原来的几何 `beam budget/custom A3` handover 代理链已从主线移除；几何计算只保留给轨道推进、地面锚点与初始接入

当前默认关键参数：
- `gNbNum = 8`
- `orbitPlaneCount = 2`
- `ueNum = 25`
- `ueLayoutType = seven-cell`
- `hexCellRadiusKm = 20`
- `ueCenterSpacingMeters = 6000`
- `ueRingPointOffsetMeters = 5000`
- `interPlaneRaanSpacingDeg = -2 deg`
- `interPlaneTimeOffsetSeconds = 0.0 s`
- `alignmentReferenceTimeSeconds = 20 s`
- `overpassGapSeconds = 2 s`
- `updateIntervalMs = 100 ms`
- `lambda = 1000 pkt/s/UE`
- `hoHysteresisDb = 2.0 dB`
- `hoTttMs = 200 ms`
- `measurementReportIntervalMs = 120 ms`
- `measurementMaxReportCells = 8`
- `handoverMode = baseline`
- `improvedSignalWeight = 0.7`
- `improvedLoadWeight = 0.3`
- `improvedVisibilityWeight = 0.2`
- `improvedMinLoadScoreDelta = 0.2`
- `improvedMaxSignalGapDb = 3.0 dB`
- `improvedReturnGuardSeconds = 1.5 s`
- `improvedMinVisibilitySeconds = 1.0 s`
- `improvedVisibilityHorizonSeconds = 8.0 s`
- `improvedVisibilityPredictionStepSeconds = 0.5 s`
- `pingPongWindowSeconds = 1.5 s`
- `forceRlcAmForEpc = false`
- `disableUeIpv4Forwarding = true`

## 当前已确认实现
- 主脚本与运行时、统计、工具辅助头文件的拆分已经完成
- 当前 `UE` 位置生成逻辑已收口为“两阶段”实现：先生成局部东-北平面偏移模板，再统一转换为 `WGS84` 地理点和 `ECEF`
- 当前默认 `UE` 布局为 `seven-cell`：中心小区 `3x3` 密集簇 `9 UE`，外围 `6` 个相邻小区共 `16 UE`
- 当前 `NrChannelHelper` 已配置 `NTN-Rural + LOS + ThreeGpp`，并开启 `ShadowingEnabled`
- baseline 与 improved 都通过 `NrLeoA3MeasurementHandoverAlgorithm` 注册标准 A3 测量，并在同一份 `MeasurementReport` 上做目标选择
- `handoverMode = baseline` 时，直接选择最强测量邻区
- `handoverMode = improved` 时，直接在同一批测量邻区上按“信号质量 + 可见性效用 + 负载效用”联合打分，并对过载候选、短时回切、最小剩余可见时间和过小负载优势做额外门控
- 周期更新中已经会计算每星 `attachedUeCount`、`offeredPacketRate`、`loadScore`、`admissionAllowed`
- 当前默认关闭高噪声的 `KPI`、`GRID-ANCHOR` 输出，保留切换、进度和最终汇总日志
- 当前已支持按成功切换序列自动统计短时 `ping-pong`
- 当前默认关闭 `SRS` 调度相关项，避免与 handover 主线无关的 `PHY fatal`

## 当前负载接口状态
- `SatelliteRuntime` 已具备 `attachedUeCount`、`offeredPacketRate`、`loadScore`、`admissionAllowed`
- `loadScore` 当前使用更平滑的容量压力近似，避免负载在少量 UE 时过早饱和
- 这些量已进入 `handoverMode = improved` 的目标选择，且 improved 会根据源站负载压力动态偏向轻载目标
- baseline 仍不使用负载做决策，因此对照边界保持清楚

## 当前输出与脚本
- 默认结果目录：`scratch/results/`
- 当前默认输出：
  - `hex_grid_cells.csv`
  - `hex_grid_cells.svg`（当 `runGridSvgScript = true`）
  - `ue_layout.csv`
  - `sat_anchor_trace.csv`
  - `handover_dl_throughput_trace.csv`
  - `handover_event_trace.csv`
- 当前常用分析脚本：
  - `scratch/plotting/plot_hex_grid_svg.py`

## 当前研究问题
- 继续验证传统 `A3 baseline` 在当前双轨场景下是否能稳定暴露：
  - 频繁切换
  - 潜在 `ping-pong`
  - 触发过早或过晚
  - 吞吐连续性问题
- 继续核实当前 `2x4` 双轨场景中第二轨是否真正参与竞争，避免把“已建成双轨场景”误写成“已形成充分双轨竞争”
- 区分 `hex grid`（地面锚点/服务区域目录）与真实 `cell`（协议栈中的卫星小区）语义，避免把 `hex` 数量直接等同为可接入小区数量
- 评估当前外围 `6` 个小区中的 `UE` 分布是否足够均衡，是否已经形成可观察的跨小区竞争
- 在保持当前场景口径不变的前提下，继续验证 improved 的可见性与负载权重是否真的改善切换目标选择

## 当前工作边界
- 当前不把“继续扩大星座规模”作为默认主线，除非现有 `2x4` 场景已无法体现算法差异
- 当前不再把“代码结构整理”作为第一优先级，除非算法实现需要新的结构调整
- 当前 baseline 的 `UE` 场景口径已经固定为 `seven-cell`
- 修改 `scratch/` 目录下的重要代码后，同步检查：
  - `scratch/README.md`
  - `scratch/baseline-definition.md`
  - 相关 `midterm-report` 文档

## 当前优先方向
- 稳住 `2x4` 双轨、`25 UE`、`seven-cell` baseline 场景
- 在统一 `MeasurementReport` 入口下完成 baseline 与 improved 对照验证
- 优先围绕 handover success rate、handover delay、throughput continuity、ping-pong 和 load balance 做结果解释

## 当前汇报准备入口
- 版本收口说明优先参考本文件和 `scratch/README.md`
- 配图说明优先参考 `scratch/midterm-report/midterm-image-generation-spec.md`
- PPT 页次与配图对应优先参考 `scratch/midterm-report/midterm-ppt-design.md`
