# 毕设中期汇报流程图

本文件保留两张适合中期汇报展示的流程图：
- 图 1：当前 `baseline` 的切换时序图
- 图 2：下一阶段“信号质量 + 卫星负载”联合策略的接入图

如果 Markdown 预览支持 `Mermaid`，可以直接查看；如果 `PPT` 不方便嵌入 `Mermaid`，可截图后使用。

## 图 1：当前 LEO-NTN baseline 切换时序图

图 1 对应当前平台的真实实现，突出以下内容：
- 三维轨道更新与 `ECI -> ECEF`
- `Earth-fixed` 地面六边形网格锚点
- `25 UE` 的 `hotspot-boundary` 二维部署
- `strict NRT guard`
- 自定义 `A3` 风格判决
- `RRC TriggerHandover`
- 切换统计与结果输出

```mermaid
sequenceDiagram
    autonumber
    participant UE as 地面UE
    participant SRC as 当前服务卫星/gNB
    participant RT as Runtime/UpdateConstellation
    participant TGT as 候选目标卫星/gNB
    participant EPC as EPC/5GC
    participant REP as Reporting/Results

    Note over UE,SRC: 初始状态：UE 已接入当前服务卫星，业务流持续传输

    rect rgb(245,245,245)
        Note over RT: 阶段 1：三维轨道与 Earth-fixed 几何更新
        RT->>RT: 根据轨道参数更新卫星状态
        RT->>RT: ECI -> ECEF 转换，引入地球自转
        RT->>SRC: 更新服务卫星位置
        RT->>TGT: 更新候选卫星位置
        RT->>RT: 计算 slant range / elevation / azimuth / Doppler
        RT->>RT: 更新 Earth-fixed 六边形网格锚点
        RT->>RT: 计算每颗卫星对 UE 的 beam budget / RSRP
    end

    rect rgb(250,250,235)
        Note over RT,SRC: 阶段 2：候选邻区筛选与 baseline 判决
        RT->>RT: 根据可见性与 beam lock 判定候选星
        RT->>RT: strict NRT guard 过滤无效邻区
        RT->>SRC: 动态维护 activeNeighbours
        alt 目标星优于服务星且满足 Hysteresis
            loop 持续满足 TTT
                RT->>RT: 维持 manualHoCandidateSince
            end
            RT->>SRC: ExecuteCustomA3Handover()
        else 不满足切换条件
            RT->>RT: 保持当前服务星不变
        end
    end

    rect rgb(245,250,255)
        Note over SRC,TGT: 阶段 3：切换执行
        SRC->>SRC: 生成 HO-START
        SRC->>TGT: TriggerHandover(targetCellId)
        TGT-->>SRC: 切换准备完成
        SRC-->>UE: UE 切换到目标小区
        UE->>TGT: 重新建立目标侧服务关系
        TGT->>TGT: 生成 HO-END-OK
    end

    rect rgb(250,245,255)
        Note over TGT,EPC: 阶段 4：核心网与路径保持
        TGT->>EPC: 保持数据面/控制面上下文连续
        EPC-->>TGT: 路径继续指向当前目标服务星
    end

    rect rgb(245,255,245)
        Note over RT,REP: 阶段 5：运行时统计与结果输出
        RT->>RT: 更新 attachedUeCount / offeredPacketRate / loadScore
        RT->>REP: 记录 HO start / HO success / delay
        RT->>REP: 输出 throughput / handover summary
        RT->>REP: 导出 sat_beam_trace / grid catalog / attenuation data
    end

    Note over UE,TGT: 结束状态：UE 在新服务卫星下继续传输，等待下一轮周期更新
```

## 图 2：下一阶段“信号质量 + 卫星负载”联合策略接入图

图 2 不表示“已经完成”，而是说明下一阶段准备在现有平台上的插入位置。

重点突出：
- 当前平台已经具备的输入量
- 联合策略模块的接入位置
- 新策略相对于 baseline 的升级点

```mermaid
flowchart TD
    A[三维轨道更新<br/>ECI -> ECEF] --> B[星地几何量计算<br/>距离/仰角/方位角/多普勒]
    B --> C[Earth-fixed 网格锚点更新<br/>Hex Grid Anchor]
    C --> D[链路预算计算<br/>RSRP / Beam Lock / Visibility]

    D --> E[候选星集合构建<br/>strict NRT guard]
    E --> F[当前 baseline<br/>A3-style: RSRP + Hys + TTT]

    D --> G[负载状态统计<br/>attachedUeCount / offeredPacketRate / loadScore]
    G --> H[联合评估模块<br/>Signal + Load Utility]

    F --> I[当前切换执行器<br/>TriggerHandover]
    H --> J[下一阶段联合切换执行器]

    I --> K[切换结果统计<br/>HO success / delay / throughput]
    J --> K

    K --> L[对比实验输出<br/>Baseline vs Joint Strategy]

    style A fill:#eef6ff,stroke:#4a90e2,stroke-width:1.5px
    style C fill:#f6f2ff,stroke:#8e63d2,stroke-width:1.5px
    style F fill:#fff6dd,stroke:#d4a017,stroke-width:1.5px
    style H fill:#ffe9ec,stroke:#d95c75,stroke-width:1.5px
    style J fill:#e8fff3,stroke:#2c9f62,stroke-width:1.5px
    style L fill:#f3f3f3,stroke:#666,stroke-width:1.5px
```

## 汇报提示

讲图 1 时：
- 强调这张图对应当前平台的真实实现，而不是照搬地面蜂窝流程
- 突出三维轨道、`Earth-fixed` 网格、`strict NRT guard` 和自定义 `A3` 执行链

讲图 2 时：
- 强调这是下一阶段接入路径，不要讲成“已经完成”
- 重点说明当前平台已有 `loadScore`、`attachedUeCount`、`offeredPacketRate` 等输入，不需要推翻重来

## 建议标题
- `当前 LEO-NTN Baseline 切换时序图`
- `联合信号-负载策略的后续接入路径`
