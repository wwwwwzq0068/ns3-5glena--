# 当前任务记忆

## 当前工作区域
- 当前研究仓库版本：`3.0`
- 版本说明：
  - `3.0` 表示当前毕设/研究工作版本
  - `ns-3` 框架版本仍然是 `3.46`
- 仓库工作流与版本规则：`docs/research-workflow.md`
- 当前主要仿真入口文件：`scratch/myfirst.cc`
- 当前已拆分出的自定义头文件：
  - `scratch/myfirst-runtime.h`
  - `scratch/myfirst-reporting.h`
  - `scratch/myfirst-utils.h`
  - `scratch/beam-link-budget.h`
  - `scratch/leo-orbit-calculator.h`
  - `scratch/wgs84-hex-grid.h`
- 最近已经阅读过的辅助脚本：
  - `scratch/sat_attenuation_report.py`
  - `scratch/plot_hex_grid_svg.py`

## 当前仿真背景
- 当前 `3.0` 基线的主要目标，是先稳定当前 LEO 多卫星、多 UE 切换场景，再逐步加入优化组策略。
- 当前阶段的重点是先完成毕设基础组搭建，再在基础组之上设计优化对照组。
- 当前首要目标是构建一个更合理的 LEO 物理场景。
- 用户当前明确给出的基础组目标为：
  - 5 颗同轨道卫星
  - 3 个 UE
- 目前从 `scratch/myfirst.cc` 已确认的代码状态：
  - `gNbNum = 5`
  - `ueNum` 已改为可配置，默认值为 `3`
  - UE 使用简单的地面间隔模型进行部署
- 当前基础组已经不再局限于单 UE 路径。
- 最近已确认的日志行为：
  - 六边形网格模式已开启
  - 严格 NRT 守卫已开启
  - 自定义 A3 风格切换执行逻辑已开启
  - 每个 UE 都会输出简洁的初始化接入信息
  - 默认保留切换相关日志，关闭了较嘈杂的 KPI、OVERPASS、GRID-ANCHOR 输出

## 已确认输出文件
- `sat_beam_trace.csv`
  - 作为衰减分析输入的逐时刻波束/链路原始跟踪文件
  - 当前已增加 `ue` 列
- `sat_attenuation_per_time.csv`
  - 由 `scratch/sat_attenuation_report.py` 生成
  - 当前已保留 `ue` 维度
- 六边形网格相关 CSV 和 SVG
  - 用于 WGS84 六边形网格可视化流程

## 已确认的代码行为
- `KPI` 日志用于报告当前服务卫星、最优 RSRP 卫星和最近卫星。
- `GRID-ANCHOR` 日志用于报告每颗卫星当前对应的六边形网格锚点。
- `HO-START` 和 `HO-END-OK` 用于表示切换开始和切换成功完成。
- 日志中“freeze automatic handover after predicted success”并不会阻止后续全部切换，因为当前实现路径里仍然存在自定义切换执行器。
- 最终统计输出已从 `myfirst.cc` 抽到 `scratch/myfirst-reporting.h`，主脚本现在更偏向场景搭建和流程控制。
- 自定义头文件已开始统一补充中文文件头说明、结构体注释和函数注释。

## 当前待解决问题
- 继续引入更大的星座规模，使仿真场景更具代表性，而不是只停留在很小的局部过境序列。
- 继续精简调试输出，重点保留：
  - 切换事件
  - 服务卫星变化
  - 切换是否成功
  - 最终汇总指标
- 增加适量中文注释，提升代码可读性，但避免注释过多造成噪声。
- 继续提升代码模块化程度和可维护性。
- 每次修改 `scratch/` 目录下的重要代码后，同步维护 `scratch/README.md`。

## 近期工作方向
- 先把当前场景作为基础组稳定下来，再逐步加入更复杂的优化决策逻辑。
- 明确区分：
  - 基础组构建
  - 后续优化组设计
- 在进入算法优化前，优先推进：
  - 更合理的 LEO 场景构建
  - 多 UE 支持
  - 更干净的日志输出
  - 更清晰的代码结构
- 在解读仿真结果时，始终优先从研究指标出发，而不仅仅是判断代码是否能运行。
- 保持“当前已实现行为”和“毕设目标行为”之间的明确区分。
