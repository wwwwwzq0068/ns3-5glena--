# 研究仓库工作流与版本规则

## 版本定义
- 当前研究主阶段定义为 `6.1`。
- 最近稳定节点定位为 `6.1`（建议 Git tag：`research-v6.1`）。
- `research-v5.0` 是在 `research-v4.3` 定向天线默认口径基础上完成 thesis-mainline 清理与文档收口后的发布包。
- `research-v5.1` 是在 `research-v5.0` 基础上的结构与口径收口包：主仿真入口继续下放无线 bootstrap、UE 初始接入/业务安装和 trace 输出生命周期，几何波束参数改为从真实 PHY `b00-* + gNB` 阵列规模自动推导。
- `research-v6.0` 是正式论文输出链收口包：固定 baseline / full improved 两组比较，默认保留业务、切换、负载公平性和轨迹/HTML 示意材料。
- `research-v6.1` 是正式论文场景与代码瘦身收口包：唯一主场景固定为 `2x4 + poisson-3ring + overlap-only + beam-only`，唯一正式比较为 baseline / improved。
- 这里的 `6.1 / 6.1.x` 指本仓库中的“毕设/研究工作版本”，不是 ns-3 框架本身的版本号。
- ns-3 框架版本仍然保持为 `3.46`，不要改动根目录的 `VERSION` 文件去表达研究版本。

## 当前 `6.1` 的含义
- 基于 `ns-3.46 + contrib/nr` 的当前研究基线。
- 主仿真入口为 `scratch/leo-ntn-handover-baseline.cc`。
- 当前默认基础组以 LEO 多卫星、多 UE 切换基线为主。
- `6.1` 提供当前默认 baseline / improved 语义：
  - `poisson-3ring` UE 生成场景与统一 `UE` 位置生成逻辑
  - `overlap-only` 波束锚点排他与 `beam-only` 真实链路候选门控
  - 统一 `MeasurementReport` 驱动的 `A3` baseline / improved 目标选择与 `ping-pong` 统计
  - `sat_anchor_trace.csv`、`sat_ground_track.csv`、`satellite_state_trace.csv`、`ue_layout.csv`、`hex_grid_cells.html`、`handover_event_trace.csv` 等结果输出链
  - `disableUeIpv4Forwarding`、`forceRlcAmForEpc` 等稳定性控制项
- `6.1` 还纳入：
  - 移除旧的几何 `beam budget/custom A3` handover 代理链与其派生脚本输出
  - 将周期更新主循环收紧为“轨道推进 + 服务关系观测 + 负载统计”
  - 将 TTT 归一化与手工防抖语义对齐到当前标准 A3 配置
  - 将 `docs/`、`scratch/` 与 `midterm-report/` 的当前口径同步到 measurement-driven 主线
  - 将 improved 收口为“信号 + 可见性 + 负载”的联合策略，并增强源站负载感知与卸载导向
- 当前 `6.1` 主阶段的研究口径已经明确为：
  - 固定当前 `2x4 + poisson-3ring + overlap-only + beam-only` 场景，并只做 baseline / full improved 对照
  - 在同一 `MeasurementReport` 入口下，对比 baseline 与 improved 的目标选择差异
- 当前默认输出目录为 `scratch/results/`，结果默认不直接纳入 Git。

## 分支规则
- `main`：保持可回溯、可运行的主线基线。
- `feature/v6.1-xxx`：功能开发，例如切换触发、邻区门控、日志重构。
- `experiment/v6.1-xxx`：实验性方案验证，例如新 TTT、hysteresis、目标选择策略。
- `fix/v6.1-xxx`：缺陷修复。
- `docs/v6.1-xxx`：文档、实验记录和说明更新。
- 当前优先推荐的功能分支主题：
  - `feature/v6.1-a3-baseline`
  - `feature/v6.1-load-aware-handover`
  - `feature/v6.1-neighbor-gating`

## 提交信息规则
- 主阶段提交推荐格式：`type(v6.1): 简短说明`
- 稳定节点收口提交可使用：`type(v6.1.x): 简短说明`
- 常用 `type`：
  - `feat`
  - `fix`
  - `exp`
  - `docs`
  - `refactor`
  - `chore`
- 示例：
  - `feat(v6.1): refine source-load-aware target selection`
  - `exp(v6.1): compare visibility and load-aware settings`
  - `fix(v6.1): avoid writing outputs to repo root`
  - `chore(v6.1): snapshot thesis scenario release`

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
- `6.1` 代表当前论文正式场景收口阶段，不因为每次小改动就升大版本。
- `6.1` 代表当前主阶段下的稳定节点。
- 同一主阶段内的稳定节点，推荐使用 Git tag，而不是修改 ns-3 自带版本文件。
- 推荐 tag 形式：
  - `research-v4.2`
  - `research-v5.0`
  - `research-v5.1`
  - `research-v6.0`
  - `research-v6.1`

## 当前执行原则
- 当前先固定 `2x4 + poisson-3ring + overlap-only + beam-only` 基线，再围绕传统 A3 baseline 和 full improved 策略开展对比设计。
- 优先保证切换主流程、日志、输出和实验可复现性清晰。
- 当前一段时间不把“继续扩大星座规模”作为主目标，优先在现有 `2x4` 双轨场景下完成 baseline / improved 对照与改进细化。
- 所有与切换策略直接相关的改动，提交时优先说明：
  - 触发条件
  - 目标选择
  - TTT / hysteresis
  - 邻区管理
  - 是否引入负载项
  - 对吞吐连续性和切换成功率的影响
