# 当前任务记忆

## 当前稳定节点
- 当前研究仓库稳定节点：`3.2.0`
- 这里的 `3.2.0` 指研究工作版本，不是 ns-3 框架版本
- ns-3 框架版本保持为 `3.46`
- 当前主仿真入口：`scratch/leo-ntn-handover-baseline.cc`

## 当前 baseline 快照
- 当前场景 baseline：`2x4` 双轨、`25 UE`、`hotspot-boundary`（热点增加 + 边界增强）二维部署
- 当前算法 baseline：传统 `A3` 风格切换，判决主线为 `RSRP + hysteresis + TTT + strict NRT guard`
- 当前改进方向：后续引入“信号质量 + 卫星负载”联合感知策略，但不改 baseline 的对照组定位

当前默认关键参数：
- `gNbNum = 8`
- `orbitPlaneCount = 2`
- `ueNum = 25`
- `interPlaneRaanSpacingDeg`（轨道面 RAAN 间隔）=`6 deg`
- `alignmentReferenceTimeSeconds`（对齐参考时刻）=`20 s`
- `updateIntervalMs`（主循环更新周期）=`100`
- `lambda`（业务流强度）=`1000 pkt/s/UE`
- `hoHysteresisDb`（切换迟滞门限）=`0.2 dB`
- `hoTttMs`（切换触发时间）=`1200 ms`

## 当前已确认实现
- 主脚本与运行时、统计、工具辅助头文件的拆分已经完成
- 周期更新已经拆开“卫星公共轨道状态”和“按 UE 派生观测几何”，用于减少重复轨道传播计算
- 当前默认启用六边形网格、严格 `NRT` 守卫和自定义 `A3` 风格切换执行路径
- 当前默认关闭高噪声的 `KPI`、`OVERPASS`、`GRID-ANCHOR` 输出，保留切换和最终汇总日志
- 当前新增周期性 `[Progress]` 日志，用于观察仿真推进
- 当前默认关闭 `SRS`（探测参考信号）调度相关项，避免与 handover 主线无关的 `PHY fatal`
- `SN Status Transfer`、`NrPdcp::DoReceivePdu()`、`UdpServer::HandleRead()` 相关崩溃防御已纳入

## 当前负载接口状态
- `SatelliteRuntime` 已具备 `attachedUeCount`、`offeredPacketRate`、`loadScore`、`admissionAllowed`
- 周期更新中已经会按当前接入 UE 数计算每星基础负载状态
- 这些量当前只用于观测和 trace 输出，尚未进入 baseline 切换决策

## 当前输出与脚本
- 默认结果目录：`scratch/results/`
- 当前默认输出：
  - `sat_beam_trace.csv`
  - `sat_attenuation_per_time.csv`（当 `runAttenuationScript = true`）
  - 六边形网格相关 `CSV/SVG`
- 当前常用分析脚本：
  - `scratch/sat_attenuation_report.py`
  - `scratch/plot_hex_grid_svg.py`

## 当前研究问题
- 继续验证传统 `A3` baseline 在当前双轨场景下是否能稳定暴露：
  - 频繁切换
  - 潜在 `ping-pong`
  - 触发过早或过晚
  - 吞吐连续性问题
- 将毕设任务书中的“信号质量 + 卫星负载”要求转化为后续改进算法的明确设计目标
- 在保持当前场景口径不变的前提下，为后续联合策略提供可解释的对照组

## 当前工作边界
- 当前不把“继续扩大星座规模”作为默认主线，除非现有 `2x4` 场景已无法体现算法差异
- 当前不再把“代码结构整理”作为第一优先级，除非算法实现需要新的结构调整
- 讨论或文档中提到关键参数时，优先使用“英文参数名（中文释义）”的写法
- 修改 `scratch/` 目录下的重要代码后，同步检查：
  - `scratch/README.md`
  - `scratch/baseline-definition.md`
  - 相关 `midterm-report` 文档

## 当前优先方向
- 稳住 `2x4` 双轨、多 UE baseline 场景
- 用统一口径验证传统 `A3` baseline 的局限
- 在不改变 baseline 定位的前提下，准备“信号质量 + 卫星负载”联合感知算法
