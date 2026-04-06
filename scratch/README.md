# Scratch 目录说明

## 目录用途
- 本目录存放当前毕设使用的 LEO-NTN 切换仿真代码、辅助头文件和分析脚本
- 当前主要仿真入口：`scratch/leo-ntn-handover-baseline.cc`
- 最近已发布稳定节点：`4.1`（Git tag：`research-v4.1`）
- 当前工作区与 `research-v4.1` 对齐，当前主线已经统一到真实测量驱动的 baseline / improved 对照
- 这里的 `4.1.x` 是研究工作稳定节点，不是 ns-3 框架版本；ns-3 本身仍然是 `3.46`

## 当前主线
- 当前阶段先稳住基础组，再在此基础上验证和改进切换策略
- 当前工作区默认研究场景是：`2x4` 双轨、`25 UE`、`seven-cell` 二维部署
- 当前默认目标不是继续扩星，而是把该七小区场景作为传统 `A3 baseline` 的缺陷暴露平台
- 当前 handover 主链已统一到标准 `PHY/RRC MeasurementReport`
- 当前改进方向是在同一测量入口上推进“信号质量 + 剩余可见时间 + 卫星负载”联合目标选择，并逐步补上回切保护、可见性门控与源站负载感知的负载覆写门槛

## 当前版本判断
- 想确认“是不是进入新版本”，先看是否已打新的 `research-v4.1.x` 或 `research-v4.1` tag
- 目前最近已发布稳定节点是 `research-v4.1`
- 当前工作区中若继续补充结果、脚本或说明，默认按“`4.1` 之后的主阶段内继续迭代”理解，除非后续再打新 tag

## 当前默认口径
场景与参数：
- 卫星数：`8`
- 轨道面数：`2`
- UE 数：`25`
- UE 主布局：`seven-cell`
- `interPlaneRaanSpacingDeg`（轨道面 RAAN 间隔）=`-2 deg`
- `alignmentReferenceTimeSeconds`（对齐参考时刻）=`20 s`
- `simTime`（仿真时长）=`40 s`
- `updateIntervalMs`（主循环更新周期）=`100`
- `lambda`（业务流强度）=`1000 pkt/s/UE`
- `hoHysteresisDb`（切换迟滞门限）=`2.0 dB`
- `hoTttMs`（切换触发时间）=`200 ms`
- `measurementReportIntervalMs`（测量上报周期）=`120 ms`
- `measurementMaxReportCells`（单次最多上报邻区数）=`8`
- `handoverMode`（切换模式）默认=`baseline`
- `improvedSignalWeight`（改进策略信号权重）=`0.7`
- `improvedLoadWeight`（改进策略负载权重）=`0.3`
- `improvedVisibilityWeight`（改进策略可见性权重）=`0.2`
- `improvedMinLoadScoreDelta`（触发负载覆写所需的最小负载优势）=`0.2`
- `improvedMaxSignalGapDb`（允许负载覆写时相对最强信号候选的最大信号差）=`3.0 dB`
- `improvedReturnGuardSeconds`（短时回切保护窗口）=`1.5 s`
- `improvedMinVisibilitySeconds`（允许切入候选所需的最小剩余可见时间）=`1.0 s`
- `improvedVisibilityHorizonSeconds`（可见性评分归一化时间窗）=`8.0 s`
- `improvedVisibilityPredictionStepSeconds`（可见性向前预测步长）=`0.5 s`
- `pingPongWindowSeconds`（将 `A->B->A` 记为 `ping-pong` 的时间窗口）=`1.5 s`

切换口径：
- 当前算法 baseline 为传统 `A3` 风格切换
- baseline 与 improved 共用同一条 `MeasurementReport -> target selection -> TriggerHandover` 主链
- `handoverMode=baseline`：在 A3 上报候选中选择最强邻区
- `handoverMode=improved`：在同一批真实测量候选中联合考虑 `remainingVisibility` 与 `loadScore` 选目标，并加入过载候选过滤、短时回切保护、最小剩余可见时间门控和源站负载感知的负载优势门槛
- 当前 baseline 不使用负载做决策，但运行时已保留负载观测字段；`loadScore` 本身采用更平滑的压力近似，避免少量 UE 时过早打满
- 当前 PHY 信道已开启 `ShadowingEnabled`，切换判决直接读取真实 PHY/RRC 测量
- 原来的几何 `beam budget/custom A3` handover 代理链已经移除
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
  - 后续“信号质量 + 可见性 + 卫星负载”联合策略的设计说明、变量映射与数学表达
