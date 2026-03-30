# 当前任务记忆

## 当前版本状态
- 最近已发布稳定节点：`3.3.0`（Git tag：`research-v3.3.0`）
- 当前工作区已进入 `3.3` 主阶段；当前版本保持 `3.2.2` baseline 语义不变，重点完成工作区收纳与文档收口
- 这里的 `3.3.x` 指研究工作版本，不是 ns-3 框架版本
- ns-3 框架版本保持为 `3.46`
- 当前主仿真入口：`scratch/leo-ntn-handover-baseline.cc`

## 当前 baseline 快照
- 当前场景 baseline：`2x4` 双轨、`25 UE`、`seven-cell`（中心 1 小区 + 周围 6 小区）二维部署
- 当前算法 baseline：传统 `A3` 风格切换，判决主线为 `RSRP + hysteresis + TTT + 基本可见性/beam lock` 
- `strictNrtGuard`（严格邻区表守卫）不再作为 baseline 默认项，转入后续增强策略方向
- 当前改进方向：当前默认已打开 `shadowing / Rician` 扰动并注入自定义 `beam budget/A3` 判决链，后续再进入“信号质量 + 卫星负载”联合感知策略

当前默认关键参数：
- `gNbNum = 8`
- `orbitPlaneCount = 2`
- `ueNum = 25`
- `ueLayoutType = seven-cell`
- `hexCellRadiusKm`（小区 hex 半径）=`20`
- `ueCenterSpacingMeters`（中心 3x3 间距）=`6000`
- `ueRingPointOffsetMeters`（外围小区内局部散点偏移）=`5000`
- `interPlaneRaanSpacingDeg`（轨道面 RAAN 间隔）=`-2 deg`
- `interPlaneTimeOffsetSeconds`（轨道面时间偏移）=`0.0 s`
- `alignmentReferenceTimeSeconds`（对齐参考时刻）=`20 s`
- `overpassGapSeconds`（同轨过境间隔）=`2 s`
- `updateIntervalMs`（主循环更新周期）=`100`
- `lambda`（业务流强度）=`1000 pkt/s/UE`
- `hoHysteresisDb`（切换迟滞门限）=`2.0 dB`
- `hoTttMs`（切换触发时间）=`200 ms`
- `pingPongWindowSeconds`（将 `A->B->A` 记为 `ping-pong` 的时间窗口）=`1.5 s`
- `customA3ShadowingSigmaDb`（阴影衰落标准差）=`2.5 dB`
- `customA3ShadowingCorrelationSeconds`（阴影衰落相关时间）=`1.0 s`
- `customA3RicianKDb`（莱斯 `K` 因子）=`3.0 dB`
- `customA3RicianCorrelationSeconds`（莱斯衰落相关时间）=`0.2 s`
- `forceRlcAmForEpc`（可选将 `EPC` 用户面承载映射为 `RLC AM`）=`false`
- `disableUeIpv4Forwarding`（默认关闭 `UE IPv4 forwarding`）=`true`

