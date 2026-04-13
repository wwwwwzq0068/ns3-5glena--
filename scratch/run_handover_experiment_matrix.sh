#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

SIM_BINARY="leo-ntn-handover-baseline"
DEFAULT_SIM_TIME="40"
DEFAULT_REPEAT="1"
DEFAULT_RNG_RUN_START="1"
RESULT_BASE_DIR="scratch/results/exp/v4.2"

print_usage() {
    cat <<'EOF'
Usage:
  scratch/run_handover_experiment_matrix.sh --list
  scratch/run_handover_experiment_matrix.sh --run ID [options]
  scratch/run_handover_experiment_matrix.sh --group NAME [options]
  scratch/run_handover_experiment_matrix.sh --all [options]

Options:
  --list                    List the experiment matrix and groups.
  --run ID                  Run one experiment ID, such as B00 or I02.
  --group NAME              Run a predefined group.
  --all                     Run the full matrix.
  --repeat N                Repeat each experiment N times. Default: 1.
  --rng-run-start N         First RngRun value. Default: 1.
  --sim-time SECONDS        Override simTime. Default: 40.
  --build                   Run ./ns3 build before launching experiments.
  --enable-grid-svg         Keep grid SVG generation enabled. Default is OFF.
  --dry-run                 Print commands without executing them.
  --help                    Show this help.
  -- [EXTRA NS3 ARGS...]    Forward extra arguments to every experiment.

Groups:
  baseline-repeat           B00
  ttt-scan                  B10 B11 B12
  hysteresis-scan           B20 B21
  improved-weight           I00 I01 I02
  improved-stablelead       I10 I11 I12 I13 I14
  improved-margin           I20 I21 I22
  improved-opt-grid         B00 I30 I31 I32 I33 I34 I35 I36 I37 I38
  paper-shortlist           B00 I00 I02 I31
  carrier-reuse-diagnosis   R10 R20 R40 (PHY/carrier reuse diagnosis, NOT formal baseline)
  carrier-reuse-phase2      R21 R22 R41 R42 (inter-frequency HO support)
EOF
}