- `docs/research-workflow.md`
  - 版本、分支、提交和结果管理规则
- `scratch/baseline-definition.md`
  - baseline 的正式定义、默认参数口径、验证清单和改进边界
- `scratch/midterm-report/midterm-technical-summary.md`
  - 面向中期汇报的技术总结
- `scratch/midterm-report/midterm-handover-flowcharts.md`
  - 中期汇报流程图与简要讲解提示
- `scratch/midterm-report/midterm-ppt-design.md`
  - 中期答辩 PPT 逐页设计终稿
- `scratch/midterm-report/midterm-image-generation-spec.md`
  - 中期答辩图片生成与制图说明

当前目录收纳约定：
- `scratch/leo-ntn-handover-baseline.cc`
  - 主仿真入口，继续留在 `scratch/` 根目录
- `scratch/handover/`
  - handover 主线相关辅助头文件
- `scratch/plotting/`
  - 分析与绘图脚本

## 开发与运行流程
这部分只回答一个问题：在当前 `ns-3.46 + contrib/nr + scratch/leo-ntn-handover-baseline.cc` 工作流里，什么情况下该 `configure`，什么情况下只要 `build`，什么情况下可以直接 `run --no-build`。

### 1. 推荐的两套工作模式
建议长期固定成两套模式，不要混着用：

- `debug` 模式
  - 用途：查崩溃、断言失败、异常日志、切换逻辑是否跑偏
  - 特点：更容易排障，但运行慢
- `optimized` 模式
  - 用途：正式跑 baseline、扫参数、导出结果、长时间仿真
  - 特点：明显更快，适合实验

建议默认使用：

- 开发排障时：`debug`
- 正式实验时：`optimized`

### 2. 什么情况下要重新 `configure`
只有当“构建配置”发生变化时，才需要重新 `configure`。

常见触发条件：

- 在 `debug` 和 `optimized/release` 之间切换
- 想打开或关闭 `tests/examples`
- 修改了 `CMake` 相关配置
- 更换编译器、生成器或构建目录
- 构建缓存明显异常，需要重建配置

常用命令：

```bash
./ns3 configure -d debug
./ns3 configure -d optimized --disable-tests --disable-examples
```

说明：

- `-d debug`：保留断言和日志，适合排障
- `-d optimized`：关闭大量调试成本并打开优化，适合正式仿真
- `--disable-tests --disable-examples`：主要减少编译负担，不是直接改变仿真逻辑

### 3. 什么情况下只要 `build`
如果你改了 `.cc`、`.h` 里的实现逻辑，但没有改构建档位或 `CMake` 配置，通常只要重新编译，不用再次 `configure`。

典型例子：

- 改了 `leo-ntn-handover-baseline.cc`
- 改了 `leo-ntn-handover-update.h`
- 改了 `leo-ntn-handover-runtime.h`
- 改了 `contrib/nr/` 下的 `C++` 代码
- 改了 `leo-ntn-handover-config.h` 中的默认参数值

常用命令：

```bash
./ns3 build
```

然后运行：

```bash
./ns3 run --no-build "leo-ntn-handover-baseline"
```

这里推荐带上 `--no-build`，原因很简单：既然刚刚已经成功 `build` 过，就不要在 `run` 时再隐式触发一轮构建。

### 4. 什么情况下可以直接 `run --no-build`
如果你没有改任何 `C++` 源码，只是想换一组运行参数，那么可以直接运行，不需要重新编译。

典型例子：

- 想把 `simTime` 从 `40` 改到 `10`
- 想把 `lambda` 从 `1000` 改到 `100`
- 想测试不同的 `hoTttMs`
- 想临时关闭 `runGridSvgScript`

常用命令：

