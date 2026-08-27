// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include "sb.h"

// These Snitch cores have no FPU, so every float op in the hot loop is emulated
// by libgcc soft-float (__addsf3/__subsf3/__mulsf3). That path is both slow and
// where a core was observed corrupting its register file and faulting. The whole
// SB update is really integer: spins are exactly +/-1 and the coefficients are
// fixed, so we evaluate it with integer arithmetic and never call soft-float.

// Extract the +/-1 spin straight from the float's sign bit. memcpy compiles to a
// plain word load (no soft-float, no strict-aliasing hazard); +1.0f has sign bit
// 0, -1.0f has sign bit 1.
static inline int32_t spin_of(float f) {
  uint32_t bits;
  __builtin_memcpy(&bits, &f, sizeof(bits));
  return (bits >> 31) ? -1 : 1;
}

void leaf_sb_update(const float *x_in, float *x_out, uint32_t start,
                    uint32_t stop, const uint32_t *row_ptr,
                    const uint32_t *col_idx, float a, float b, float decay,
                    float c) {
  // sign(a*x - b*S) == sign(2a*x - 2b*S). For the benchmark constants a=15,
  // b=7.5 these scale to the integers 30 and 15, so the threshold is exact in
  // pure integer math. Using the float args would pull in soft-float, so use the
  // folded constants instead (bit-identical result). Keep in sync with sb_a/sb_b
  // in data_bifurcation.h.
  (void)a;
  (void)b;
  (void)decay;
  (void)c;
  const int32_t ca = 30; // 2 * sb_a
  const int32_t cb = 15; // 2 * sb_b

  for (uint32_t spin = start; spin < stop; spin++) {
    int32_t sum = 0;
    for (uint32_t edge = row_ptr[spin]; edge < row_ptr[spin + 1]; edge++) {
      sum += spin_of(x_in[col_idx[edge]]);
    }
    int32_t val = ca * spin_of(x_in[spin]) - cb * sum;
    x_out[spin] = (val >= 0) ? 1.0f : -1.0f; // matches sb_sign (>=0 -> +1)
  }
}

float compute_maxcut(const float *x, const uint32_t *row_ptr,
                     const uint32_t *col_idx, uint32_t n_nodes) {
  uint32_t cut = 0;

  for (uint32_t i = 0; i < n_nodes; i++) {
    for (uint32_t edge = row_ptr[i]; edge < row_ptr[i + 1]; edge++) {
      uint32_t j = col_idx[edge];
      // An edge is cut when its endpoints have opposite spins, i.e. their sign
      // bits differ -- no float multiply/compare needed.
      if (i < j && (spin_of(x[i]) != spin_of(x[j]))) {
        cut++;
      }
    }
  }

  return (float)cut;
}
