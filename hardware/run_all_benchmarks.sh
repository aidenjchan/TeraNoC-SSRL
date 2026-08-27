#!/usr/bin/env bash
# Run all terapool paper benchmarks sequentially.
# Usage:
#   ./run_all_benchmarks.sh              # build binaries + run all sims
#   ./run_all_benchmarks.sh --sim-only   # skip build, run sims only
#   ./run_all_benchmarks.sh --build-only # build binaries only, no sims
#
# Each simulation uses `make benchmark config=terapool` which:
#   1. Logs environment + binary to a timestamped results dir
#   2. Runs the VCS simulation (simcvcs)
#   3. Generates per-core traces and a results.csv
#
# Results land in hardware/results/<timestamp>_<app>_<githash>/

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
HARDWARE_DIR="$SCRIPT_DIR"
SOFTWARE_DIR="$REPO_ROOT/software"
BUILD_PLAN="$SOFTWARE_DIR/apps/baremetal/build_plan.json"
BIN_DIR="$SOFTWARE_DIR/bin/apps/baremetal/terapool"

CONFIG="terapool"

APPS=(
    "axpy_f32_262144"
    "axpy_f16_524288"
    "batchnorm_f16_384x2048"
    "cfft_radix4_f16_4096"
    "chest_f16_8x64x1024"
    "cholesky_f16_16x1024"
    "dotp_f32_262144"
    "dotp_f16_524288"
    "gemv_f32_4096x192"
    "gemv_f16_8192x192"
    "layernorm_f16_2048x384"
    "matmul_f32_512x256x512"
    "matmul_f16_512x512x512"
    "mimo_mmse_f32_8x16x1024"
    "mimo_mmse_f16_8x32x1024"
    "softmax_f16_2048x384"
)

DO_BUILD=true
DO_SIM=true

for arg in "$@"; do
    case "$arg" in
        --sim-only)  DO_BUILD=false ;;
        --build-only) DO_SIM=false ;;
        --help|-h)
            echo "Usage: $0 [--sim-only] [--build-only]"
            exit 0
            ;;
    esac
done

log() { echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*"; }

# ── Phase 1: Build all terapool binaries ──
if $DO_BUILD; then
    log "===== PHASE 1: Building all terapool binaries ====="
    cd "$SOFTWARE_DIR/apps/baremetal"
    python3 auto_build.py --plan "$BUILD_PLAN"
    log "===== Build complete ====="
fi

# ── Phase 2: Run simulations ──
if $DO_SIM; then
    log "===== PHASE 2: Running all simulations (config=$CONFIG) ====="

    total=${#APPS[@]}
    passed=0
    failed=0
    skipped=0
    declare -a FAILED_APPS=()

    for i in "${!APPS[@]}"; do
        app="${APPS[$i]}"
        n=$((i + 1))
        log "────────────────────────────────────────────────"
        log "[$n/$total] Starting: $app"

        binary="$BIN_DIR/$app"
        if [[ ! -f "$binary" ]]; then
            log "WARNING: Binary not found: $binary — skipping"
            skipped=$((skipped + 1))
            continue
        fi

        cd "$HARDWARE_DIR"
        if app="$app" make benchmark config="$CONFIG"; then
            log "[$n/$total] PASSED: $app"
            passed=$((passed + 1))
        else
            log "[$n/$total] FAILED: $app (exit code $?)"
            failed=$((failed + 1))
            FAILED_APPS+=("$app")
        fi
    done

    log "════════════════════════════════════════════════════"
    log "SUMMARY: $total total | $passed passed | $failed failed | $skipped skipped"
    if [[ ${#FAILED_APPS[@]} -gt 0 ]]; then
        log "Failed apps:"
        for fa in "${FAILED_APPS[@]}"; do
            log "  - $fa"
        done
    fi
    log "Results are in: $HARDWARE_DIR/results/"
    log "════════════════════════════════════════════════════"
fi