list_experiments() {
    cat <<'EOF'
Experiment Matrix (v4.2)
  B00  fixed baseline control    handoverMode=baseline hoTttMs=160 hoHysteresisDb=2.0
  B10  baseline short TTT        handoverMode=baseline hoTttMs=160
  B11  baseline medium TTT       handoverMode=baseline hoTttMs=320
  B12  baseline long TTT         handoverMode=baseline hoTttMs=480
  B20  baseline low hysteresis   handoverMode=baseline hoHysteresisDb=1.0
  B21  baseline high hysteresis  handoverMode=baseline hoHysteresisDb=3.0
  I00  improved default weight   handoverMode=improved improvedSignalWeight=0.7 improvedLoadWeight=0.3
  I01  improved signal-heavy     handoverMode=improved improvedSignalWeight=0.8 improvedLoadWeight=0.2
  I02  improved balanced weight  handoverMode=improved improvedSignalWeight=0.5 improvedLoadWeight=0.5
  I10  improved stableLead 80ms  handoverMode=improved hoTttMs=160 hoHysteresisDb=2.0 improvedMinStableLeadTimeSeconds=0.08
  I11  improved stableLead 120ms handoverMode=improved hoTttMs=160 hoHysteresisDb=2.0 improvedMinStableLeadTimeSeconds=0.12
  I12  improved stableLead 160ms handoverMode=improved hoTttMs=160 hoHysteresisDb=2.0 improvedMinStableLeadTimeSeconds=0.16
  I13  improved stableLead 200ms handoverMode=improved hoTttMs=160 hoHysteresisDb=2.0 improvedMinStableLeadTimeSeconds=0.20
  I14  improved stableLead 240ms handoverMode=improved hoTttMs=160 hoHysteresisDb=2.0 improvedMinStableLeadTimeSeconds=0.24
  I20  improved margin 0.02      handoverMode=improved hoTttMs=160 hoHysteresisDb=2.0 improvedMinJointScoreMargin=0.02
  I21  improved margin 0.03      handoverMode=improved hoTttMs=160 hoHysteresisDb=2.0 improvedMinJointScoreMargin=0.03
  I22  improved margin 0.05      handoverMode=improved hoTttMs=160 hoHysteresisDb=2.0 improvedMinJointScoreMargin=0.05
  I30  improved 120ms m02        handoverMode=improved hoTttMs=160 hoHysteresisDb=2.0 stableLead=0.12 margin=0.02
  I31  improved 120ms m03        handoverMode=improved hoTttMs=160 hoHysteresisDb=2.0 stableLead=0.12 margin=0.03
  I32  improved 120ms m05        handoverMode=improved hoTttMs=160 hoHysteresisDb=2.0 stableLead=0.12 margin=0.05
  I33  improved 160ms m02        handoverMode=improved hoTttMs=160 hoHysteresisDb=2.0 stableLead=0.16 margin=0.02
  I34  improved 160ms m03        handoverMode=improved hoTttMs=160 hoHysteresisDb=2.0 stableLead=0.16 margin=0.03
  I35  improved 160ms m05        handoverMode=improved hoTttMs=160 hoHysteresisDb=2.0 stableLead=0.16 margin=0.05
  I36  improved 240ms m02        handoverMode=improved hoTttMs=160 hoHysteresisDb=2.0 stableLead=0.24 margin=0.02
  I37  improved 240ms m03        handoverMode=improved hoTttMs=160 hoHysteresisDb=2.0 stableLead=0.24 margin=0.03
  I38  improved 240ms m05        handoverMode=improved hoTttMs=160 hoHysteresisDb=2.0 stableLead=0.24 margin=0.05

  R10  carrier reuse1 (baseline) carrierReuseMode=reuse1 (PHY diagnosis)
  R20  carrier reuse2-plane      carrierReuseMode=reuse2-plane carrierFrequencySpacingHz=60e6 (PHY diagnosis)
  R40  carrier reuse4            carrierReuseMode=reuse4 carrierFrequencySpacingHz=60e6 (PHY diagnosis)

  R21  reuse2-plane inter-freq OFF  carrierReuseMode=reuse2-plane interFrequencyHandoverEnabled=false (Phase2)
  R22  reuse2-plane inter-freq ON   carrierReuseMode=reuse2-plane interFrequencyHandoverEnabled=true (Phase2)
  R41  reuse4 inter-freq OFF        carrierReuseMode=reuse4 interFrequencyHandoverEnabled=false (Phase2)
  R42  reuse4 inter-freq ON         carrierReuseMode=reuse4 interFrequencyHandoverEnabled=true (Phase2)

Groups
  baseline-repeat  B00
  ttt-scan         B10 B11 B12
  hysteresis-scan  B20 B21
  improved-weight  I00 I01 I02
  improved-stablelead I10 I11 I12 I13 I14
  improved-margin  I20 I21 I22
  improved-opt-grid B00 I30 I31 I32 I33 I34 I35 I36 I37 I38
  paper-shortlist  B00 I00 I02 I31
  carrier-reuse-diagnosis R10 R20 R40
  carrier-reuse-phase2 R21 R22 R41 R42
EOF
}

