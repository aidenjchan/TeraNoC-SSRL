// Copyright 2026 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdint.h>

void leaf_sb_update(const float *x_in, float *x_out, uint32_t start,
                    uint32_t stop, const uint32_t *row_ptr,
                    const uint32_t *col_idx, float a, float b, float decay,
                    float c);

float compute_maxcut(const float *x, const uint32_t *row_ptr,
                     const uint32_t *col_idx, uint32_t n_nodes);
