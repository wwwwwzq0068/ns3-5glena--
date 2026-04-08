# 当前任务记忆

## 当前版本状态
- 最近已发布稳定节点：`4.1.1`（Git tag：`research-v4.1.1`）
- 当前工作区已收口到 `4.1.1` 快照，底层框架仍为 `ns-3.46`
- 当前主仿真入口：`scratch/leo-ntn-handover-baseline.cc`

## 发布日志（2026-04-08）
- 发布版本：`research-v4.1.1`
- 发布定位：仍属于 `4.1` 主阶段内的稳定节点，不单独开启 `4.2`
- 本次收口重点：
  - 将主脚本进一步收紧为场景装配入口，并把目标选择与场景装配继续拆到独立头文件
  - 补齐 measurement-driven handover 的准备阶段、准入门控、失败事件与统计口径说明
  - 对齐 `README`、baseline 定义、联合策略与中期材料中的参数和术语口径
  - 清理文档中重复展开的默认参数表，同时保留实验记录登记和常用命令入口

## 当前 baseline 快照
- 当前默认场景：`2x4` 双轨、`25 UE`、`seven-cell` 二维部署
- 当前 baseline：传统 `A3` 风格切换，但触发入口已经统一到标准 `PHY/RRC MeasurementReport`
- 当前 improved：复用同一批真实测量候选，在目标选择时联合考虑 `signal`、`remainingVisibility` 与 `loadScore`，并逐步加入回切保护、可见性门控与源站负载感知的负载覆写门槛；当前还会在目标选择阶段直接剔除 `admissionAllowed=false` 的拥堵候选，若无可接纳目标则放弃本轮切换
- 原来的几何 `beam budget/custom A3` handover 代理链已从主线移除；几何计算只保留给轨道推进、地面锚点与初始接入

当前默认参数管理方式：
- 精确默认参数以 `scratch/baseline-definition.md` 和 `scratch/handover/leo-ntn-handover-config.h` 为准，本文件不再重复维护整张默认参数表
- 当前最常用的研究口径仍是：`2x4` 双轨、`25 UE`、`seven-cell`、`handoverMode=baseline`
- 当前高频核对项仍是：`hoTttMs = 200 ms`、`measurementReportIntervalMs = 120 ms`、`improvedReturnGuardSeconds = 0.5 s`、`pingPongWindowSeconds = 1.5 s`
- 当前默认稳定运行口径仍是：`useIdealRrc = true`、`enableDynamicHoPreparation = true`、地面锚点固定为 `WGS84 hex-grid`

## 当前已确认实现
- 主脚本与运行时、统计、工具辅助头文件的拆分已经完成
- 当前 `UE` 位置生成逻辑已收口为“两阶段”实现：先生成局部东-北平面偏移模板，再统一转换为 `WGS84` 地理点和 `ECEF`
- 当前默认 `UE` 布局为 `seven-cell`：中心小区 `3x3` 密集簇 `9 UE`，外围 `6` 个相邻小区共 `16 UE`
- 当前 `NrChannelHelper` 已配置 `NTN-Rural + LOS + ThreeGpp`，并开启 `ShadowingEnabled`
- baseline 与 improved 都通过 `NrLeoA3MeasurementHandoverAlgorithm` 注册标准 A3 测量，并在同一份 `MeasurementReport` 上做目标选择
- `handoverMode = baseline` 时，直接选择最强测量邻区
- `handoverMode = improved` 时，直接在同一批测量邻区上按“信号质量 + 可见性效用 + 负载效用”联合打分，并对过载候选、短时回切、最小剩余可见时间和过小负载优势做额外门控；若当前无可接纳目标，则本轮不发起切换准备
- 当前默认稳定口径仍使用 `ideal RRC`，但 `S1-U/S11/S5/remote-host/X2` 已显式注入非零控制面/回传时延
- 当前 `useIdealRrc=0` 已可稳定跑通，但默认会先用一次 `ideal bootstrap transport` 完成首轮 initial attach，再切回 real RRC transport；这是因为纯 real-RRC 初始接入在当前 NTN 上行链路预算下会卡在 `Msg3 / RRC Connection Request`
- 当前 measurement-driven handover 已显式加入 `HO preparation` 阶段；其附加时延由 `X2` 几何传播、目标斜距、目标负载和低仰角惩罚共同决定
- 当前目标侧 `AdmitHandoverRequest` 只在 `handoverMode = improved` 时与运行时 `admissionAllowed` 动态绑定；baseline 保持纯信号驱动 `A3` 执行，不再被 admission 阻塞
- 当前 `HO execution delay` 已改为从 measurement-driven `HO_PREP_START` 记到 `HO_END_OK`
- 当前最终 `HO success rate` 仍以剔除 `HO_PREP_BLOCKED_ADMISSION` 后的有效 `attempt` 为分母；该拒绝路径当前只属于 `handoverMode = improved`
- 当前 `handover_event_trace.csv` 已接入 `HO_PREP_START`、`HO_TRIGGER`、`HO_PREP_BLOCKED_*`、`HO_FAILURE_*` 与 `HO_END_ERROR` 事件，便于区分准备阶段失败与执行阶段失败
- 周期更新中已经会计算每星 `attachedUeCount`、`offeredPacketRate`、`loadScore`、`admissionAllowed`
- 当前已移除高噪声的 `KPI`、`GRID-ANCHOR` 控制台输出，保留切换、进度和最终汇总日志
- 当前已支持按成功切换序列自动统计短时 `ping-pong`
- 当前已将 `SRS` 调度相关参数入口从 baseline 配置面移除，并固定关闭相关项，避免与 handover 主线无关的 `PHY fatal`

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

