# 研究仓库工作流与版本规则

## 版本定义
- 当前研究主阶段定义为 `3.3`。
- 最近已收敛并打 tag 的稳定节点为 `3.3.0`（`research-v3.3.0`）。
- 当前 `3.3.0` 是在继承 `3.2.2` baseline 语义不变的前提下，对工作区布局、脚本归档和文档口径做的一次收口快照。
- 这里的 `3.3 / 3.3.x` 指本仓库中的“毕设/研究工作版本”，不是 ns-3 框架本身的版本号。
- ns-3 框架版本仍然保持为 `3.46`，不要改动根目录的 `VERSION` 文件去表达研究版本。

## 当前 `3.3` 的含义
- 基于 `ns-3.46 + contrib/nr` 的当前研究基线。
- 主仿真入口为 `scratch/leo-ntn-handover-baseline.cc`。
- 当前默认基础组仍以 LEO 多卫星、多 UE 切换基线为主。
- `3.2.2` 提供当前默认 baseline 语义：
  - `seven-cell` baseline 场景与两阶段 `UE` 位置生成逻辑
  - custom `A3` 扰动观测链与 `ping-pong` 统计
  - `sat_anchor_trace.csv`、`ue_layout.csv`、`hex_grid_cells.svg`、`sat_beam_report.csv` 等结果输出链
  - `disableUeIpv4Forwarding`、`forceRlcAmForEpc` 等稳定性控制项
- `3.3.0` 在此基础上继续纳入：
  - `scratch/` 目录按“主脚本 / handover 头文件 / plotting 脚本 / midterm 文档”重组
  - 清理历史结果、缓存、示例 scratch 代码与中期阶段性派生资产
  - 将当前主线文档口径与新目录结构同步
- 当前 `3.3` 主阶段的研究口径已经明确为：
  - 使用当前 `2x4` 双轨场景暴露传统 A3 baseline 的局限
  - 后续围绕毕设任务书要求，设计“信号质量 + 卫星负载”联合感知切换策略
- 当前默认输出目录为 `scratch/results/`，结果默认不直接纳入 Git。

## 分支规则
- `main`：保持可回溯、可运行的主线基线。
- `feature/v3.3-xxx`：功能开发，例如切换触发、邻区门控、日志重构。
- `experiment/v3.3-xxx`：实验性方案验证，例如新 TTT、hysteresis、目标选择策略。
- `fix/v3.3-xxx`：缺陷修复。
- `docs/v3.3-xxx`：文档、实验记录和说明更新。
- 当前优先推荐的功能分支主题：
  - `feature/v3.3-a3-baseline`
  - `feature/v3.3-load-aware-handover`
  - `feature/v3.3-neighbor-gating`

## 提交信息规则
- 主阶段提交推荐格式：`type(v3.3): 简短说明`
- 稳定节点收口提交可使用：`type(v3.3.x): 简短说明`
- 常用 `type`：
  - `feat`
  - `fix`
  - `exp`
  - `docs`
  - `refactor`
  - `chore`
- 示例：
  - `feat(v3.3): refine A3 trigger gating`
  - `exp(v3.3): compare ttt 800ms and 1200ms`
  - `fix(v3.3): avoid writing outputs to repo root`
  - `chore(v3.3.0): snapshot reorganized scratch workspace`

## 结果管理规则
- 日常仿真输出统一落到 `scratch/results/`。
- `scratch/results/` 中的内容默认视为中间结果，不直接提交。
- 需要长期保留的关键结果，必须同时记录：
  - 对应 commit id
  - 关键参数
  - 实验目的
  - 主要结论
- 这些说明优先写入 `docs/current-task-memory.md` 或后续单独实验记录文档。

## 版本推进规则
- `3.3` 代表当前主基线阶段，不因为每次小改动就升大版本。
- `3.3.0` 代表当前主阶段下第一次收敛的稳定节点。
- 同一主阶段内的稳定节点，推荐使用 Git tag，而不是修改 ns-3 自带版本文件。
- 推荐 tag 形式：
  - `research-v3.3.0`

## 当前执行原则
- 先稳住 `3.3` 基线，再围绕传统 A3 baseline 和改进策略开展对比设计。
- 优先保证切换主流程、日志、输出和实验可复现性清晰。
- 当前不把“继续扩大星座规模”作为主目标，除非对比实验已经证明现有 `2x4` 双轨场景不足以暴露算法差异。
- 所有与切换策略直接相关的改动，提交时优先说明：
  - 触发条件
  - 目标选择
  - TTT / hysteresis
  - 邻区管理
  - 是否引入负载项
  - 对吞吐连续性和切换成功率的影响