## 当前已确认实现
- 主脚本与运行时、统计、工具辅助头文件的拆分已经完成
- 周期更新已经拆开“卫星公共轨道状态”和“按 UE 派生观测几何”，用于减少重复轨道传播计算
- 当前 `UE` 位置生成逻辑已收口为“两阶段”实现：先生成局部东-北平面偏移模板，再统一转换为 `WGS84` 地理点/`ECEF`
- 当前默认 `UE` 布局已改为 `seven-cell`：中心小区内 `3x3` 密集簇 `9 UE`，外围 `6` 个相邻小区共分布 `16 UE`
- 外围 `16 UE` 按 `3/3/3/3/2/2` 分配到六个相邻 hex，并在各自小区中心附近做局部散点
- 当前默认启用六边形网格和自定义 `A3` 风格切换执行路径
- 当前 `NrChannelHelper` 已配置 `NTN-Rural + LOS + ThreeGpp`，并开启 `ShadowingEnabled`
- 但当前 `A3` 判决仍使用自定义 `beam budget/rsrpDbm` 观测链，尚未直接消费 PHY 层真实测量结果
- 当前默认关闭 `UE IPv4 forwarding`，避免异常下行包被误送回 `NrEpcUeNas::Send()` 重新分类
- 当前保留 `forceRlcAmForEpc` 作为可选稳定性开关，但默认不改变 helper 的 `RLC` 映射
- 当前默认关闭 `strictNrtGuard`（严格邻区表守卫），保留其作为后续增强策略开关
- 当前默认关闭高噪声的 `KPI`、`OVERPASS`、`GRID-ANCHOR` 输出，保留切换和最终汇总日志
- 当前新增周期性 `[Progress]` 日志，用于观察仿真推进
- 当前已支持按成功切换序列自动统计短时 `ping-pong`（`A->B->A`）次数，并在最终 summary 中输出整体与分 UE 计数
- 当前默认关闭 `SRS`（探测参考信号）调度相关项，避免与 handover 主线无关的 `PHY fatal`
- `SN Status Transfer`、`NrPdcp::DoReceivePdu()`、`UdpServer::HandleRead()` 相关崩溃防御已纳入

## 当前负载接口状态
- `SatelliteRuntime` 已具备 `attachedUeCount`、`offeredPacketRate`、`loadScore`、`admissionAllowed`
- 周期更新中已经会按当前接入 UE 数计算每星基础负载状态
- 这些量当前只用于观测和 trace 输出，尚未进入 baseline 切换决策

## 后续联合策略草案
- 详细定义、变量名和数学式已独立整理到 `docs/joint-handover-strategy.md`
- 当前建议的 improved 路线保持 baseline 场景口径不变，继续在 `2x4`、`25 UE`、`seven-cell` 条件下做对照
- 当前建议的 improved 主线仍是“信号质量 + 剩余可见时间 + 负载”的联合效用目标选择
- 在进入 improved 之前，当前优先先澄清 `shadowing / Rician` 是否接入自定义 `beam budget` 判决链，以及接入后 baseline 暴露性会如何变化
- 第一版实现建议复用当前自定义切换执行路径，在 `UpdateConstellation()` 观测链中插入联合评分选择逻辑，而不是优先改成 `NrHandoverAlgorithm` 子类
- 当前文档口径继续强调：baseline 不使用负载做判决，`rsrpDbm` 仍是几何 `beam budget`/`RSRP` 代理

## 当前输出与脚本
- 默认结果目录：`scratch/results/`
- 当前默认输出：
  - `hex_grid_cells.svg`（当 `runGridSvgScript = true`）
  - `ue_layout.csv`
  - `sat_beam_trace.csv`
  - `sat_beam_report.csv`（当 `runBeamReportScript = true`）
  - 六边形网格相关 `CSV/SVG`
- 当前常用分析脚本：
  - `scratch/plotting/sat_beam_report.py`
  - `scratch/plotting/plot_hex_grid_svg.py`

## 当前研究问题
- 继续验证传统 `A3` baseline 在当前双轨场景下是否能稳定暴露：
  - 频繁切换
  - 潜在 `ping-pong`
  - 触发过早或过晚
  - 吞吐连续性问题
- 继续核实当前 `2x4` 双轨场景中“第二轨是否真正参与竞争”，避免把“已建成双轨场景”误写成“已形成充分双轨竞争”
- 区分 `hex grid`（地面锚点/服务区域目录）与真实 `cell`（协议栈中的卫星小区）语义，避免把 `hex` 数量直接等同为可接入小区数量
- 结合 `beamLocked`、`patternLossDb`、`rsrpDbm` 与服务星分布，确认“未参与接入的卫星”究竟是：
  - 当前主波束没有有效扫到 `UE` 热区
  - 只是可见但长期处于大离轴角、始终不是最优候选
  - 还是轨道对齐/时序参数导致双轨重叠不足
