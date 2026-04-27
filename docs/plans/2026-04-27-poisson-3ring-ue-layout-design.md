# UE 布局重构设计：poisson-3ring

## 背景

当前 `seven-cell` 布局存在核心问题：
- 8 颗卫星仅有 7 个小区，必然有卫星过剩找不到 UE
- 中心小区密集 9 UE，外围小区仅 2-3 UE，负载分布不合理
- UE 固定偏移模板，无法模拟真实负载差异

## 设计目标

设计新的 `poisson-3ring` 布局替代 `seven-cell` 作为正式 baseline：
- 小区数接近卫星数，让每颗卫星有足够候选小区
- 降低 UE 密度，每小区平均 1-2 UE
- 使用泊松分布模拟真实负载差异，允许空置小区

## 小区布局

### 结构
- 完整 3-ring hex grid
- 中心 1 + 二跳 6 + 三跳 12 = 19 小区
- 小区半径：保持 `hexCellRadiusKm = 20 km`

### 生成逻辑
沿用现有 WGS84 hex grid 生成逻辑：
- 以 `gridCenterLatitudeDeg / gridCenterLongitudeDeg` 为中心
- 使用 `wgs84-hex-grid.h` 的 `GenerateWgs84HexGridCells` 函数
- 取前 19 个 cell（按 ring 顺序：中心→二跳→三跳）

## UE 分布

### 分配方式：截断泊松分布
每小区 UE 数按 Poisson(λ=1.5) 生成，截断上限 5 UE/小区：
```
for each cell in 19 cells:
    ue_count = min(5, Poisson(λ=1.5))
```

### 期望统计
- λ=1.5，截断到 5 后单小区期望约 1.49 UE
- 总期望：19 × 1.49 ≈ 28 UE
- 空置小区概率：P(X=0) = e^{-1.5} ≈ 22%

### 位置放置：hex 内均匀随机
- 以小区 hex 中心为原点
- 在正六边形区域内随机撒点（东-北平面偏移）
- 均匀随机采样：先在矩形内随机，再 reject 落在六边形外的点
- 最后统一转换为 WGS84 地理点和 ECEF

## 实现方案

### 代码修改点

1. **leo-ntn-handover-config.h**
   - `BaselineSimulationConfig` 中 `ueLayoutType` 默认值改为 `"poisson-3ring"`
   - 移除 `ueNum` 的固定值约束
   - 新增 `poissonLambda` 参数（默认 1.5）
   - 新增 `maxUePerCell` 参数（默认 5）

2. **leo-ntn-handover-utils.h**
   - 新增 `GeneratePoisson3RingUePlacements` 函数
   - 实现截断泊松分配和 hex 内随机位置生成

3. **leo-ntn-handover-baseline.cc**
   - 在布局分支中增加 `poisson-3ring` case
   - 调用新的 UE 生成函数
   - 更新日志输出

4. **ValidateBaselineSimulationConfig**
   - 移除 `seven-cell` 的 `ueNum == 25` 约束
   - 新增 `poisson-3ring` 的参数校验
   - `ueNum` 改为可选上限（若指定则截断总数）

### 与现有布局对比

| 属性 | seven-cell（旧） | poisson-3ring（新） |
|---|---|---|
| 小区数 | 7 | 19 |
| UE 数 | 固定 25 | 泊松 λ=1.5，约 28 |
| UE 分布 | 中心密集 9 + 外围 16 | 每小区 Poisson，允许空置 |
| UE 位置 | 固定偏移模板 | hex 内均匀随机 |

### 向后兼容
- `seven-cell` 和 `r2-diagnostic` 布局保留，通过 `ueLayoutType` 参数切换
- 历史实验可继续使用旧布局复现

## 测试验证

### 功能验证
1. 运行 `--ueLayoutType=poisson-3ring` 能正常启动
2. 检查 `ue_layout.csv` 输出正确
3. 检查 `hex_grid_cells.csv` 包含 19 个小区
4. 验证 UE 数约 28，分布在 19 个小区内

### 研究验证
1. 8 颗卫星不再出现明显过剩
2. 切换现象能正常暴露
3. baseline / improved 能拉开差异
4. E2E delay、packet loss、SINR、ping-pong、load balance 可统计

## 文档更新

完成后同步更新：
- `docs/current-task-memory.md`
- `scratch/baseline-definition.md`
- `scratch/README.md`