experiment_label() {
    case "$1" in
        B00) printf '%s\n' "baseline-b00-control" ;;
        B10) printf '%s\n' "baseline-ttt160" ;;
        B11) printf '%s\n' "baseline-ttt320" ;;
        B12) printf '%s\n' "baseline-ttt480" ;;
        B20) printf '%s\n' "baseline-hys1" ;;
        B21) printf '%s\n' "baseline-hys3" ;;
        I00) printf '%s\n' "improved-w73" ;;
        I01) printf '%s\n' "improved-w82" ;;
        I02) printf '%s\n' "improved-w55" ;;
        I10) printf '%s\n' "improved-stablelead80" ;;
        I11) printf '%s\n' "improved-stablelead120" ;;
        I12) printf '%s\n' "improved-stablelead160" ;;
        I13) printf '%s\n' "improved-stablelead200" ;;
        I14) printf '%s\n' "improved-stablelead240" ;;
        I20) printf '%s\n' "improved-margin02" ;;
        I21) printf '%s\n' "improved-margin03" ;;
        I22) printf '%s\n' "improved-margin05" ;;
        I30) printf '%s\n' "improved-s120-m02" ;;
        I31) printf '%s\n' "improved-s120-m03" ;;
        I32) printf '%s\n' "improved-s120-m05" ;;
        I33) printf '%s\n' "improved-s160-m02" ;;
        I34) printf '%s\n' "improved-s160-m03" ;;
        I35) printf '%s\n' "improved-s160-m05" ;;
        I36) printf '%s\n' "improved-s240-m02" ;;
        I37) printf '%s\n' "improved-s240-m03" ;;
        I38) printf '%s\n' "improved-s240-m05" ;;
        R10) printf '%s\n' "carrier-reuse1" ;;
        R20) printf '%s\n' "carrier-reuse2-plane" ;;
        R40) printf '%s\n' "carrier-reuse4" ;;
        R21) printf '%s\n' "reuse2-plane-inter-freq-off" ;;
        R22) printf '%s\n' "reuse2-plane-inter-freq-on" ;;
        R41) printf '%s\n' "reuse4-inter-freq-off" ;;
        R42) printf '%s\n' "reuse4-inter-freq-on" ;;
        *) return 1 ;;
    esac
}

