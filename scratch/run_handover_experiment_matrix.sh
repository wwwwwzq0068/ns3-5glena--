#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

SIM_BINARY="leo-ntn-handover-baseline"
DEFAULT_SIM_TIME="40"
DEFAULT_REPEAT="1"
DEFAULT_RNG_RUN_START="1"
DEFAULT_RESULT_BASE_DIR="scratch/results/exp/v4.1/joint"
RESULT_BASE_DIR="$DEFAULT_RESULT_BASE_DIR"

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
  --result-base-dir DIR     Override the result root directory.
  --build                   Run ./ns3 build before launching experiments.
  --enable-grid-svg         Keep grid SVG generation enabled. Default is OFF.
  --dry-run                 Print commands without executing them.
  --help                    Show this help.
  -- [EXTRA NS3 ARGS...]    Forward extra arguments to every experiment.

Groups:
  baseline-core             B00 B10 B11 B20 B21
  improved-weight           I00 I10 I11
  improved-load-gating      I20 I21
  improved-return-guard     I30 I31
  improved-load-pressure    I40
  joint-core                B00 I00 I10 I20 I30
  paper-shortlist           B00 B11 I00 I21 I31 I40
EOF
}

list_experiments() {
    cat <<'EOF'
Experiment Matrix (v4.1)
  B00  baseline default          handoverMode=baseline hoTttMs=200 hoHysteresisDb=2.0
  B10  baseline short TTT        handoverMode=baseline hoTttMs=160
  B11  baseline long TTT         handoverMode=baseline hoTttMs=320
  B20  baseline low hysteresis   handoverMode=baseline hoHysteresisDb=1.0
  B21  baseline high hysteresis  handoverMode=baseline hoHysteresisDb=3.0
  I00  improved default joint    handoverMode=improved improvedSignalWeight=0.7 improvedLoadWeight=0.3 improvedVisibilityWeight=0.2
  I10  improved visibility off   handoverMode=improved improvedVisibilityWeight=0.0 improvedMinVisibilitySeconds=0.0
  I11  improved visibility strong handoverMode=improved improvedSignalWeight=0.6 improvedLoadWeight=0.2 improvedVisibilityWeight=0.4 improvedMinVisibilitySeconds=1.5
  I20  improved load conservative handoverMode=improved improvedMinLoadScoreDelta=0.3 improvedMaxSignalGapDb=2.0
  I21  improved load aggressive  handoverMode=improved improvedMinLoadScoreDelta=0.1 improvedMaxSignalGapDb=5.0
  I30  improved guard off        handoverMode=improved improvedReturnGuardSeconds=0.0
  I31  improved guard strong     handoverMode=improved improvedReturnGuardSeconds=1.0
  I40  improved tighter capacity handoverMode=improved maxSupportedUesPerSatellite=2.5 loadCongestionThreshold=0.7

Groups
  baseline-core          B00 B10 B11 B20 B21
  improved-weight        I00 I10 I11
  improved-load-gating   I20 I21
  improved-return-guard  I30 I31
  improved-load-pressure I40
  joint-core             B00 I00 I10 I20 I30
  paper-shortlist        B00 B11 I00 I21 I31 I40
EOF
}

experiment_label() {
    case "$1" in
        B00) printf '%s\n' "baseline-default" ;;
        B10) printf '%s\n' "baseline-ttt160" ;;
        B11) printf '%s\n' "baseline-ttt320" ;;
        B20) printf '%s\n' "baseline-hys1" ;;
        B21) printf '%s\n' "baseline-hys3" ;;
        I00) printf '%s\n' "improved-joint-default" ;;
        I10) printf '%s\n' "improved-vis-off" ;;
        I11) printf '%s\n' "improved-vis-strong" ;;
        I20) printf '%s\n' "improved-load-conservative" ;;
        I21) printf '%s\n' "improved-load-aggressive" ;;
        I30) printf '%s\n' "improved-guard-off" ;;
        I31) printf '%s\n' "improved-guard-strong" ;;
        I40) printf '%s\n' "improved-tight-capacity" ;;
        *) return 1 ;;
    esac
}

