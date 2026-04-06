# 毕设中期汇报流程图

本文件保留两张适合中期汇报展示的流程图：
- 图 1：当前统一测量驱动切换主线图
- 图 2：当前 baseline / improved 目标选择分叉图

如果 Markdown 预览支持 `Mermaid`，可以直接查看；如果 `PPT` 不方便嵌入 `Mermaid`，可截图后使用。

## 图 1：当前统一测量驱动切换主线图

图 1 对应当前平台的真实实现，突出以下内容：
- 三维轨道更新与 `ECI -> ECEF`
- `Earth-fixed` 地面六边形网格锚点
- `25 UE` 的 `seven-cell` 二维部署
- `UE` 局部偏移模板到 `WGS84/ECEF` 的统一位置生成
- 标准 `MeasurementReport` 上报与目标选择
- `RRC TriggerHandover`
- 切换统计与结果输出

```mermaid
sequenceDiagram
    autonumber
    participant UE as 地面UE
    participant SRC as 当前服务卫星/gNB
    participant RT as Runtime/UpdateConstellation
    participant A3 as A3 Measurement Algorithm
    participant DEC as Scenario Target Selection
    participant TGT as 目标卫星/gNB
    participant EPC as EPC/5GC
    participant REP as Reporting/Results

    Note over UE,SRC: 初始状态：UE 已接入当前服务卫星，业务流持续传输

    rect rgb(245,245,245)
        Note over RT: 阶段 1：三维轨道与 Earth-fixed 几何更新
        RT->>RT: 根据轨道参数更新卫星状态
        RT->>RT: ECI -> ECEF 转换，引入地球自转
        RT->>RT: 根据 UE 偏移模板生成地面位置
        RT->>RT: 计算 slant range / elevation / azimuth / Doppler
        RT->>RT: 更新 Earth-fixed 六边形网格锚点
        RT->>RT: 更新 attachedUeCount / offeredPacketRate / loadScore（平滑压力）
    end

    rect rgb(250,250,235)
        Note over SRC,A3: 阶段 2：标准 A3 测量上报
        SRC->>A3: 安装 A3 配置（hysteresis / TTT / report interval）
        UE-->>SRC: 上报 PHY MeasurementReport
        SRC->>A3: 转发匹配的 A3 测量结果
        A3->>DEC: 输出候选邻区 RSRP 列表
    end

    rect rgb(245,250,255)
        Note over DEC,TGT: 阶段 3：目标选择与切换执行
        alt handoverMode = baseline
            DEC->>DEC: 选择最强测量邻区
        else handoverMode = improved
            DEC->>DEC: 叠加 loadScore 与源站负载压力做联合评分
        end
        DEC->>SRC: TriggerHandover(targetCellId)
        SRC-->>UE: UE 切换到目标小区
        UE->>TGT: 重新建立目标侧服务关系
        TGT->>REP: 记录 HO-START / HO-END-OK
    end

    rect rgb(250,245,255)
        Note over TGT,EPC: 阶段 4：核心网与路径保持
        TGT->>EPC: 保持数据面/控制面上下文连续
        EPC-->>TGT: 路径继续指向当前目标服务星
    end

    rect rgb(245,255,245)
        Note over RT,REP: 阶段 5：运行时统计与结果输出
        RT->>REP: 输出 throughput / handover summary
        RT->>REP: 导出 ue_layout / sat_anchor_trace / grid catalog / grid svg / handover traces
    end

    Note over UE,TGT: 结束状态：UE 在新服务卫星下继续传输，等待下一轮周期更新
```

## 图 2：当前 baseline / improved 目标选择分叉图

图 2 对应当前平台的真实结构，说明 baseline 与 improved 已经在同一测量入口上形成对照。

重点突出：
- 当前平台已经统一的测量入口
- baseline 与 improved 的分叉位置
- 负载项如何只进入 improved，不回写 baseline

```mermaid
flowchart TD
    A[三维轨道更新<br/>ECI -> ECEF] --> B[星地几何量计算<br/>距离/仰角/方位角/多普勒]
    B --> C[Earth-fixed 网格锚点更新<br/>Hex Grid Anchor]
    C --> D[标准 A3 测量配置<br/>MeasurementReport]
    D --> E[候选邻区列表<br/>Measured RSRP]
    C --> G[负载状态统计<br/>attachedUeCount / offeredPacketRate / loadScore]

    E --> F[baseline 目标选择<br/>最强邻区]
    E --> H[improved 目标选择<br/>Signal + Load + Visibility Utility]
    G --> H

    F --> I[统一切换执行器<br/>TriggerHandover]
    H --> I

    I --> K[切换结果统计<br/>HO success / delay / throughput]
    K --> L[对比实验输出<br/>Baseline vs Improved]

    style A fill:#eef6ff,stroke:#4a90e2,stroke-width:1.5px
    style C fill:#f6f2ff,stroke:#8e63d2,stroke-width:1.5px
    style F fill:#fff6dd,stroke:#d4a017,stroke-width:1.5px
    style H fill:#ffe9ec,stroke:#d95c75,stroke-width:1.5px
    style L fill:#f3f3f3,stroke:#666,stroke-width:1.5px
```

## 汇报提示

讲图 1 时：
- 强调这张图对应当前平台的真实实现，而不是照搬地面蜂窝流程
- 突出三维轨道、`Earth-fixed` 网格、标准测量上报与统一切换执行链

讲图 2 时：
- 强调 baseline 与 improved 已经共用同一 `MeasurementReport` 入口
- 重点说明当前平台已有 `loadScore`、`attachedUeCount`、`offeredPacketRate` 等输入，当前差异只发生在目标选择阶段

## 建议标题
- `当前统一测量驱动切换主线图`
- `当前 baseline / improved 目标选择分叉图`