```bash
./ns3 run --no-build "leo-ntn-handover-baseline --simTime=10 --lambda=100"
./ns3 run --no-build "leo-ntn-handover-baseline --hoTttMs=400 --hoHysteresisDb=3.0"
./ns3 run --no-build "leo-ntn-handover-baseline --handoverMode=improved --runGridSvgScript=0"
```

适用前提：

- 之前已经成功编译过当前构建档位
- 这次没有改 `C++` 代码

### 5. 面向当前仓库的推荐操作流程
下面这套流程足够覆盖大多数日常工作。

#### 大范围改代码之后
适用情况：

- 改了多个 `.cc/.h`
- 改了 `scratch/` 和 `contrib/nr/` 的实现
- 不确定缓存是否仍然干净

建议流程：

```bash
./ns3 configure -d optimized --disable-tests --disable-examples
./ns3 build
./ns3 run --no-build "leo-ntn-handover-baseline"
```

如果当前目的是查 bug，而不是跑正式结果，把第一步换成：

```bash
./ns3 configure -d debug
```

#### 小范围改逻辑之后
适用情况：

- 只改了少量 `C++` 逻辑
- 没有切换构建档位
- 没动 `CMake`

建议流程：

```bash
./ns3 build
./ns3 run --no-build "leo-ntn-handover-baseline"
```

#### 只微调参数之后
适用情况：

- 只改命令行参数
- 不改源码

建议流程：

```bash
./ns3 run --no-build "leo-ntn-handover-baseline --simTime=10 --lambda=100"
```

#### 正式实验批量跑参数时
适用情况：

- 想扫 `TTT`、`hysteresis`、`lambda`
- 想连续多次运行
- 想控制 wall-clock

建议流程：

```bash
./ns3 configure -d optimized --disable-tests --disable-examples
./ns3 build
./ns3 run --no-build "leo-ntn-handover-baseline --simTime=40 ..."
```

原则：

- 用 `optimized`
- 尽量复用同一轮编译结果
- 运行阶段优先使用 `--no-build`

### 6. 构建缓存异常时怎么处理
如果你遇到下面这些症状，优先怀疑缓存或构建状态有问题：

- 明明改了代码，运行结果像没生效
- 构建目标名或库链接状态异常
- 切换过 `debug/optimized` 后行为很混乱
- `build` 或 `run` 反复出现难以解释的旧错误

## `4.1` 实验矩阵
本节给出当前 `research-v4.1` 主线下建议优先执行的一组实验编号表，目标是把后续研究重心集中到切换策略本身，而不是再回到旧的物理层观测入口争议。

当前这组实验的前提是：
- baseline 与 improved 已经统一到真实 `MeasurementReport` 入口
- “第二轨是否参与竞争”不再作为当前实验矩阵的待验证问题
- 场景边界继续固定为 `2x4` 双轨、`25 UE`、`seven-cell`

建议优先关注的指标：
- handover success rate
- handover delay
- throughput continuity
- ping-pong count
- load balance

| ID | 目标 | 关键参数 | 建议重复次数 | 主要用途 |
| --- | --- | --- | --- | --- |
| `B00` | baseline 验证基线 | `handoverMode=baseline hoTttMs=160 hoHysteresisDb=2.0` | `5` | 采用当前选定的 `E3` 高 `ping-pong` 暴露组，作为后续算法验证起点 |
| `B10` | baseline 短 TTT | `handoverMode=baseline hoTttMs=160` | `3` | 观察更激进触发是否增加频繁切换和 `ping-pong` |
| `B11` | baseline 中 TTT | `handoverMode=baseline hoTttMs=320` | `3` | 观察更稳健触发是否降低无效切换 |
| `B12` | baseline 长 TTT | `handoverMode=baseline hoTttMs=480` | `3` | 观察过晚切换是否损伤吞吐连续性 |
| `B20` | baseline 低 hysteresis | `handoverMode=baseline hoHysteresisDb=1.0` | `3` | 观察较低门限对边界切换的放大效应 |
| `B21` | baseline 高 hysteresis | `handoverMode=baseline hoHysteresisDb=3.0` | `3` | 观察较高门限对切换抑制和时延的影响 |
| `I00` | improved 默认权重 | `handoverMode=improved improvedSignalWeight=0.7 improvedLoadWeight=0.3` | `5` | 作为当前 improved 主对照组 |
| `I01` | improved 偏信号 | `handoverMode=improved improvedSignalWeight=0.8 improvedLoadWeight=0.2` | `3` | 观察轻度负载感知能否兼顾稳定性 |
| `I02` | improved 均衡权重 | `handoverMode=improved improvedSignalWeight=0.5 improvedLoadWeight=0.5` | `3` | 观察更强负载感知是否改善负载分布 |