experiment_args() {
    case "$1" in
        B00) printf '%s\n' "--handoverMode=baseline --hoTttMs=160 --hoHysteresisDb=2.0" ;;
        B10) printf '%s\n' "--handoverMode=baseline --hoTttMs=160" ;;
        B11) printf '%s\n' "--handoverMode=baseline --hoTttMs=320" ;;
        B12) printf '%s\n' "--handoverMode=baseline --hoTttMs=480" ;;
        B20) printf '%s\n' "--handoverMode=baseline --hoHysteresisDb=1.0" ;;
        B21) printf '%s\n' "--handoverMode=baseline --hoHysteresisDb=3.0" ;;
        I00) printf '%s\n' "--handoverMode=improved --improvedSignalWeight=0.7 --improvedLoadWeight=0.3" ;;
        I01) printf '%s\n' "--handoverMode=improved --improvedSignalWeight=0.8 --improvedLoadWeight=0.2" ;;
        I02) printf '%s\n' "--handoverMode=improved --improvedSignalWeight=0.5 --improvedLoadWeight=0.5" ;;
        I10) printf '%s\n' "--handoverMode=improved --hoTttMs=160 --hoHysteresisDb=2.0 --improvedSignalWeight=0.7 --improvedLoadWeight=0.3 --improvedMinStableLeadTimeSeconds=0.08" ;;
        I11) printf '%s\n' "--handoverMode=improved --hoTttMs=160 --hoHysteresisDb=2.0 --improvedSignalWeight=0.7 --improvedLoadWeight=0.3 --improvedMinStableLeadTimeSeconds=0.12" ;;
        I12) printf '%s\n' "--handoverMode=improved --hoTttMs=160 --hoHysteresisDb=2.0 --improvedSignalWeight=0.7 --improvedLoadWeight=0.3 --improvedMinStableLeadTimeSeconds=0.16" ;;
        I13) printf '%s\n' "--handoverMode=improved --hoTttMs=160 --hoHysteresisDb=2.0 --improvedSignalWeight=0.7 --improvedLoadWeight=0.3 --improvedMinStableLeadTimeSeconds=0.20" ;;
        I14) printf '%s\n' "--handoverMode=improved --hoTttMs=160 --hoHysteresisDb=2.0 --improvedSignalWeight=0.7 --improvedLoadWeight=0.3 --improvedMinStableLeadTimeSeconds=0.24" ;;
        I20) printf '%s\n' "--handoverMode=improved --hoTttMs=160 --hoHysteresisDb=2.0 --improvedSignalWeight=0.7 --improvedLoadWeight=0.3 --improvedMinJointScoreMargin=0.02" ;;
        I21) printf '%s\n' "--handoverMode=improved --hoTttMs=160 --hoHysteresisDb=2.0 --improvedSignalWeight=0.7 --improvedLoadWeight=0.3 --improvedMinJointScoreMargin=0.03" ;;
        I22) printf '%s\n' "--handoverMode=improved --hoTttMs=160 --hoHysteresisDb=2.0 --improvedSignalWeight=0.7 --improvedLoadWeight=0.3 --improvedMinJointScoreMargin=0.05" ;;
        I30) printf '%s\n' "--handoverMode=improved --hoTttMs=160 --hoHysteresisDb=2.0 --improvedSignalWeight=0.7 --improvedLoadWeight=0.3 --improvedMinStableLeadTimeSeconds=0.12 --improvedMinJointScoreMargin=0.02" ;;
        I31) printf '%s\n' "--handoverMode=improved --hoTttMs=160 --hoHysteresisDb=2.0 --improvedSignalWeight=0.7 --improvedLoadWeight=0.3 --improvedMinStableLeadTimeSeconds=0.12 --improvedMinJointScoreMargin=0.03" ;;
        I32) printf '%s\n' "--handoverMode=improved --hoTttMs=160 --hoHysteresisDb=2.0 --improvedSignalWeight=0.7 --improvedLoadWeight=0.3 --improvedMinStableLeadTimeSeconds=0.12 --improvedMinJointScoreMargin=0.05" ;;
        I33) printf '%s\n' "--handoverMode=improved --hoTttMs=160 --hoHysteresisDb=2.0 --improvedSignalWeight=0.7 --improvedLoadWeight=0.3 --improvedMinStableLeadTimeSeconds=0.16 --improvedMinJointScoreMargin=0.02" ;;
        I34) printf '%s\n' "--handoverMode=improved --hoTttMs=160 --hoHysteresisDb=2.0 --improvedSignalWeight=0.7 --improvedLoadWeight=0.3 --improvedMinStableLeadTimeSeconds=0.16 --improvedMinJointScoreMargin=0.03" ;;
        I35) printf '%s\n' "--handoverMode=improved --hoTttMs=160 --hoHysteresisDb=2.0 --improvedSignalWeight=0.7 --improvedLoadWeight=0.3 --improvedMinStableLeadTimeSeconds=0.16 --improvedMinJointScoreMargin=0.05" ;;
        I36) printf '%s\n' "--handoverMode=improved --hoTttMs=160 --hoHysteresisDb=2.0 --improvedSignalWeight=0.7 --improvedLoadWeight=0.3 --improvedMinStableLeadTimeSeconds=0.24 --improvedMinJointScoreMargin=0.02" ;;
        I37) printf '%s\n' "--handoverMode=improved --hoTttMs=160 --hoHysteresisDb=2.0 --improvedSignalWeight=0.7 --improvedLoadWeight=0.3 --improvedMinStableLeadTimeSeconds=0.24 --improvedMinJointScoreMargin=0.03" ;;
        I38) printf '%s\n' "--handoverMode=improved --hoTttMs=160 --hoHysteresisDb=2.0 --improvedSignalWeight=0.7 --improvedLoadWeight=0.3 --improvedMinStableLeadTimeSeconds=0.24 --improvedMinJointScoreMargin=0.05" ;;
        R10) printf '%s\n' "--handoverMode=baseline --hoTttMs=160 --hoHysteresisDb=2.0 --carrierReuseMode=reuse1 --gnbAntennaElement=three-gpp --ueAntennaElement=three-gpp" ;;
        R20) printf '%s\n' "--handoverMode=baseline --hoTttMs=160 --hoHysteresisDb=2.0 --carrierReuseMode=reuse2-plane --carrierFrequencySpacingHz=60e6 --gnbAntennaElement=three-gpp --ueAntennaElement=three-gpp" ;;
        R40) printf '%s\n' "--handoverMode=baseline --hoTttMs=160 --hoHysteresisDb=2.0 --carrierReuseMode=reuse4 --carrierFrequencySpacingHz=60e6 --gnbAntennaElement=three-gpp --ueAntennaElement=three-gpp" ;;
        R21) printf '%s\n' "--handoverMode=baseline --hoTttMs=160 --hoHysteresisDb=2.0 --carrierReuseMode=reuse2-plane --carrierFrequencySpacingHz=60e6 --gnbAntennaElement=three-gpp --ueAntennaElement=three-gpp --interFrequencyHandoverEnabled=false" ;;
        R22) printf '%s\n' "--handoverMode=baseline --hoTttMs=160 --hoHysteresisDb=2.0 --carrierReuseMode=reuse2-plane --carrierFrequencySpacingHz=60e6 --gnbAntennaElement=three-gpp --ueAntennaElement=three-gpp --interFrequencyHandoverEnabled=true" ;;
        R41) printf '%s\n' "--handoverMode=baseline --hoTttMs=160 --hoHysteresisDb=2.0 --carrierReuseMode=reuse4 --carrierFrequencySpacingHz=60e6 --gnbAntennaElement=three-gpp --ueAntennaElement=three-gpp --interFrequencyHandoverEnabled=false" ;;
        R42) printf '%s\n' "--handoverMode=baseline --hoTttMs=160 --hoHysteresisDb=2.0 --carrierReuseMode=reuse4 --carrierFrequencySpacingHz=60e6 --gnbAntennaElement=three-gpp --ueAntennaElement=three-gpp --interFrequencyHandoverEnabled=true" ;;
        *) return 1 ;;
    esac
}

