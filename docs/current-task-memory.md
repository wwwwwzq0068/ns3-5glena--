# 当前任务记忆

## 当前工作区域
- 当前研究仓库稳定节点：`3.1.3`
- 版本说明：
  - `3.1` 表示当前毕设/研究工作主阶段
  - `3.1.3` 表示当前主阶段下当前已收敛的稳定节点
  - `ns-3` 框架版本仍然是 `3.46`
- 仓库工作流与版本规则：`docs/research-workflow.md`
- 当前主要仿真入口文件：`scratch/leo-ntn-handover-baseline.cc`
- 当前已拆分出的自定义头文件：
  - `scratch/leo-ntn-handover-runtime.h`
  - `scratch/leo-ntn-handover-reporting.h`
  - `scratch/leo-ntn-handover-utils.h`
  - `scratch/beam-link-budget.h`
  - `scratch/leo-orbit-calculator.h`
  - `scratch/wgs84-hex-grid.h`
- 最近已经阅读过的辅助脚本：
  - `scratch/sat_attenuation_report.py`
  - `scratch/plot_hex_grid_svg.py`

## 当前仿真背景
- 当前 `3.1.3` 稳定节点以 LEO 多卫星、多 UE 切换场景为主，但当前阶段的研究重点已经从“继续扩星”和“工程优化”切换为“围绕毕设任务书收束研究主线”。
- 当前阶段不继续扩大星座规模，先保持 `2x4` 双轨基础组，把它作为传统 A3 缺陷暴露和后续改进策略对比的可控实验平台。
- 当前默认 UE 主场景已升级为 `25 UE` 的二维热点增强布局，用于更明显地暴露负载不均衡与边界竞争。
- 当前首要目标已经明确切换为：
  - 定义清晰的传统 A3 baseline
  - 在当前双轨场景下验证传统 A3 的局限
  - 为后续“信号质量 + 卫星负载”联合感知切换策略预留实现路径
- 目前从 `scratch/leo-ntn-handover-baseline.cc` 已确认的代码状态：
  - `gNbNum = 8`
  - `ueNum` 已改为可配置，默认值为 `25`
  - `orbitPlaneCount = 2`
  - 默认采用双轨道面生成卫星
  - 当前默认同时通过几何与时序参数整定、以及 `25 UE` 的热点增强布局来增强双轨竞争，作为 A3 baseline 缺陷验证场景
  - 当前默认已将 `interPlaneRaanSpacingDeg`（轨道面 RAAN 间隔）收敛到 `6 deg`
  - 当前默认保持 `alignmentReferenceTimeSeconds`（对齐参考时刻）为独立参数，默认 `20 s`
  - 当前代码默认保持 `updateIntervalMs = 100`（主循环更新周期）和 `lambda = 1000 pkt/s/UE`（业务流强度）
  - 当前调参阶段默认保持中等仿真时长，避免过长窗口增加日志和排障负担
  - UE 默认使用 `hotspot-boundary`（热点增加 + 边界增强）二维部署，同时保留 `line`（线性）作为对照入口
  - 脚本崩溃问题已完成一轮修复，当前已纳入 `SN Status Transfer`（序列号状态转移）头长度修正、`NrPdcp::DoReceivePdu()`（PDCP 接收入口）异常头保护，以及 `UdpServer::HandleRead()`（UDP 接收入口）对短包的防御性丢弃
  - 当前默认关闭 `EnableSrsInFSlots`、`EnableSrsInUlSlots`，并将 `SrsSymbols` 设为 `0`，避免与 handover 主线无关的 PHY fatal
  - `UpdateConstellation()` 已开始拆分“卫星公共轨道状态”和“按 UE 派生的观测几何”，用于减少重复轨道传播计算
- 当前基础组已经不再局限于单 UE 路径。
- 最近已确认的日志行为：
  - 六边形网格模式已开启
  - 严格 NRT 守卫已开启
  - 自定义 A3 风格切换执行逻辑已开启
  - 每个 UE 都会输出简洁的初始化接入信息
  - 默认保留切换相关日志，关闭了较嘈杂的 KPI、OVERPASS、GRID-ANCHOR 输出
