/*
 * 文件说明：
 * `b00-equivalent-antenna-model.h` 用于探索同频干扰的自定义天线模型。
 *
 * 设计目标：
 * - 使用 clipped-parabolic 公式（与旧 B00 几何波束同源）
 * - 通过数值标定让 PHY 总波束逼近旧 B00 几何理想波束（38 dBi / 4°）
 * - 参数可配置：阵元峰值增益、方向图宽度参数、旁瓣衰减上限
 *
 * 公式：
 * G_elem(psi) = G_elem_max - min(12 * (psi / theta_elem)^2, A_elem_max)
 *
 * 其中：
 * - psi：离轴角（相对阵元 boresight）
 * - G_elem_max：阵元峰值增益
 * - theta_elem：阵元方向图宽度参数（不是严格 -3 dB 宽度）
 * - A_elem_max：旁瓣衰减上限
 */

#ifndef B00_EQUIVALENT_ANTENNA_MODEL_H
#define B00_EQUIVALENT_ANTENNA_MODEL_H

#include "ns3/antenna-model.h"
#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/angles.h"

#include <cmath>

namespace ns3
{

/**
 * @ingroup antenna
 *
 * @brief B00 Equivalent Antenna Model for same-frequency interference exploration
 *
 * This antenna model uses a clipped-parabolic formula similar to the old B00
 * geometric beam budget. It is designed to be calibrated so that the total
 * beam (element pattern + UPA array factor + beamforming) approximates the
 * old B00 ideal beam (38 dBi / 4° beamwidth).
 *
 * The element power pattern is:
 * G_elem(psi) = G_elem_max - min(12 * (psi / theta_elem)^2, A_elem_max)
 *
 * where psi is the off-boresight angle in degrees and theta_elem is the
 * clipped-parabolic width parameter, not a strict -3 dB beamwidth.
 */
class B00EquivalentAntennaModel : public AntennaModel
{
  public:
    /**
     * @brief Get the type ID.
     * @return The object TypeId.
     */
    static TypeId GetTypeId();

    /**
     * @brief Constructor.
     */
    B00EquivalentAntennaModel();

    /**
     * @brief Destructor.
     */
    ~B00EquivalentAntennaModel() override;

    // inherited from AntennaModel
    double GetGainDb(Angles a) override;

    /**
     * @brief Set the maximum gain (dBi) at boresight.
     * @param maxGainDb the maximum gain in dBi
     */
    void SetMaxGainDb(double maxGainDb);

    /**
     * @brief Get the maximum gain (dBi) at boresight.
     * @return the maximum gain in dBi
     */
    double GetMaxGainDb() const;

    /**
     * @brief Set the clipped-parabolic width parameter in degrees.
     * @param beamwidthDeg the width parameter in degrees
     */
    void SetBeamwidthDeg(double beamwidthDeg);

    /**
     * @brief Get the clipped-parabolic width parameter in degrees.
     * @return the width parameter in degrees
     */
    double GetBeamwidthDeg() const;

    /**
     * @brief Set the maximum attenuation (dB) for side lobe suppression.
     * @param maxAttenuationDb the maximum attenuation in dB
     */
    void SetMaxAttenuationDb(double maxAttenuationDb);

    /**
     * @brief Get the maximum attenuation (dB) for side lobe suppression.
     * @return the maximum attenuation in dB
     */
    double GetMaxAttenuationDb() const;