建议执行顺序：
1. 先跑 `B00`，使用当前选定的 `E3` 参数组建立高 `ping-pong` baseline，对后续算法验证形成更强对照。
2. 再跑 `B10` 到 `B12`，单独看 `TTT` 的影响边界。
3. 然后跑 `B20`、`B21`，单独看 `hysteresis` 的影响边界。
4. 最后跑 `I00` 到 `I02`，在相同场景口径下比较目标选择收益。

论文第一轮推荐对照集：
- `B00`
- `B11`
- `B21`
- `I00`
- `I02`

每组实验建议至少记录：
- 版本：`research-v4.1`
- 运行命令
- `RngRun`
- 总切换次数
- 切换成功率
- 平均切换时延
- `ping-pong` 次数
- 切换附近吞吐波动
- 负载分布的定性结论

当前仓库已提供批量执行脚本：

```bash
scratch/run_handover_experiment_matrix.sh --list
scratch/run_handover_experiment_matrix.sh --group baseline-repeat --repeat 5
scratch/run_handover_experiment_matrix.sh --group improved-weight --repeat 3
```

说明：
- 这里的脚本层 `B00` 已切换到当前选定的 `E3` 参数组：`hoTttMs=160`、`hoHysteresisDb=2.0`
- 仓库正式 baseline 口径仍保持 `research-v4.1` 的默认参数定义；这里只是为了后续算法验证，先固定一个更能暴露 `ping-pong` 的实验起点

建议流程：

```bash
./ns3 clean
./ns3 configure -d optimized --disable-tests --disable-examples
./ns3 build
```

如果当前是深度排障，再改为：

```bash
./ns3 configure -d debug
```

### 7. 为什么正式实验更建议用 `optimized`
当前 baseline 默认就是高事件量场景：

- `25 UE`
- `lambda = 1000 pkt/s/UE`
- `updateIntervalMs = 100`
- 默认走 `MeasurementReport` 驱动的 `A3` 触发链

因此：

- `debug` 更适合查错
- `optimized` 更适合出结果

如果使用 `debug` 跑长时间 baseline，wall-clock 明显变慢是正常现象，不要把这种放慢直接误判成算法回归。

### 8. VS Code 里怎么用更顺手
当前工作区已经有 `.vscode/tasks.json`，其中比较实用的是：

- `ns3: build`
- `ns3: run target`
- `ns3: show targets`

因此在 VS Code 中，推荐优先使用：

- Terminal 直接执行 `./ns3 ...`
- 或者用现成的 `Tasks`

原因：

- `./ns3` 是 ns-3 官方 wrapper，最懂当前目标名和参数传递方式
- 对 `scratch` 程序来说，`./ns3 run "target --args"` 通常比直接找底层二进制路径更稳

### 9. CMake Tools 的 `Run in Terminal` 什么时候可用
如果你安装了 `CMake Tools` 扩展，`Run in Terminal` 通常要在下面条件满足后才会变得稳定可用：

- 工作区已经成功 `Configure`
- CMake 已经识别到一个可执行 `launch target`
- 当前不是只选中了一个库目标

如果它不可用，常见原因通常是：

- 还没成功 `configure`
- 当前没有选中可执行目标
- CMake Tools 还没识别到 `launch target`

对这个仓库，判断标准可以简单记成：

- 只要 `./ns3 build` 和 `./ns3 run ...` 已经正常工作，就不必强依赖 `Run in Terminal`
- `Run in Terminal` 更像 IDE 便捷入口，不是这个仓库的主工作流

