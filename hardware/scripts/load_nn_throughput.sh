#!/bin/bash
# Copyright 2026
# SPDX-License-Identifier: Apache-2.0
#
# Nearest-neighbor traffic-generator load sweep (Verilator).
# Saves each run's transcript and summary before starting the next.

set -euo pipefail

MEMPOOL_DIR=$(git rev-parse --show-toplevel 2>/dev/null || echo "${MEMPOOL_DIR:-}")
cd "${MEMPOOL_DIR}/hardware"

CONFIG="${CONFIG:-terapool}"
TG_PATTERN="${TG_PATTERN:-1}"
TG_NN_DIR="${TG_NN_DIR:-0}"
TG_NN_WRAP="${TG_NN_WRAP:-1}"
TG_NCYCLES="${TG_NCYCLES:-10000}"

# Explicit load points (avoid GNU seq floats — seq can emit -0.02 at range ends).
# Override: REQ_PROBS="0.02 0.2 0.4 0.6" ./scripts/load_nn_throughput.sh
if [[ -n "${REQ_PROBS:-}" ]]; then
  # shellcheck disable=SC2206
  REQ_PROB_LIST=(${REQ_PROBS})
else
  REQ_PROB_LIST=(0.02 0.4 0.6)
fi

timestamp=$(date +%Y%m%d_%H%M%S)
OUT_DIR="load_nn_${timestamp}"
mkdir -p "${OUT_DIR}"

echo "Results directory: ${PWD}/${OUT_DIR}"
echo "config=${CONFIG} tg_pattern=${TG_PATTERN} tg_nn_dir=${TG_NN_DIR} tg_ncycles=${TG_NCYCLES}"
echo "req_probs: ${REQ_PROB_LIST[*]}"
echo ""

for req_prob in "${REQ_PROB_LIST[@]}"; do
  label=$(echo "${req_prob}" | tr '.' '_')
  run_dir="${OUT_DIR}/reqprob_${label}"
  mkdir -p "${run_dir}"

  echo "=== req_prob=${req_prob} ==="

  make clean
  tg=1 \
    config="${CONFIG}" \
    tg_pattern="${TG_PATTERN}" \
    tg_nn_dir="${TG_NN_DIR}" \
    tg_nn_wrap="${TG_NN_WRAP}" \
    tg_reqprob="${req_prob}" \
    tg_ncycles="${TG_NCYCLES}" \
    make verilate 2>&1 | tee "${run_dir}/make_verilate.log"

  if [[ -f build/transcript ]]; then
    cp build/transcript "${run_dir}/transcript"
  fi

  grep -E "Pattern:|Offered|Total cycles|Average|Max|Throughput" \
    "${run_dir}/transcript" > "${run_dir}/summary.txt" || true

  {
    echo "req_prob=${req_prob}"
    echo "config=${CONFIG}"
    echo "tg_pattern=${TG_PATTERN}"
    echo "tg_nn_dir=${TG_NN_DIR}"
    echo "tg_nn_wrap=${TG_NN_WRAP}"
    echo "tg_ncycles=${TG_NCYCLES}"
    echo "timestamp=$(date -Iseconds)"
  } > "${run_dir}/params.txt"

  cat "${run_dir}/summary.txt"
  echo ""
done

echo "Done. All runs saved under: ${PWD}/${OUT_DIR}"
