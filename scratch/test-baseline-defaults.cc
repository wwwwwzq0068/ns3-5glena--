#include "handover/beam-link-budget.h"
#include "handover/leo-ntn-handover-config.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace ns3;

namespace
{

void
Require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "[TEST-FAIL] " << message << std::endl;
        std::exit(1);
    }
}

void
RequireNear(double actual, double expected, double tolerance, const char* message)
{
    if (std::abs(actual - expected) > tolerance)
    {
        std::cerr << "[TEST-FAIL] " << message << " actual=" << actual
                  << " expected=" << expected << std::endl;
        std::exit(1);
    }
}

} // namespace

int
main()
{
    const BaselineSimulationConfig config;

    Require(config.ueLayoutType == "seven-cell", "baseline default should keep seven-cell layout");
    Require(config.ueNum == 25, "baseline default should keep 25 UE");
    RequireNear(config.poissonLambda, 1.5, 1e-12, "poisson layout default lambda should be 1.5");
    Require(config.maxUePerCell == 5, "poisson layout default should cap each cell at 5 UE");
    Require(config.ueLayoutRandomSeed == 42, "poisson layout default seed should be deterministic");
    Require(config.handoverMode == "baseline", "baseline default should keep A3 baseline mode");
    Require(ResolveEffectiveBeamExclusionMode(config) == "ring",
            "baseline default should keep one-ring beam anchor exclusion");
    Require(ResolveEffectiveRealLinkGateMode(config) == "beam-and-anchor",
            "baseline default should keep beam-plus-anchor real-link gating");
    Require(!config.enableSatAnchorTrace,
            "baseline default should keep satellite anchor trace disabled");
    Require(!config.enableSatGroundTrackTrace,
            "baseline default should keep satellite ground-track trace disabled");
    Require(!config.enableFlowMonitor,
            "baseline default should keep FlowMonitor disabled");
    Require(!config.enablePhyDlTbStats,
            "baseline default should keep PHY DL TB stats disabled");
    Require(!config.printMeasurementDecisionDiagnostics,
            "baseline default should keep measurement decision diagnostics disabled");

    Require(config.earthFixedBeamTargetMode == "grid-anchor",
            "baseline default should enable grid-anchor beam targeting");
    Require(config.beamExclusionCandidateK == 64,
            "baseline default should use K=64 beam exclusion search");
    Require(config.gnbAntennaRows == 12 && config.gnbAntennaColumns == 12,
            "baseline default should use a 12x12 gNB array");

    const BeamModelConfig defaultBeamConfig = DeriveBeamModelConfig(config);
    const double defaultArrayGainDb =
        10.0 * std::log10(config.gnbAntennaRows * config.gnbAntennaColumns);
    RequireNear(defaultBeamConfig.gMax0Dbi,
                config.b00MaxGainDb + defaultArrayGainDb,
                1e-9,
                "geometric peak gain should derive from b00 gain plus gNB array gain");
    RequireNear(defaultBeamConfig.theta3dBRad,
                LeoOrbitCalculator::DegToRad(config.b00BeamwidthDeg),
                1e-12,
                "geometric beamwidth should derive from b00 beamwidth");
    RequireNear(defaultBeamConfig.slaVDb,
                config.b00MaxAttenuationDb,
                1e-12,
                "geometric sidelobe attenuation should derive from b00 attenuation");

    BaselineSimulationConfig customConfig = config;
    customConfig.gnbAntennaRows = 4;
    customConfig.gnbAntennaColumns = 8;
    customConfig.b00MaxGainDb = 17.5;
    customConfig.b00BeamwidthDeg = 6.0;
    customConfig.b00MaxAttenuationDb = 24.0;
    const BeamModelConfig customBeamConfig = DeriveBeamModelConfig(customConfig);
    const double customArrayGainDb =
        10.0 * std::log10(customConfig.gnbAntennaRows * customConfig.gnbAntennaColumns);
    RequireNear(customBeamConfig.gMax0Dbi,
                customConfig.b00MaxGainDb + customArrayGainDb,
                1e-9,
                "geometric peak gain should track changed PHY antenna dimensions");
    RequireNear(customBeamConfig.theta3dBRad,
                LeoOrbitCalculator::DegToRad(customConfig.b00BeamwidthDeg),
                1e-12,
                "geometric beamwidth should track changed b00 beamwidth");
    RequireNear(customBeamConfig.slaVDb,
                customConfig.b00MaxAttenuationDb,
                1e-12,
                "geometric attenuation should track changed b00 attenuation");

    std::cout << "[TEST-PASS] baseline defaults are aligned with the current best reuse1 profile"
              << std::endl;
    return 0;
}