- 明确 `sat_beam_trace.csv` 当前记录的是理想化 `beam budget`（波束预算）/几何 `RSRP` 代理，而不是完整真实测量值，避免把极小 `delta`（前两强功率差）直接解释成真实物理世界下的边界强弱关系
- 评估当前外围 `6` 个小区中的 `UE` 分布是否足够均衡，是否已经形成可观察的跨小区竞争
- 评估中心 `3x3` 密集簇与外围 `6` 小区占位的组合，对“baseline 缺陷暴露”和“场景代表性”的影响边界
- 继续评估当前默认开启的 `shadowing / Rician` 扰动对自定义 `beam budget`/A3 判决链的影响，重点观察边界竞争、`delta` 分布和 `ping-pong` 暴露性如何变化
- 将毕设任务书中的“信号质量 + 卫星负载”要求转化为后续改进算法的明确设计目标
- 在保持当前场景口径不变的前提下，为后续联合策略提供可解释的对照组

## 当前工作边界
- 当前不把“继续扩大星座规模”作为默认主线，除非现有 `2x4` 场景已无法体现算法差异
- 当前不再把“代码结构整理”作为第一优先级，除非算法实现需要新的结构调整
- 当前 baseline 的 `UE` 场景口径已经切到 `seven-cell`，后续不要再把旧的 `hotspot-boundary` 图或参数当成当前默认定义
- 讨论或文档中提到关键参数时，优先使用“英文参数名（中文释义）”的写法
- 修改 `scratch/` 目录下的重要代码后，同步检查：
  - `scratch/README.md`
  - `scratch/baseline-definition.md`
  - 相关 `midterm-report` 文档

## 当前优先方向
- 稳住 `2x4` 双轨、`25 UE`、`seven-cell` baseline 场景
- 用统一口径完成一轮七小区几何与切换现象验证
- 先澄清当前 `sat_beam_trace` 和自定义 `beam budget` 判决链在默认开启 `shadowing / Rician` 后还能回答什么、不能回答什么，再进入后续联合策略
- 在不改变 baseline 定位的前提下，最后再推进“信号质量 + 卫星负载”联合感知算法

## `3.3.0` 收口结果
- 当前最新稳定节点已收口为 `research-v3.3.0`
- 本次升级不改变 `3.2.2` 已收口的 baseline 场景和判决语义，重点是把工作区整理成更适合后续 thesis 主线推进的形态

`3.3.0` 已纳入的改动包：
- 将主脚本保留在 `scratch/` 根目录，并把 handover 辅助头文件集中到 `scratch/handover/`
- 将分析与绘图脚本集中到 `scratch/plotting/`
- 清理 `scratch/results/` 中历史实验结果、`__pycache__`、示例 scratch 目录与中期阶段性派生资产
- 保留 `scratch/midterm-report/` 的核心 markdown 文档作为轻量归档
- 同步更新 `docs/` 与 `scratch/` 的路径说明和工作区口径

`3.3.0` 最少发布检查：
- 跑通一轮默认 `seven-cell baseline`
- 确认 `hex_grid_cells.svg`、`ue_layout.csv`、`sat_beam_trace.csv`、`sat_anchor_trace.csv`、`sat_beam_report.csv` 输出链完整
- 确认文档中的默认参数与代码默认值一致
- 确认 `scratch/README.md`、`scratch/baseline-definition.md`、`scratch/midterm-report/midterm-technical-summary.md` 口径一致

## 当前汇报准备入口
- 版本收口说明优先参考本文件本节，以及 `scratch/README.md` 中的版本演进记录
- 配图说明优先参考 `scratch/midterm-report/midterm-image-generation-spec.md`
- PPT 页次与配图对应优先参考 `scratch/midterm-report/midterm-ppt-design.md`
