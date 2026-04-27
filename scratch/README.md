# Scratch 目录索引

本目录保留当前 thesis mainline 使用的仿真入口、辅助头文件、绘图脚本和结果暂存区。

当前正式 baseline 口径不在这里展开重写，仍按固定 `2x4` 双轨、`25 UE`、`seven-cell`、传统 `A3` 比较路径理解；`r2-diagnostic` 继续只作为诊断入口，不替代正式 baseline。

当前还保留 `ueLayoutType=poisson-3ring` 作为可选空间负载分布入口：它在中心及两圈共 `19` 个 hex cell 内按截断泊松权重分配 `UE`，用于检查更分散/不均匀需求对锚点、负载和切换竞争的影响；该入口不改变默认 `seven-cell` baseline。

当前还支持 `realLinkGateMode=beam-only|off` 作为诊断入口，用于放宽真实接入/切换候选 gate，检查 measurement-driven `improved` 是否因为候选集合过窄而退化成 baseline。

当前还支持 `handoverMode=improved-score-only` 作为诊断入口：它复用与 baseline 相同的 `MeasurementReport` 候选和 real-link gate，只保留 `signal + load + visibility` 联合评分，不再叠加 weak-link fallback、stable-lead、joint-margin、candidate quality/visibility hard gate 等保护层，用于快速判断“联合评分本身”是否值得继续细化。

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
- 当前默认更偏向快速迭代：`FlowMonitor` 与卫星轨迹 trace 默认关闭；正式 KPI 复跑时再显式开启
- 正式论文输出保留业务 KPI、切换事件、`satellite_state_trace.csv`、`hex_grid_cells.html`、`sat_ground_track.csv` 与 `sat_anchor_trace.csv`；PHY/SINR/TBler 输出只作为隐藏诊断入口
- `scratch/results/` 默认只放临时输出；长期保留结果按 `docs/research-workflow.md` 记录
- 修改 `scratch/` 重要内容后，同步检查本页、`scratch/baseline-definition.md` 和 `scratch/midterm-report/README.md` 是否仍一致
