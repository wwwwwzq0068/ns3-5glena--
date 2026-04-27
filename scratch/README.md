# Scratch 目录索引

本目录保留当前 thesis mainline 使用的仿真入口、辅助头文件、绘图脚本和结果暂存区。

当前正式 baseline 口径不在这里展开重写，仍按固定 `2x4` 双轨、`25 UE`、`seven-cell`、传统 `A3` 比较路径理解；`r2-diagnostic` 继续只作为诊断入口，不替代正式 baseline。

## 先看哪里
- 主仿真入口：`scratch/leo-ntn-handover-baseline.cc`
- baseline 正式定义：`scratch/baseline-definition.md`
- 当前稳定参数与工作边界：`docs/current-task-memory.md`
- 研究目标与评价口径：`docs/research-context.md`
- 版本与结果管理规则：`docs/research-workflow.md`
- 结果保留规则：`scratch/results/README.md`
- 中期材料归档说明：`scratch/midterm-report/README.md`

## 使用约定
- 需要解释“baseline 是什么”时，优先引用 `scratch/baseline-definition.md`
- 需要确认“当前默认口径是什么”时，优先引用 `docs/current-task-memory.md`
- `scratch/results/` 默认只放临时输出；长期保留结果按 `docs/research-workflow.md` 记录
- 修改 `scratch/` 重要内容后，同步检查本页、`scratch/baseline-definition.md` 和 `scratch/midterm-report/README.md` 是否仍一致