- 当前新增周期性 `[Progress]` 日志，用于显示“仿真已推进到 `t=?s`”
- 当前阶段已确认完成的工程整理事项：
  - 主脚本与运行时/统计/工具辅助头文件的拆分已经完成
  - 切换相关实时日志与最终汇总格式已经完成一轮收敛
  - 当前不再把“代码结构整理”作为第一优先级
  - 当前也不再把“继续扩大星座规模”作为第一优先级

## 已确认输出文件
- `sat_beam_trace.csv`
  - 作为衰减分析输入的逐时刻波束/链路原始跟踪文件
  - 当前已增加 `ue` 列
  - 当前代码默认生成
- `sat_attenuation_per_time.csv`
  - 由 `scratch/sat_attenuation_report.py` 生成
  - 当前已保留 `ue` 维度
  - 当前在 `runAttenuationScript = true`（运行衰减后处理脚本）时默认生成
- 六边形网格相关 CSV 和 SVG
  - 用于 WGS84 六边形网格可视化流程

## 已确认的代码行为
- `KPI` 日志用于报告当前服务卫星、最优 RSRP 卫星和最近卫星。
- `GRID-ANCHOR` 日志用于报告每颗卫星当前对应的六边形网格锚点。
- `HO-START` 和 `HO-END-OK` 用于表示切换开始和切换成功完成。
- 日志中“freeze automatic handover after predicted success”并不会阻止后续全部切换，因为当前实现路径里仍然存在自定义切换执行器。
- 最终统计输出已从 `leo-ntn-handover-baseline.cc` 抽到
  `scratch/leo-ntn-handover-reporting.h`，主脚本现在更偏向场景搭建和流程控制。
- 自定义头文件已开始统一补充中文文件头说明、结构体注释和函数注释。

## 当前待解决问题
- 定义一个足够干净的传统 A3 baseline，明确其判决依据、`TTT`、`hysteresis` 和对比口径。
- 在保持 `2x4` 双轨规模不变的前提下，验证传统 A3 在当前场景中的缺陷是否稳定、可重复，重点关注：
  - 频繁切换
  - 潜在 ping-pong
  - 触发过早或过晚
  - 吞吐连续性
- 将毕设任务书中的“信号质量 + 卫星负载”要求转化为后续改进算法的明确设计目标。
- 为后续改进策略补齐负载建模入口，避免当前实现长期停留在纯信号质量驱动。
- 继续维护运行效率，但工程优化不再压过研究主线。
- 增加适量中文注释，提升代码可读性，但避免注释过多造成噪声。
- 每次修改 `scratch/` 目录下的重要代码后，同步维护 `scratch/README.md`。
- 以后在说明关键参数时，不能只写英文参数名；需要同时给出中文释义，便于后续论文整理和日常讨论。

## 近期工作方向
- 以“保持 `2x4` 双轨基线稳定，并把它用作传统 A3 缺陷验证平台”作为当前第一优先级。
- 明确区分：
  - 场景 baseline：当前 `2x4` 双轨、多 UE、可复现实验平台
  - 算法 baseline：传统 A3 风格切换逻辑
  - 改进算法：后续面向毕设任务书的联合感知切换策略
- 当前优先推进：
  - 传统 A3 baseline 的定义和收口
  - 双轨场景下 A3 局限性的对比实验
  - 后续“信号质量 + 卫星负载”联合感知算法的指标和接口设计
- 当前不把“继续扩大星座规模”作为主线，除非后续算法验证明确需要更大规模场景。
- 在解读仿真结果时，始终优先从研究指标出发，而不仅仅是判断代码是否能运行。
- 保持“当前已实现行为”和“毕设目标行为”之间的明确区分。
- 在文档、讨论和实验说明中提到参数时，优先采用“英文参数名（中文释义）”的写法，例如 `updateIntervalMs（主循环更新周期）`。
