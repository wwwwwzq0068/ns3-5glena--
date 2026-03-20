# Scratch 目录说明

## 目录用途
- 本目录存放当前毕设使用的 LEO-NTN 切换仿真代码、辅助头文件和分析脚本
- 当前主要仿真入口：`scratch/leo-ntn-handover-baseline.cc`
- 最近已发布稳定节点：`3.2.1`（Git tag：`research-v3.2.1`）
- 当前工作区仍在 `3.2` 主阶段内继续整理；若看到 `seven-cell` baseline 与相关新参数/日志改动，应视为 `3.2.1` 之后的未发布状态
- 这里的 `3.2.x` 是研究工作稳定节点，不是 ns-3 框架版本；ns-3 本身仍然是 `3.46`

## 当前主线
- 当前阶段先稳住基础组，再在此基础上验证和改进切换策略
- 当前工作区默认研究场景是：`2x4` 双轨、`25 UE`、`seven-cell` 二维部署
- 当前默认目标不是继续扩星，而是把该七小区场景作为传统 `A3 baseline` 的缺陷暴露平台
- `strictNrtGuard`（严格邻区表守卫）不再计入 baseline 默认定义，转入后续增强策略侧
- 当前改进方向先是继续评估默认已开启的 `shadowing / Rician` 扰动如何影响自定义 `beam budget/A3` 判决链，之后才是“信号质量 + 卫星负载”联合感知切换策略

## 当前版本判断
- 想确认“是不是进入新版本”，先看是否已打新的 `research-v3.2.x` tag
- 目前最近已发布稳定节点仍是 `research-v3.2.1`
- 当前工作区中与 `seven-cell` baseline、`sat_anchor_trace.csv`、custom `A3` 扰动链相关的变化，默认按“未发布工作区改动”理解，除非后续再打新 tag

## 当前默认口径
场景与参数：
- 卫星数：`8`
- 轨道面数：`2`
- UE 数：`25`
- UE 主布局：`seven-cell`
- `interPlaneRaanSpacingDeg`（轨道面 RAAN 间隔）=`3 deg`
- `alignmentReferenceTimeSeconds`（对齐参考时刻）=`20 s`
- `simTime`（仿真时长）=`40 s`
- `updateIntervalMs`（主循环更新周期）=`100`
- `lambda`（业务流强度）=`1000 pkt/s/UE`
- `hoHysteresisDb`（切换迟滞门限）=`3.0 dB`
- `hoTttMs`（切换触发时间）=`300 ms`
- `customA3ShadowingSigmaDb`（阴影衰落标准差）=`1.0 dB`
- `customA3ShadowingCorrelationSeconds`（阴影衰落相关时间）=`4.0 s`
- `customA3RicianKDb`（莱斯 `K` 因子）=`15 dB`
- `customA3RicianCorrelationSeconds`（莱斯衰落相关时间）=`1.0 s`

切换口径：
- 当前算法 baseline 为传统 `A3` 风格切换
- 判决依据保持为 `RSRP + hysteresis + TTT + 基本可见性/beam lock`
- `strictNrtGuard`（严格邻区表守卫）保留为可选增强开关，不作为 baseline 默认条件
- 当前 baseline 不使用负载做决策，但运行时已保留负载观测字段
- 当前 PHY 信道已开启 `ShadowingEnabled`，但默认 `A3` 判决仍看几何 `beam budget/rsrpDbm`
- 当前平台已支持把 `shadowing / Rician` 扰动可开关地注入 custom `beam budget/A3` 观测链，且当前默认开启；但判决仍不是直接读取 PHY 测量
- 当前默认关闭 `UE IPv4 forwarding`，避免异常下行包被 UE 误判为待转发上行包重新送回 `NAS`
- 当前保留 `forceRlcAmForEpc` 作为可选稳定性开关，但默认不覆盖 helper 的 `RLC` 映射

当前默认 UE 紧凑度：
- `hexCellRadiusKm`（小区 hex 半径）=`20`
- `ueCenterSpacingMeters`（中心 `3x3` 间距）=`6000`
- `ueRingPointOffsetMeters`（外围 `6` 小区内局部散点偏移）=`5000`

当前默认 UE 生成实现：
- 先在局部东-北平面生成 `seven-cell` 偏移模板
- 中心小区放置 `3x3` 密集簇，共 `9 UE`
- 六个相邻小区共放置 `16 UE`，按 `3/3/3/3/2/2` 分配并在各自小区中心附近散开
- 再统一将偏移模板转换为 `WGS84` 地理点和 `ECEF` 位置

## 文档分工
- `docs/research-context.md`
  - 研究范围、目标、评估指标和稳定上下文
- `docs/current-task-memory.md`
  - 当前稳定节点、默认口径、已确认实现和近期工作边界
- `docs/joint-handover-strategy.md`
  - 后续“信号质量 + 卫星负载”联合策略的设计说明、变量映射与数学表达
- `docs/research-workflow.md`
  - 版本、分支、提交和结果管理规则
- `scratch/baseline-definition.md`
  - baseline 的正式定义、默认参数口径、验证清单和改进边界
- `scratch/midterm-report/midterm-technical-summary.md`
  - 面向中期汇报的技术总结
- `scratch/midterm-report/midterm-handover-flowcharts.md`
  - 中期汇报流程图与简要讲解提示

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
- 默认保留切换开始、切换成功、服务星变化、周期性进度和最终统计
- 默认不强调 `OVERPASS`、`GRID-ANCHOR` 等高噪声信息

结果目录：
- 当前默认仿真输出统一写入 `scratch/results/`
- 常见结果包括：
  - `hex_grid_cells.csv`
  - `sat_beam_trace.csv`
  - `sat_anchor_trace.csv`
  - `sat_attenuation_per_time.csv`