  private:
    double m_maxGainDb;         //!< Maximum gain at boresight (dBi)
    double m_beamwidthDeg;      //!< Clipped-parabolic width parameter (degrees)
    double m_maxAttenuationDb;  //!< Maximum attenuation for side lobe (dB)
};

NS_OBJECT_ENSURE_REGISTERED(B00EquivalentAntennaModel);

TypeId
B00EquivalentAntennaModel::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::B00EquivalentAntennaModel")
            .SetParent<AntennaModel>()
            .SetGroupName("Antenna")
            .AddConstructor<B00EquivalentAntennaModel>()
            .AddAttribute("MaxGainDb",
                          "The maximum gain (dBi) at the antenna boresight",
                          DoubleValue(20.0),  // Initial estimate: 38 - 18 (array gain) = 20 dBi
                          MakeDoubleAccessor(&B00EquivalentAntennaModel::SetMaxGainDb,
                                             &B00EquivalentAntennaModel::GetMaxGainDb),
                          MakeDoubleChecker<double>())
            .AddAttribute("BeamwidthDeg",
                          "The clipped-parabolic width parameter in degrees. "
                          "This is an element-level formula parameter, not the total -3 dB beamwidth.",
                          DoubleValue(15.0),  // Initial estimate: need calibration
                          MakeDoubleAccessor(&B00EquivalentAntennaModel::SetBeamwidthDeg,
                                             &B00EquivalentAntennaModel::GetBeamwidthDeg),
                          MakeDoubleChecker<double>(0.0, 180.0))
            .AddAttribute("MaxAttenuationDb",
                          "The maximum attenuation (dB) for side lobe suppression",
                          DoubleValue(30.0),
                          MakeDoubleAccessor(&B00EquivalentAntennaModel::SetMaxAttenuationDb,
                                             &B00EquivalentAntennaModel::GetMaxAttenuationDb),
                          MakeDoubleChecker<double>(0.0));
    return tid;
}

B00EquivalentAntennaModel::B00EquivalentAntennaModel()
    : m_maxGainDb(20.0),
      m_beamwidthDeg(15.0),
      m_maxAttenuationDb(30.0)
{
}

B00EquivalentAntennaModel::~B00EquivalentAntennaModel()
{
}

void
B00EquivalentAntennaModel::SetMaxGainDb(double maxGainDb)
{
    m_maxGainDb = maxGainDb;
}

double
B00EquivalentAntennaModel::GetMaxGainDb() const
{
    return m_maxGainDb;
}

void
B00EquivalentAntennaModel::SetBeamwidthDeg(double beamwidthDeg)
{
    m_beamwidthDeg = beamwidthDeg;
}

double
B00EquivalentAntennaModel::GetBeamwidthDeg() const
{
    return m_beamwidthDeg;
}

void
B00EquivalentAntennaModel::SetMaxAttenuationDb(double maxAttenuationDb)
{
    m_maxAttenuationDb = maxAttenuationDb;
}

double
B00EquivalentAntennaModel::GetMaxAttenuationDb() const
{
    return m_maxAttenuationDb;
}

double
B00EquivalentAntennaModel::GetGainDb(Angles a)
{
    // Convert angles to degrees
    double phiDeg = RadiansToDegrees(a.GetAzimuth());
    double thetaDeg = RadiansToDegrees(a.GetInclination());

    // For axis-symmetric pattern, we compute off-boresight angle from both azimuth and inclination
    // The boresight is at phi=0, theta=90 (horizontal plane, pointing forward)

    // Compute horizontal off-boresight angle (azimuth component)
    double psiH = std::abs(phiDeg);

    // Compute vertical off-boresight angle (inclination component)
    // theta=90 is boresight (horizontal), so off-boresight is |theta - 90|
    double psiV = std::abs(thetaDeg - 90.0);

    // Total off-boresight angle (combined)
    double psiDeg = std::sqrt(psiH * psiH + psiV * psiV);

    // Clipped-parabolic formula (same as old B00 geometric beam budget)
    // G_elem(psi) = G_elem_max - min(12 * (psi / theta_elem)^2, A_elem_max)
    double patternLoss = std::min(12.0 * std::pow(psiDeg / m_beamwidthDeg, 2.0), m_maxAttenuationDb);

    double gainDb = m_maxGainDb - patternLoss;

    return gainDb;
}

} // namespace ns3

#endif // B00_EQUIVALENT_ANTENNA_MODEL_H