experiment_args() {
    case "$1" in
        B00) printf '%s\n' "--handoverMode=baseline --hoTttMs=200 --hoHysteresisDb=2.0" ;;
        B10) printf '%s\n' "--handoverMode=baseline --hoTttMs=160" ;;
        B11) printf '%s\n' "--handoverMode=baseline --hoTttMs=320" ;;
        B20) printf '%s\n' "--handoverMode=baseline --hoHysteresisDb=1.0" ;;
        B21) printf '%s\n' "--handoverMode=baseline --hoHysteresisDb=3.0" ;;
        I00) printf '%s\n' "--handoverMode=improved --improvedSignalWeight=0.7 --improvedLoadWeight=0.3 --improvedVisibilityWeight=0.2" ;;
        I10) printf '%s\n' "--handoverMode=improved --improvedSignalWeight=0.7 --improvedLoadWeight=0.3 --improvedVisibilityWeight=0.0 --improvedMinVisibilitySeconds=0.0" ;;
        I11) printf '%s\n' "--handoverMode=improved --improvedSignalWeight=0.6 --improvedLoadWeight=0.2 --improvedVisibilityWeight=0.4 --improvedMinVisibilitySeconds=1.5" ;;
        I20) printf '%s\n' "--handoverMode=improved --improvedSignalWeight=0.7 --improvedLoadWeight=0.3 --improvedVisibilityWeight=0.2 --improvedMinLoadScoreDelta=0.3 --improvedMaxSignalGapDb=2.0" ;;
        I21) printf '%s\n' "--handoverMode=improved --improvedSignalWeight=0.7 --improvedLoadWeight=0.3 --improvedVisibilityWeight=0.2 --improvedMinLoadScoreDelta=0.1 --improvedMaxSignalGapDb=5.0" ;;
        I30) printf '%s\n' "--handoverMode=improved --improvedSignalWeight=0.7 --improvedLoadWeight=0.3 --improvedVisibilityWeight=0.2 --improvedReturnGuardSeconds=0.0" ;;
        I31) printf '%s\n' "--handoverMode=improved --improvedSignalWeight=0.7 --improvedLoadWeight=0.3 --improvedVisibilityWeight=0.2 --improvedReturnGuardSeconds=1.0" ;;
        I40) printf '%s\n' "--handoverMode=improved --improvedSignalWeight=0.7 --improvedLoadWeight=0.3 --improvedVisibilityWeight=0.2 --maxSupportedUesPerSatellite=2.5 --loadCongestionThreshold=0.7" ;;
        *) return 1 ;;
    esac
}

expand_group() {
    case "$1" in
        baseline-core) printf '%s\n' "B00 B10 B11 B20 B21" ;;
        improved-weight) printf '%s\n' "I00 I10 I11" ;;
        improved-load-gating) printf '%s\n' "I20 I21" ;;
        improved-return-guard) printf '%s\n' "I30 I31" ;;
        improved-load-pressure) printf '%s\n' "I40" ;;
        joint-core) printf '%s\n' "B00 I00 I10 I20 I30" ;;
        paper-shortlist) printf '%s\n' "B00 B11 I00 I21 I31 I40" ;;
        *) return 1 ;;
    esac
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
        --result-base-dir)
            [ $# -ge 2 ] || { echo "missing value for --result-base-dir" >&2; exit 1; }
            RESULT_BASE_DIR="$2"
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
    SELECTED_IDS="$SELECTED_IDS B00 B10 B11 B20 B21 I00 I10 I11 I20 I21 I30 I31 I40"
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

if [ "$DRY_RUN" = "0" ]; then
    mkdir -p "$RESULT_BASE_DIR"
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
            mkdir -p "$run_dir"
            {
                printf 'experimentId=%s\n' "$experiment_id"
                printf 'label=%s\n' "$label"
                printf 'rngRun=%s\n' "$rng_run"
                printf 'simTime=%s\n' "$SIM_TIME"
                printf 'outputDir=%s\n' "$run_dir"
                printf 'resultBaseDir=%s\n' "$RESULT_BASE_DIR"
                printf 'programArgs=%s\n' "$full_program_args"
                if [ -n "$EXTRA_NS3_ARGS" ]; then
                    printf 'extraArgs=%s\n' "$EXTRA_NS3_ARGS"
                fi
                printf 'gitCommit=%s\n' "$(git rev-parse HEAD 2>/dev/null || printf 'unknown')"
                printf 'timestampUtc=%s\n' "$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
            } > "${run_dir}/run-meta.txt"
            ./ns3 run --no-build "${full_program_args}"
        fi

        run_index=$((run_index + 1))
    done
done
