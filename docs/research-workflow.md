# 研究仓库工作流与版本规则

## 版本定义
- 当前研究版本号定义为 `3.0`。
- 这里的 `3.0` 指本仓库中的“毕设/研究工作版本”，不是 ns-3 框架本身的版本号。
- ns-3 框架版本仍然保持为 `3.46`，不要改动根目录的 `VERSION` 文件去表达研究版本。

## 当前 `3.0` 的含义
- 基于 `ns-3.46 + contrib/nr` 的当前研究基线。
- 主仿真入口为 `scratch/leo-ntn-handover-baseline.cc`。
- 当前默认基础组仍以 LEO 多卫星、多 UE 切换基线为主。
- 当前阶段优先任务已从文件整理和日志收敛切换到星座规模扩展。
- 当前默认输出目录为 `scratch/results/`，结果默认不直接纳入 Git。

## 分支规则
- `main`：保持可回溯、可运行的主线基线。
- `feature/v3.0-xxx`：功能开发，例如切换触发、邻区门控、日志重构。
- `experiment/v3.0-xxx`：实验性方案验证，例如新 TTT、hysteresis、目标选择策略。
- `fix/v3.0-xxx`：缺陷修复。
- `docs/v3.0-xxx`：文档、实验记录和说明更新。
- 当前优先推荐的功能分支主题：
  - `feature/v3.0-constellation-scaling`
  - `feature/v3.0-neighbor-scaling`

## 提交信息规则
- 推荐格式：`type(v3.0): 简短说明`
- 常用 `type`：
  - `feat`
  - `fix`
  - `exp`
  - `docs`
  - `refactor`
  - `chore`
- 示例：
  - `feat(v3.0): refine A3 trigger gating`
  - `exp(v3.0): compare ttt 800ms and 1200ms`
  - `fix(v3.0): avoid writing outputs to repo root`

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
- `3.0` 代表当前主基线阶段，不因为每次小改动就升大版本。
- 同一主阶段内的稳定节点，推荐使用 Git tag，而不是修改 ns-3 自带版本文件。
- 推荐 tag 形式：
  - `research-v3.0.0`
  - `research-v3.0.1`
  - `research-v3.0.2`

## 当前执行原则
- 先稳住 `3.0` 基线，再开展优化组设计。
- 优先保证切换主流程、日志、输出和实验可复现性清晰。
- 所有与切换策略直接相关的改动，提交时优先说明：
  - 触发条件
  - 目标选择
  - TTT / hysteresis
  - 邻区管理
  - 对吞吐连续性和切换成功率的影响
