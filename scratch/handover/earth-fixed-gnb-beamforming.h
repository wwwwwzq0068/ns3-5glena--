#ifndef EARTH_FIXED_GNB_BEAMFORMING_H
#define EARTH_FIXED_GNB_BEAMFORMING_H

#include "ns3/angles.h"
#include "ns3/beamforming-vector.h"
#include "ns3/ideal-beamforming-algorithm.h"
#include "ns3/log.h"
#include "ns3/net-device.h"
#include "ns3/nr-spectrum-phy.h"
#include "ns3/nr-wraparound-utils.h"
#include "ns3/object.h"
#include "ns3/uniform-planar-array.h"

#include <cmath>
#include <functional>
#include <optional>

namespace ns3
{

class EarthFixedGnbBeamforming : public IdealBeamformingAlgorithm
{
  public:
    using AnchorResolver = std::function<std::optional<Vector>(const Ptr<const NetDevice>&)>;

    static TypeId GetTypeId();

    static void SetAnchorResolver(AnchorResolver resolver);

    BeamformingVectorPair GetBeamformingVectors(const Ptr<NrSpectrumPhy>& gnbSpectrumPhy,
                                                const Ptr<NrSpectrumPhy>& ueSpectrumPhy) const override;

  private:
    static AnchorResolver& GetAnchorResolverStorage();

    static std::optional<Vector> ResolveAnchorPosition(const Ptr<const NetDevice>& gnbDevice);

    static PhasedArrayModel::ComplexVector CreateEarthFixedGnbWeights(
        const Ptr<NrSpectrumPhy>& gnbSpectrumPhy,
        const Vector& anchorPosition);
};

NS_LOG_COMPONENT_DEFINE("EarthFixedGnbBeamforming");
NS_OBJECT_ENSURE_REGISTERED(EarthFixedGnbBeamforming);

inline TypeId
EarthFixedGnbBeamforming::GetTypeId()
{
    static TypeId tid = TypeId("ns3::EarthFixedGnbBeamforming")
                            .SetParent<IdealBeamformingAlgorithm>()
                            .AddConstructor<EarthFixedGnbBeamforming>();
    return tid;
}

inline EarthFixedGnbBeamforming::AnchorResolver&
EarthFixedGnbBeamforming::GetAnchorResolverStorage()
{
    static AnchorResolver resolver;
    return resolver;
}

inline void
EarthFixedGnbBeamforming::SetAnchorResolver(AnchorResolver resolver)
{
    GetAnchorResolverStorage() = std::move(resolver);
}

inline std::optional<Vector>
EarthFixedGnbBeamforming::ResolveAnchorPosition(const Ptr<const NetDevice>& gnbDevice)
{
    const auto& resolver = GetAnchorResolverStorage();
    if (!resolver)
    {
        return std::nullopt;
    }
    return resolver(gnbDevice);
}

inline PhasedArrayModel::ComplexVector
EarthFixedGnbBeamforming::CreateEarthFixedGnbWeights(const Ptr<NrSpectrumPhy>& gnbSpectrumPhy,
                                                     const Vector& anchorPosition)
{
    Ptr<const UniformPlanarArray> gnbAntenna =
        gnbSpectrumPhy->GetAntenna()->GetObject<UniformPlanarArray>();
    NS_ABORT_MSG_IF(!gnbAntenna, "gNB antenna should be UniformPlanarArray");

    const Vector gnbPosition = gnbSpectrumPhy->GetMobility()->GetPosition();
    const Vector delta(anchorPosition.x - gnbPosition.x,
                       anchorPosition.y - gnbPosition.y,
                       anchorPosition.z - gnbPosition.z);
    const double norm = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    NS_ABORT_MSG_IF(norm <= 0.0, "Earth-fixed beam anchor coincides with gNB position");

    const Angles anchorAngles(anchorPosition, gnbPosition);
    const double azimuthDeg = anchorAngles.GetAzimuth() * 180.0 / M_PI;
    const double zenithDeg = anchorAngles.GetInclination() * 180.0 / M_PI;
    return CreateDirectionalBfvAz(gnbAntenna, azimuthDeg, zenithDeg);
}

inline BeamformingVectorPair
EarthFixedGnbBeamforming::GetBeamformingVectors(const Ptr<NrSpectrumPhy>& gnbSpectrumPhy,
                                                const Ptr<NrSpectrumPhy>& ueSpectrumPhy) const
{
    NS_LOG_FUNCTION(this);

    Ptr<const UniformPlanarArray> ueAntenna =
        ueSpectrumPhy->GetAntenna()->GetObject<UniformPlanarArray>();
    NS_ABORT_MSG_IF(!ueAntenna, "UE antenna should be UniformPlanarArray");

    auto gnbMobility = GetVirtualMobilityModel(gnbSpectrumPhy->GetSpectrumChannel(),
                                               gnbSpectrumPhy->GetMobility(),
                                               ueSpectrumPhy->GetMobility());

    auto anchorPosition = ResolveAnchorPosition(gnbSpectrumPhy->GetDevice());
    if (!anchorPosition.has_value())
    {
        DirectPathBeamforming fallback;
        return fallback.GetBeamformingVectors(gnbSpectrumPhy, ueSpectrumPhy);
    }

    const auto gnbWeights = CreateEarthFixedGnbWeights(gnbSpectrumPhy, anchorPosition.value());
    const BeamformingVector gnbBfv =
        BeamformingVector(std::make_pair(gnbWeights, BeamId::GetEmptyBeamId()));

    const auto ueWeights =
        CreateDirectPathBfv(ueSpectrumPhy->GetMobility(), gnbMobility, ueAntenna);
    const BeamformingVector ueBfv =
        BeamformingVector(std::make_pair(ueWeights, BeamId::GetEmptyBeamId()));

    return BeamformingVectorPair(std::make_pair(gnbBfv, ueBfv));
}

} // namespace ns3

#endif // EARTH_FIXED_GNB_BEAMFORMING_H