分析脚本：
- `sat_attenuation_report.py`
  - 默认读取 `scratch/results/sat_beam_trace.csv`
  - 默认生成 `scratch/results/sat_attenuation_per_time.csv`
- `plot_hex_grid_svg.py`
  - 读取六边形网格 `CSV`
  - 生成对应 `SVG`
  - 当前支持叠加 `UE` 布局 `CSV`，用于导出 `grid + UE` 视图
  - 当前支持叠加 `sat_anchor_trace.csv`，用于导出“两轨波束锚点主线 + 各卫星起终点”视图
- `export_ue_layout.py`
  - 按当前 `UE` 布局规则导出 `UE` 位置 `CSV`
  - 当前可直接复现 `line` 和 `seven-cell` 两类布局

## 当前已完成的关键收口
- 主脚本与运行时、统计、工具辅助头文件的拆分已经完成
- 切换相关实时日志与最终汇总格式已经完成一轮收敛
- 当前已去掉单 UE `A3 gate` 兼容链，baseline 固定走多 UE `custom A3` 路径
- 当前已将周期更新中的卫星公共轨道传播与 UE 派生观测彻底分开
- 当前已将默认参数和合法性检查集中到 `leo-ntn-handover-config.h`
- 当前已接入 `loadScore`（负载评分）相关运行时字段与逐时刻 trace 输出
- 当前已将默认 `UE` 布局切换为 `seven-cell`，并补齐导出 `CSV/SVG` 的脚本链

## 接下来
- 先用当前默认参数完成一轮 `seven-cell baseline` 几何与切换现象验证
- 继续评估当前默认开启的 `shadowing / Rician` 扰动如何影响自定义 `beam budget/A3` 判决链中的边界竞争与 `ping-pong`
- 最后在不改 baseline 场景定义的前提下，推进“信号质量 + 卫星负载”联合目标选择

## 维护规则
- 修改 `scratch/` 目录下的重要代码后，同步检查：
  - `scratch/README.md`
  - `scratch/baseline-definition.md`
  - 相关 `midterm-report` 文档
- 文档优先写清当前口径和决策，不堆叠过长历史说明
- 当文档中提到关键参数时，优先采用“英文参数名（中文释义）”写法
- 版本、分支和提交命名规则以 `docs/research-workflow.md` 为准

## 版本演进记录
本节保留研究过程中的主要版本收口信息。写法上不再逐条堆叠零散改动，而是按版本归档关键变化，便于后续回看“什么时候改了什么、为什么改”。

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
  - 更新 `sat_attenuation_report.py`，使衰减导出结果保留 `ue` 维度
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
  - `sat_attenuation_report.py` 的默认输入输出路径同步迁移到 `scratch/results/`

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
  - 将 `sat_attenuation_report.py` 收口为更适合当前分析使用的精简逐时刻输出
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
  - 明确当前代码默认值为 `updateIntervalMs = 100`、`lambda = 1000 pkt/s/UE`
  - 明确默认会生成 `sat_beam_trace.csv`，并在 `runAttenuationScript = true` 时继续生成 `sat_attenuation_per_time.csv`
  - 同步收口当前崩溃防御链说明，补齐 `SN Status Transfer`、`NrPdcp::DoReceivePdu()`、`UdpServer::HandleRead()` 的描述
- 配置入口收口
  - 新增 `leo-ntn-handover-config.h`
  - 将默认参数、命令行参数注册、输出路径收口和参数合法性检查从主脚本抽离
  - 本次调整不改变 `2x4` 双轨、`25 UE`、传统 `A3 baseline` 的场景口径
- 减法式清理
  - 去掉单 UE `A3 gate` 兼容链，明确当前 baseline 固定走多 UE `custom A3` 执行路径
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
- 最近已发布稳定节点
  - 对应 tag：`research-v3.2.1`
  - 对应提交：`c692e68 chore(v3.2.1): snapshot tightened baseline and joint strategy docs`
  - 重点是收紧 baseline 与联合策略文档口径，而不是再定义一个新的场景版本

### `post-v3.2.1`
- 当前未发布工作区主线
  - `b50d5cc Refactor UE layout to seven-cell baseline` 已将 `UE` 主布局重构到 `seven-cell`
  - 当前工作区继续围绕 `seven-cell baseline` 整理参数、观测链与可视化输出
  - 若没有新的 `research-v3.2.x` tag，这些改动都应视为“下一稳定节点之前的未发布工作”

## 建议发布包
- 建议下一稳定节点使用 `research-v3.2.2`
- 当前这批改动更像 `3.2` 主阶段内的 baseline 收紧，而不是新的主阶段升级

建议纳入 `v3.2.2` 的内容：
- `seven-cell` baseline 场景与两阶段 `UE` 位置生成逻辑
- custom `A3` 观测链的 `shadowing / Rician` 扰动注入
- `sat_beam_trace.csv` 新字段：`geometry_rsrp_dbm`、`custom_a3_shadowing_db`、`custom_a3_rician_fading_db`
- `sat_anchor_trace.csv` 输出，以及 `plot_hex_grid_svg.py` 对卫星锚点轨迹的叠加绘图
- `forceRlcAmForEpc`、`disableUeIpv4Forwarding` 等稳定性开关
- `NrEpcTftClassifier` 的 malformed packet 防御
- 当前版本文档口径收口

建议发布前检查：
- 默认参数下跑通一轮 `seven-cell baseline`
- 确认 `scratch/results/` 输出链完整
- 确认 baseline 文档、README 与中期汇报技术总结的参数口径一致

建议收口提交：
- `chore(v3.2.2): snapshot seven-cell baseline and custom-a3 measurement chain`
