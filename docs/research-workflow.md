# 研究仓库工作流与版本规则

## 版本定义
- 当前研究主阶段定义为 `3.2`。
- 当前已收敛的稳定节点定义为 `3.2.0`。
- 这里的 `3.2 / 3.2.0` 指本仓库中的“毕设/研究工作版本”，不是 ns-3 框架本身的版本号。
- ns-3 框架版本仍然保持为 `3.46`，不要改动根目录的 `VERSION` 文件去表达研究版本。

## 当前 `3.2` 的含义
- 基于 `ns-3.46 + contrib/nr` 的当前研究基线。
- 主仿真入口为 `scratch/leo-ntn-handover-baseline.cc`。
- 当前默认基础组仍以 LEO 多卫星、多 UE 切换基线为主。
- 当前 `3.2.0` 节点继承并保留：
  - 双轨 `2x4` 基线
  - 崩溃修复
  - 周期性进度输出
  - 第一轮工程性能优化
- 当前 `3.2.0` 节点继续纳入：
  - `loadScore`（负载评分）相关运行时字段
  - 逐时刻 beam/load trace 导出
  - 面向当前分析使用的精简逐时刻 CSV 输出
  - 更强的双轨候选星空间重叠整定
  - `alignmentReferenceTimeSeconds`（对齐参考时刻）与 `simTime`（仿真时长）解耦
  - 默认关闭 `SRS`（探测参考信号）调度以规避非主线 PHY fatal
- 当前 `3.2` 主阶段的研究口径已经明确为：
  - 使用当前 `2x4` 双轨场景暴露传统 A3 baseline 的局限
  - 后续围绕毕设任务书要求，设计“信号质量 + 卫星负载”联合感知切换策略
- 当前默认输出目录为 `scratch/results/`，结果默认不直接纳入 Git。

## 分支规则
- `main`：保持可回溯、可运行的主线基线。
- `feature/v3.2-xxx`：功能开发，例如切换触发、邻区门控、日志重构。
- `experiment/v3.2-xxx`：实验性方案验证，例如新 TTT、hysteresis、目标选择策略。
- `fix/v3.2-xxx`：缺陷修复。
- `docs/v3.2-xxx`：文档、实验记录和说明更新。
- 当前优先推荐的功能分支主题：
  - `feature/v3.2-a3-baseline`
  - `feature/v3.2-load-aware-handover`
  - `feature/v3.2-neighbor-gating`

## 提交信息规则
- 主阶段提交推荐格式：`type(v3.2): 简短说明`
- 稳定节点收口提交可使用：`type(v3.2.0): 简短说明`
- 常用 `type`：
  - `feat`
  - `fix`
  - `exp`
  - `docs`
  - `refactor`
  - `chore`
- 示例：
  - `feat(v3.2): refine A3 trigger gating`
  - `exp(v3.2): compare ttt 800ms and 1200ms`
  - `fix(v3.2): avoid writing outputs to repo root`
  - `chore(v3.2.0): snapshot current baseline state`

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
- `3.2` 代表当前主基线阶段，不因为每次小改动就升大版本。
- `3.2.0` 代表当前主阶段下当前已收敛的一次稳定节点。
- 同一主阶段内的稳定节点，推荐使用 Git tag，而不是修改 ns-3 自带版本文件。
- 推荐 tag 形式：
  - `research-v3.2.0`
  - `research-v3.2.1`
  - `research-v3.2.2`

## 当前执行原则
- 先稳住 `3.2` 基线，再围绕传统 A3 baseline 和改进策略开展对比设计。
- 优先保证切换主流程、日志、输出和实验可复现性清晰。
- 当前不把“继续扩大星座规模”作为主目标，除非对比实验已经证明现有 `2x4` 双轨场景不足以暴露算法差异。
- 所有与切换策略直接相关的改动，提交时优先说明：
  - 触发条件
  - 目标选择
  - TTT / hysteresis
  - 邻区管理
  - 是否引入负载项
  - 对吞吐连续性和切换成功率的影响