expand_group() {
    case "$1" in
        baseline-repeat) printf '%s\n' "B00" ;;
        ttt-scan) printf '%s\n' "B10 B11 B12" ;;
        hysteresis-scan) printf '%s\n' "B20 B21" ;;
        improved-weight) printf '%s\n' "I00 I01 I02" ;;
        improved-stablelead) printf '%s\n' "I10 I11 I12 I13 I14" ;;
        improved-margin) printf '%s\n' "I20 I21 I22" ;;
        improved-opt-grid) printf '%s\n' "B00 I30 I31 I32 I33 I34 I35 I36 I37 I38" ;;
        paper-shortlist) printf '%s\n' "B00 I00 I02 I31" ;;
        carrier-reuse-diagnosis) printf '%s\n' "R10 R20 R40" ;;
        carrier-reuse-phase2) printf '%s\n' "R21 R22 R41 R42" ;;
        *) return 1 ;;
    esac
}

run_command() {
    printf '%s\n' "$*"
    if [ "$DRY_RUN" = "0" ]; then
        eval "$@"
    fi
}

LIST_ONLY=0
RUN_ALL=0
BUILD_FIRST=0
ENABLE_GRID_SVG=0
DRY_RUN=0
REPEAT_COUNT="$DEFAULT_REPEAT"
RNG_RUN_START="$DEFAULT_RNG_RUN_START"
SIM_TIME="$DEFAULT_SIM_TIME"
SELECTED_IDS=""
EXTRA_NS3_ARGS=""

