# LEO-NTN 仿真链路预算参数说明

这份文档只做一件事：说明当前工作区里几何链路预算如何从真实 PHY 默认天线参数推导，避免再维护两套手写口径。

## 一、发射功率

| 参数 | 变量名 | 默认值 | 说明 |
|------|--------|--------|------|
| 卫星下行发射功率 | `gnbTxPower` | `100.0 dBm` | 仿真里 gNB 侧配置值 |
| UE 上行发射功率 | `ueTxPower` | `23.0 dBm` | 仿真里 UE 侧配置值 |

功率换算：
- `23 dBm` 约等于 `200 mW`
- `100 dBm` 等于 `10 MW`

这里的 `gnbTxPower=100 dBm` 是当前仿真配置参数，不应直接当成真实卫星平台的工程发射功率结论。

## 二、两套“波束”口径

### 2.1 几何链路预算口径

这组参数服务于 [beam-link-budget.h](/Users/mac/Desktop/workspace/ns-3.46/scratch/handover/beam-link-budget.h) 的几何估算。当前不再提供 `beamMaxGainDbi`、`theta3dBDeg`、`sideLobeAttenuationDb` 三个独立命令行参数，而是从真实 PHY 配置自动推导：

| 参数 | 变量名 | 默认值 | 说明 |
|------|--------|--------|------|
| 波束峰值增益 | `gMax0Dbi` | `b00MaxGainDb + 10*log10(gnbAntennaRows * gnbAntennaColumns)` | 几何主瓣中心最大增益 |
| 波束宽度参数 | `theta3dBRad` | `DegToRad(b00BeamwidthDeg)` | clipped-parabolic 公式分母参数 |
| 旁瓣衰减上限 | `slaVDb` | `b00MaxAttenuationDb` | 几何离轴损耗封顶 |
| UE 接收增益 | `ueRxGainDbi` | `0.0 dBi` | 几何链路预算里的 UE 增益口径 |

当前几何损耗形式为：

```text
Loss_geo = min(12 * (psi / theta)^2, Amax)
```

因此在当前实现里：
- `psi = 2°`、`theta = 4°` 时，损耗约为 `3 dB`
- `psi = 4°` 时，损耗约为 `12 dB`

也就是说，`b00BeamwidthDeg=4°` 在几何估算里也更接近“公式参数”，不是严格意义上的“整束 -3 dB 宽度”。

### 2.2 真实 PHY 默认天线口径

当前稳定默认真实 PHY 为：
- `gNB b00-custom + UE three-gpp`
- `gNB 12x12 UPA`
- `UE 1x2`
- `beamformingMode=ideal-earth-fixed`
- `earthFixedBeamTargetMode=grid-anchor`

`b00-custom` 当前默认参数：

| 参数 | 变量名 | 默认值 | 说明 |
|------|--------|--------|------|
| 阵元峰值增益 | `b00MaxGainDb` | `20.0 dBi` | 阵元主轴方向最大增益 |
| 阵元宽度参数 | `b00BeamwidthDeg` | `4.0°` | clipped-parabolic 公式参数，不是严格 `-3 dB` 宽度 |
| 阵元旁瓣衰减上限 | `b00MaxAttenuationDb` | `30.0 dB` | 阵元方向图最大衰减 |

当前 `b00-custom` 方向图同样采用：

```text
Loss_elem = min(12 * (psi / theta_elem)^2, Aelem_max)
```

因此在当前实现里：
- `psi = theta_elem` 时，衰减是 `12 dB`
- `b00BeamwidthDeg=4°` 表示阵元公式参数，不应写成“严格 -3 dB 半功率宽度”

按当前工作区默认 `gNB 12x12 UPA + b00-custom + ideal-earth-fixed` 数值扫总方向图，
`b00BeamwidthDeg=4.0` 时主瓣 `-3 dB` 地面半径约为 `20 km` 量级；
早期 `15.0` 口径对应的真实主瓣则接近 `50 km` 级别。
这里的 `20 km` 结论来自当前实现公式的数值计算，不是程序现成导出的运行时字段。

## 三、阵列规模与总增益解释

当前默认阵列规模：
- 卫星端：`12 x 12 = 144` 阵元
- UE 端：`1 x 2 = 2` 阵元

如果只做非常粗略的 boresight 叠加估算：
- 阵元峰值增益约 `20 dBi`
- `144` 阵元阵列因子上限约 `21.6 dB`

所以当前默认几何主轴方向增益量级约为 `41.6 dBi`。  
但这只是数量级解释，不等价于：
- 实际发射 `EIRP`
- 实测 NR `RSRP`
- 任意离轴角下的真实总方向图

## 四、自由空间路径损耗

自由空间损耗随卫星到 UE 距离和载波频率动态变化：

$$L_{\text{FSPL}} = 20 \log_{10}\left(\frac{4\pi d f}{c}\right)\ \text{dB}$$

其中：
- $d$：卫星到 UE 的距离（m）
- $f$：载波频率，当前默认 `2 GHz`
- $c$：光速

在 `600 km` 高度、`2 GHz` 下的典型量级：

| 场景 | 距离 | FSPL |
|------|------|------|
| 星下点附近 | `600 km` | 约 `154 dB` |
| 低仰角长斜距 | `~1800 km` | 约 `169 dB` |

## 五、如何理解“粗链路预算”和“真实测量值”

可以用下面这个粗公式理解主轴方向的接收功率量级：

$$P_{\text{rx}} \approx P_{\text{tx}} + G_{\text{tx}} + G_{\text{rx}} - L_{\text{FSPL}} - L_{\text{atm}}$$

但这里更合适的名字是“接收功率粗估”，不是“真实 NR RSRP”。

原因是当前代码里的真实测量值还会受到：
- NR 资源元素口径
- 阵列波束成形
- 离轴损耗
- `ThreeGpp` 路径损耗与阴影衰落
- 同频干扰

所以控制台或 CSV 里常见的 `RSRP ~ -110 dBm`，并不和这种粗链路预算公式直接一一对应。

## 六、参数来源

| 参数类别 | 来源文件 |
|----------|----------|
| 仿真配置默认值 | [leo-ntn-handover-config.h](/Users/mac/Desktop/workspace/ns-3.46/scratch/handover/leo-ntn-handover-config.h) |
| `b00-custom` 阵元模型 | [b00-equivalent-antenna-model.h](/Users/mac/Desktop/workspace/ns-3.46/scratch/handover/b00-equivalent-antenna-model.h) |
| 几何链路预算 | [beam-link-budget.h](/Users/mac/Desktop/workspace/ns-3.46/scratch/handover/beam-link-budget.h) |

参数可通过命令行覆盖，例如：

```bash
./ns3 run "leo-ntn-handover-baseline --gnbTxPower=90 --b00MaxGainDb=22"
```

## 七、当前工作区口径

- 已发布稳定节点：`research-v6.0`
- 当前默认真实天线配置：`gNB b00-custom + UE three-gpp`
- 当前默认波束方法：`ideal-earth-fixed`

如果要解释当前高 `PHY DL TB error rate`，应优先考虑同频干扰、阵列定向性和离轴/旁瓣影响，而不是只看这份几何链路预算表。