### 10. 最推荐记住的 6 条命令
调试版重新配置：

```bash
./ns3 configure -d debug
```

实验版重新配置：

```bash
./ns3 configure -d optimized --disable-tests --disable-examples
```

重新编译：

```bash
./ns3 build
```

运行默认 baseline：

```bash
./ns3 run --no-build "leo-ntn-handover-baseline"
```

运行一组临时参数：

```bash
./ns3 run --no-build "leo-ntn-handover-baseline --simTime=10 --lambda=100"
```

缓存异常时重来一轮：

```bash
./ns3 clean
./ns3 configure -d optimized --disable-tests --disable-examples
./ns3 build
```

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
- 最终统计当前已包含整体与分 UE 的 `ping-pong` 自动计数
- 默认不强调 `OVERPASS`、`GRID-ANCHOR` 等高噪声信息

结果目录：
- 当前默认仿真输出统一写入 `scratch/results/`
- 常见结果包括：
  - `hex_grid_cells.csv`
  - `hex_grid_cells.svg`（当 `runGridSvgScript = true`）
  - `ue_layout.csv`
  - `sat_anchor_trace.csv`
  - `handover_dl_throughput_trace.csv`
  - `handover_event_trace.csv`

分析脚本：
- `plotting/plot_hex_grid_svg.py`
  - 读取六边形网格 `CSV`
  - 生成对应 `SVG`
  - 默认会尝试读取同目录下的 `ue_layout.csv`，用于导出 `grid + UE` 视图
  - 当前支持叠加 `sat_anchor_trace.csv`，用于导出“两轨代表主线”的轨迹视图
- `plotting/plot_handover_throughput.py`
  - 默认读取 `scratch/results/handover_dl_throughput_trace.csv`
  - 默认读取 `scratch/results/handover_event_trace.csv`
  - 面向单个 `UE` 的单次切换窗口，输出 `HO Start / HO Success` 对齐的吞吐连续性图

## 当前已完成的关键收口
- 主脚本与运行时、统计、工具辅助头文件的拆分已经完成
- 切换相关实时日志与最终汇总格式已经完成一轮收敛
- 当前已去掉旧的几何 `custom A3` handover 代理链，baseline 与 improved 统一走 `MeasurementReport`
- 当前已将周期更新中的卫星公共轨道传播与 handover 判决主链彻底分开
- 当前已将默认参数和合法性检查集中到 `leo-ntn-handover-config.h`
- 当前已接入 `loadScore`（负载评分）相关运行时字段与逐时刻 trace 输出
- 当前已将默认 `UE` 布局切换为 `seven-cell`，并补齐导出 `CSV/SVG` 的脚本链

## 接下来
- 先用当前默认参数完成一轮 `seven-cell baseline` 测量驱动切换验证
- 在相同 `MeasurementReport` 入口下，对比 baseline 与 improved 的目标选择差异
- 最后再根据实验结果细化联合目标函数和负载指标

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
本节中的旧参数、旧输出文件或旧脚本名称仅用于历史追溯，不代表当前默认实现。

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
  - 收紧当时的逐时刻导出格式，使多 UE 结果更便于分析
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
  - 当时的分析脚本默认输入输出路径同步迁移到 `scratch/results/`

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
  - 收口当时的辅助导出脚本，使结果更适合阶段性分析
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
  - 明确当时默认会生成额外的候选轨迹与中间分析文件
  - 明确可在 `runGridSvgScript = true` 时继续生成 `hex_grid_cells.svg`
  - 同步收口当前崩溃防御链说明，补齐 `SN Status Transfer`、`NrPdcp::DoReceivePdu()`、`UdpServer::HandleRead()` 的描述
- 配置入口收口
  - 新增 `leo-ntn-handover-config.h`
  - 将默认参数、命令行参数注册、输出路径收口和参数合法性检查从主脚本抽离
  - 本次调整不改变 `2x4` 双轨、`25 UE`、传统 `A3 baseline` 的场景口径