while [ $# -gt 0 ]; do
    case "$1" in
        --list)
            LIST_ONLY=1
            shift
            ;;
        --run)
            [ $# -ge 2 ] || { echo "missing value for --run" >&2; exit 1; }
            SELECTED_IDS="$SELECTED_IDS $2"
            shift 2
            ;;
        --group)
            [ $# -ge 2 ] || { echo "missing value for --group" >&2; exit 1; }
            GROUP_IDS=$(expand_group "$2") || { echo "unknown group: $2" >&2; exit 1; }
            SELECTED_IDS="$SELECTED_IDS $GROUP_IDS"
            shift 2
            ;;
        --all)
            RUN_ALL=1
            shift
            ;;
        --repeat)
            [ $# -ge 2 ] || { echo "missing value for --repeat" >&2; exit 1; }
            REPEAT_COUNT="$2"
            shift 2
            ;;
        --rng-run-start)
            [ $# -ge 2 ] || { echo "missing value for --rng-run-start" >&2; exit 1; }
            RNG_RUN_START="$2"
            shift 2
            ;;
        --sim-time)
            [ $# -ge 2 ] || { echo "missing value for --sim-time" >&2; exit 1; }
            SIM_TIME="$2"
            shift 2
            ;;
        --build)
            BUILD_FIRST=1
            shift
            ;;
        --enable-grid-svg)
            ENABLE_GRID_SVG=1
            shift
            ;;
        --dry-run)
            DRY_RUN=1
            shift
            ;;
        --help|-h)
            print_usage
            exit 0
            ;;
        --)
            shift
            if [ $# -gt 0 ]; then
                EXTRA_NS3_ARGS="$*"
            fi
            break
            ;;
        *)
            echo "unknown argument: $1" >&2
            print_usage >&2
            exit 1
            ;;
    esac
done

if [ "$LIST_ONLY" = "1" ]; then
    list_experiments
    exit 0
fi

if [ "$RUN_ALL" = "1" ]; then
    SELECTED_IDS="$SELECTED_IDS B00 B10 B11 B12 B20 B21 I00 I01 I02 I10 I11 I12 I13 I14 I20 I21 I22"
fi

SELECTED_IDS=$(printf '%s\n' "$SELECTED_IDS" | xargs)

if [ -z "$SELECTED_IDS" ]; then
    echo "no experiments selected" >&2
    print_usage >&2
    exit 1
fi

case "$REPEAT_COUNT" in
    ''|*[!0-9]*)
        echo "--repeat must be a positive integer" >&2
        exit 1
        ;;
esac

case "$RNG_RUN_START" in
    ''|*[!0-9]*)
        echo "--rng-run-start must be a positive integer" >&2
        exit 1
        ;;
esac

cd "$REPO_ROOT"

if [ "$BUILD_FIRST" = "1" ]; then
    if [ "$DRY_RUN" = "1" ]; then
        printf '%s\n' "./ns3 build"
    else
        ./ns3 build
    fi
fi

for experiment_id in $SELECTED_IDS; do
    label=$(experiment_label "$experiment_id") || { echo "unknown experiment id: $experiment_id" >&2; exit 1; }
    args=$(experiment_args "$experiment_id") || { echo "unknown experiment id: $experiment_id" >&2; exit 1; }

    run_index=0
    while [ "$run_index" -lt "$REPEAT_COUNT" ]; do
        rng_run=$((RNG_RUN_START + run_index))
        run_dir="${RESULT_BASE_DIR}/${experiment_id}-${label}/rng-${rng_run}"
        ns3_args="--simTime=${SIM_TIME} --outputDir=${run_dir} --RngRun=${rng_run}"

        if [ "$ENABLE_GRID_SVG" = "0" ]; then
            ns3_args="${ns3_args} --runGridSvgScript=0"
        fi

        if [ -n "$EXTRA_NS3_ARGS" ]; then
            ns3_args="${ns3_args} ${EXTRA_NS3_ARGS}"
        fi

        full_program_args="${SIM_BINARY} ${args} ${ns3_args}"
        printf '[Experiment] id=%s label=%s rngRun=%s outputDir=%s\n' \
            "$experiment_id" "$label" "$rng_run" "$run_dir"

        if [ "$DRY_RUN" = "1" ]; then
            printf '%s\n' "./ns3 run --no-build \"${full_program_args}\""
        else
            ./ns3 run --no-build "${full_program_args}"
        fi

        run_index=$((run_index + 1))
    done
done
