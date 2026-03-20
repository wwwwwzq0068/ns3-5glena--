#ifndef BEAM_LINK_BUDGET_H
#define BEAM_LINK_BUDGET_H

/*
 * 文件说明：
 * `beam-link-budget.h` 用于计算卫星波束指向地面 UE 时的简化链路预算。
 *
 * 当前模型侧重基础组场景需要的几个核心量：
 * - 扫描角；
 * - 偏离波束中心的离轴角；
 * - 扫描损耗与方向图损耗；
 * - 自由空间路径损耗；
 * - 最终得到的近似 RSRP。
 *
 * 本文件不涉及切换决策，只提供链路质量估计。
 */

#include "leo-orbit-calculator.h"
#include "ns3/vector.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ns3
{

struct BeamModelConfig
{
    /** 载波频率，单位 Hz。 */
    double carrierFrequencyHz = 2e9;

    /** 发射功率，单位 dBm。 */
    double txPowerDbm = 30.0;

    /** 波束中心最大增益，单位 dBi。 */
    double gMax0Dbi = 38.0;

    /** 最大允许扫描角，超过该角度后认为波束无法锁定。 */
    double alphaMaxRad = LeoOrbitCalculator::DegToRad(60.0);

    /** 半功率波束宽度，单位弧度。 */
    double theta3dBRad = LeoOrbitCalculator::DegToRad(4.0);

    /** 垂直方向旁瓣衰减上限，单位 dB。 */
    double slaVDb = 30.0;

    /** 接收端天线增益，单位 dBi。 */
    double rxGainDbi = 0.0;

    /** 大气附加损耗，单位 dB。 */
    double atmLossDb = 0.5;

    /** 当前版本预留的波束失锁惩罚项，单位 dB。 */
    double beamDropPenaltyDb = 200.0;

    /** 是否将阴影衰落注入自定义 A3 观测链。 */
    bool enableCustomA3Shadowing = false;

    /** 自定义 A3 阴影衰落标准差，单位 dB。 */
    double customA3ShadowingSigmaDb = 1.0;

    /** 自定义 A3 阴影衰落时间相关尺度，单位秒。 */
    double customA3ShadowingCorrelationSeconds = 4.0;

    /** 是否将莱斯快衰落注入自定义 A3 观测链。 */
    bool enableCustomA3RicianFading = false;

    /** 自定义 A3 莱斯衰落的 K 因子，单位 dB。 */
    double customA3RicianKDb = 15.0;

    /** 自定义 A3 莱斯衰落时间相关尺度，单位秒。 */
    double customA3RicianCorrelationSeconds = 1.0;
};

struct BeamLinkBudget
{
    /** 波束相对星下点方向的扫描角。 */
    double scanAngleRad = 0.0;

    /** UE 相对波束主轴的离轴角。 */
    double offBoresightAngleRad = 0.0;

    /** 扫描损耗，单位 dB。 */
    double scanLossDb = 0.0;

    /** 方向图损耗，单位 dB。 */
    double patternLossDb = 0.0;

    /** 当前方向下的发射天线增益，单位 dBi。 */
    double txGainDbi = -std::numeric_limits<double>::infinity();

    /** 自由空间路径损耗，单位 dB。 */
    double fsplDb = 0.0;

    /** 计算得到的接收功率近似值，单位 dBm。 */
    double rsrpDbm = -std::numeric_limits<double>::infinity();

    /** 叠加随机扰动前的几何 RSRP，单位 dBm。 */
    double geometryRsrpDbm = -std::numeric_limits<double>::infinity();

    /** 注入到自定义 A3 观测链中的阴影衰落量，单位 dB。 */
    double customA3ShadowingDb = 0.0;

    /** 注入到自定义 A3 观测链中的莱斯快衰落量，单位 dB。 */
    double customA3RicianFadingDb = 0.0;

    /** 当前波束是否仍处于可锁定范围内。 */
    bool beamLocked = false;

    /** 当前链路预算是否成功完成计算。 */
    bool valid = false;
};

/** 计算两个三维向量的点积。 */
inline double
Dot3(const Vector& a, const Vector& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/** 计算三维向量模长。 */
inline double
Norm3(const Vector& v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

/** 计算两个向量夹角，返回值单位为弧度。 */
inline double
AngleRad(const Vector& a, const Vector& b)
{
    const double denom = Norm3(a) * Norm3(b);
    if (denom <= 0.0)
    {
        return 0.0;
    }
    return std::acos(std::clamp(Dot3(a, b) / denom, -1.0, 1.0));
}

/**
 * 计算地固系波束模型下，从卫星到 UE 的简化链路预算。
 *
 * 输入：
 * - `satEcef`：卫星在 ECEF 坐标系中的位置；
 * - `ueEcef`：地面 UE 在 ECEF 坐标系中的位置；
 * - `cellAnchorEcef`：当前波束锚点在 ECEF 坐标系中的位置；
 * - `cfg`：波束模型配置。
 */
inline BeamLinkBudget
CalculateEarthFixedBeamBudget(const Vector& satEcef,
                              const Vector& ueEcef,
                              const Vector& cellAnchorEcef,
                              const BeamModelConfig& cfg)
{
    BeamLinkBudget budget;

    const Vector vNadir(-satEcef.x, -satEcef.y, -satEcef.z);
    const Vector vBeam(cellAnchorEcef.x - satEcef.x,
                       cellAnchorEcef.y - satEcef.y,
                       cellAnchorEcef.z - satEcef.z);
    const Vector vUe(ueEcef.x - satEcef.x, ueEcef.y - satEcef.y, ueEcef.z - satEcef.z);

    const double rangeMeters = Norm3(vUe);
    if (rangeMeters <= 0.0 || Norm3(vBeam) <= 0.0 || Norm3(vNadir) <= 0.0 || cfg.theta3dBRad <= 0.0)
    {
        return budget;
    }

    budget.valid = true;
    budget.scanAngleRad = AngleRad(vBeam, vNadir);
    budget.offBoresightAngleRad = AngleRad(vUe, vBeam);
    budget.beamLocked = (budget.scanAngleRad < cfg.alphaMaxRad);

    if (budget.beamLocked)
    {
        const double cosAlpha = std::max(std::cos(budget.scanAngleRad), 1e-9);
        budget.scanLossDb = -10.0 * std::log10(std::pow(cosAlpha, 1.5));
    }
    else
    {
        budget.scanLossDb = std::numeric_limits<double>::infinity();
    }

    budget.patternLossDb =
        std::min(12.0 * std::pow(budget.offBoresightAngleRad / cfg.theta3dBRad, 2.0), cfg.slaVDb);

    if (budget.beamLocked)
    {
        budget.txGainDbi = cfg.gMax0Dbi - budget.scanLossDb - budget.patternLossDb;
    }

    budget.fsplDb = 20.0 * std::log10((4.0 * LeoOrbitCalculator::kPi * rangeMeters *
                                        cfg.carrierFrequencyHz) /
                                       LeoOrbitCalculator::kSpeedOfLight);

    if (budget.beamLocked && std::isfinite(budget.txGainDbi))
    {
        budget.geometryRsrpDbm =
            cfg.txPowerDbm + budget.txGainDbi + cfg.rxGainDbi - budget.fsplDb - cfg.atmLossDb;
        budget.rsrpDbm = budget.geometryRsrpDbm;
    }

    return budget;
}

} // namespace ns3

#endif // BEAM_LINK_BUDGET_H