## 最新实验记录（2026-04-07）

本轮结果对应：
- `commit id`：`92618baa8ba353a51647280e3bdcfe59a0874e3d`
- 结果目录：`scratch/results/exp/v4.1/joint/`
- 已执行批量命令：
  - `scratch/run_handover_experiment_matrix.sh --group joint-core --repeat 3`
  - `scratch/run_handover_experiment_matrix.sh --group improved-load-gating --repeat 3`
  - `scratch/run_handover_experiment_matrix.sh --group improved-return-guard --repeat 3`
  - `scratch/run_handover_experiment_matrix.sh --group improved-load-pressure --repeat 3`

本轮核心对比结论：
- 所有实验组的 `HO success rate` 都保持 `100%`，平均 `HO execution delay` 基本稳定在 `4.07 ms`
- 上述结论对应的是“引入显式 `HO preparation` 与动态 `AdmitHandoverRequest` 之前”的旧一轮批量实验口径；不能直接拿来解释当前代码
- 默认 baseline `B00` 的 3 次重复平均为：
  - `HO` 次数 `95.7`
  - `ping-pong` 次数 `2.7`
  - 吞吐恢复事件数 `23.7`
  - 平均吞吐恢复时间 `1710.4 ms`
  - 基于 `handover_dl_throughput_trace.csv` 统计的系统平均总下行吞吐约 `31.28 Mbps`
- 默认联合策略 `I00` 的 3 次重复平均为：
  - `HO` 次数 `121.0`
  - `ping-pong` 次数 `2.0`
  - 吞吐恢复事件数 `24.3`
  - 平均吞吐恢复时间 `1857.5 ms`
  - 基于 `handover_dl_throughput_trace.csv` 统计的系统平均总下行吞吐约 `31.31 Mbps`

本轮最稳的实验观察：
- 当前 `I00` 相比 `B00` 的主要收益不是“明显减少切换”，而是更均衡地分散 `UE` 的服务归属
- 基于 `handover_dl_throughput_trace.csv` 中各时刻 `serving_sat` 占用计算的 `Jain` 均衡指数（推断量）：
  - `B00` 平均约 `0.511`
  - `I00` 平均约 `0.714`
  - 这说明联合策略已经显著改变服务分布，负载均衡方向是有效的
- 但 `I00` 目前也带来了更多切换，且平均吞吐恢复时间没有优于 `B00`
- 因此截至本轮，更稳妥的表述应是：
  - 联合策略已显示出明显的负载均衡能力
  - 但其对切换频度与吞吐连续性的综合收益仍需进一步优化

本轮分组差异的具体判断：
- `I20`（保守负载门控）、`I21`（激进负载门控）和 `I31`（加强回切保护）在这批 `RngRun` 下与 `I00` 的切换序列完全一致
  - 当前更合理的解释不是“参数没生效”，而是这些门控在当前 `2x4`、`25 UE`、`seven-cell` 场景下尚未成为主导约束
- `I10`（关闭可见性项）只在 `rng-2` 与 `I00` 明显不同
  - 当前可见性项属于“偶尔参与决策”，但还不是稳定主导因素
- `I30`（关闭回切保护）只在 `rng-1` 与 `I00` 明显不同
  - 当前回切保护仅在少数局部路径上被真正激活
- `I40`（提高负载压力）是本轮最能激活联合策略差异的实验
  - 平均 `HO` 次数 `121.7`
  - 平均 `ping-pong` 次数 `3.7`
  - 平均吞吐恢复时间 `2191.5 ms`
  - 平均系统总吞吐约 `30.02 Mbps`
  - 基于 `serving_sat` 占用推断的均衡指数约 `0.663`
  - 这说明当负载压力被抬高时，联合策略确实更积极地动作，但当前实现下激进卸载与业务连续性之间仍存在明显代价

当前对论文写作最有价值的收口：
- 可以稳写：
  - baseline 与 improved 在统一真实 `MeasurementReport` 入口下已经形成可复现实验对照
  - 联合策略相对 baseline 已显著改善服务均衡
- 不能直接稳写：
  - “当前联合策略在整体性能上全面优于 baseline”
- 更准确的阶段性结论应为：
  - 当前联合策略已经证明“负载均衡方向有效”
  - 但还没有同时做到“更少切换 + 更低 `ping-pong` + 更快吞吐恢复”

当前建议的下一步：
- 优先把 `B00`、`I00`、`I40` 作为论文第一轮主图表/主表格候选
- 后续若要强调各门控模块的单独贡献，需要补一组更容易激活：
  - 可见性门控
  - 负载覆写门槛
  - 回切保护
- 在不改变当前主场景口径的前提下，优先继续围绕：
  - `HO` 次数
  - `ping-pong`
  - 吞吐恢复时间
  - 服务均衡
  做权衡优化，而不是先扩大星座规模