- 减法式清理
  - 去掉单 UE 兼容触发链，统一多 UE 主执行路径
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
- 稳定节点
  - 对应 tag：`research-v3.2.1`
  - 对应提交：`c692e68 chore(v3.2.1): snapshot tightened baseline and joint strategy docs`
  - 重点是收紧 baseline 与联合策略文档口径，而不是再定义一个新的场景版本

### `3.3.0`
- 最近已发布稳定节点
  - 对应 tag：`research-v3.3.0`
  - 当前保持 `3.2.2` baseline 语义与默认参数不变
  - 将主脚本保留在 `scratch/` 根目录，辅助头文件集中到 `scratch/handover/`
  - 将分析与绘图脚本集中到 `scratch/plotting/`
  - 清理历史结果、缓存、示例 scratch 目录与中期阶段性派生资产

### `4.1`
  - 对应 tag：`research-v4.1`
  - 对应提交：本次 `4.1` 发布提交
  - 当前正式收口“信号 + 可见性 + 负载”联合策略与源站负载感知负载均衡主线
  - improved 目标选择支持剩余可见时间门控、平滑负载压力与源站负载导向卸载
  - 将 `docs/`、`scratch/`、`baseline-definition` 与 `midterm-report/` 的当前口径同步到 `4.1`

### `4.0.1`
  - 当前稳定节点
  - 对应 tag：`research-v4.0.1`
  - 对应提交：本次 `4.0.1` 发布提交
  - 当前正式收口 measurement-driven baseline / improved 对照主线
  - 统一使用标准 `PHY/RRC MeasurementReport` 作为 handover 候选入口
  - 移除旧的几何 `beam budget/custom A3` handover 代理链及其派生 beam trace/report 输出
  - 周期更新主循环收紧为轨道推进、服务关系观测与负载统计
  - 将 `TTT` 归一化和 debounce 语义对齐到当前标准 A3 配置
  - 同步 `docs/`、`scratch/`、`baseline-definition` 与 `midterm-report/` 的版本和主线口径

## `4.1` 已发布包
- `research-v4.1` 代表当前 measurement-driven baseline / improved 主线下的最新稳定收口快照，重点是把 improved 收紧为“信号 + 可见性 + 负载”的联合策略，并增强源站负载感知的负载均衡能力

`v4.1` 已纳入的内容：
- `baseline` 与 `improved` 继续共用 `MeasurementReport -> target selection -> TriggerHandover` 主链
- `improved` 当前同时使用 `signal`、`remainingVisibility`、`loadScore` 做联合目标选择
- `loadScore` 从单纯人数比提升为“线性容量比 + 平滑饱和”的平滑压力分数
- 源站负载压力会动态增强负载项、放宽负载覆写门槛，并奖励能够减轻源站压力的候选
- 同步 `docs/`、`scratch/` 与中期汇报文档的版本和主线口径

## `4.0.1` 已发布包
- `research-v4.0.1` 代表当前 measurement-driven baseline / improved 主线的第一版稳定收口快照，重点是统一测量驱动切换入口、清理旧代理链，并把文档口径同步到同一条研究主线上

`v4.0.1` 已纳入的内容：
- `baseline` 与 `improved` 共用 `MeasurementReport -> target selection -> TriggerHandover` 主链
- `NrLeoA3MeasurementHandoverAlgorithm` 暴露标准测量回调，并支持场景侧自定义目标选择
- 删除 `sat_beam_trace.csv`、`sat_beam_report.csv` 及其派生脚本，保留更贴近当前主线的事件与吞吐输出
- 周期更新中保留每星 `attachedUeCount`、`offeredPacketRate`、`loadScore`、`admissionAllowed` 统计
- 同步 `docs/`、`scratch/` 与中期汇报文档的路径、参数和版本口径

`v4.0.1` 发布前检查已收口为：
- 默认参数下跑通一轮 `seven-cell baseline`
- 确认 `scratch/results/` 输出链完整
- 确认 baseline 文档、README 与中期汇报技术总结的参数口径一致
- 确认 measurement-driven baseline / improved 的当前代码与文档描述一致

当前若继续做中期汇报整理，优先参考：
- `scratch/midterm-report/midterm-image-generation-spec.md`
- `scratch/midterm-report/midterm-ppt-design.md`
