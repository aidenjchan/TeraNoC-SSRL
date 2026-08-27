// Simulated Bifurcation max-cut testbench for TeraNoC.
// Reference: matlab_scripts/bifurcation/tb_MSIM.m
//
// Build:
//   make bifurcation
// Verilator:
//   cd hardware && make verilate_bifurcation
// VCS:
//   cd hardware && make simcvcs_bifurcation

#include <stdint.h>
#include <string.h>

#include "data_bifurcation.h"
#include "encoding.h"
#include "printf.h"
#include "runtime.h"
#include "sb.h"
#include "synchronization.h"

#ifndef SB_NUM_ITER
#define SB_NUM_ITER num_iter
#endif

float x_curr[num_nodes]
    __attribute__((aligned(4 * NUM_BANKS), section(".l2")));
float x_next[num_nodes]
    __attribute__((aligned(4 * NUM_BANKS), section(".l2")));

// Spin-wait (sense-reversing) barrier state, kept in the interleaved L1 region
// so every core observes the same words. Unlike every wake_up_all()+wfi barrier
// in the runtime, this barrier never sleeps: cores busy-wait on a shared sense
// flag. That keeps the kernel off the Snitch wake-up path, whose small
// outstanding-wake counter silently drops pulses under tight, all-core
// synchronization and deadlocks the run (observed on g1024, and on g4096/g256 at
// higher iteration counts). The hardware is left untouched -- only the test's
// synchronization strategy changes, so we still exercise the real cores + NoC.
uint32_t volatile sb_bar_cnt __attribute__((section(".l1")));
uint32_t volatile sb_bar_sense __attribute__((section(".l1")));

// Rendezvous exactly `num_active` cores without ever issuing a wake-up.
// `local_sense` is a per-core value (living on each core's stack) that flips on
// every call; the last core to arrive flips the shared sense to release the
// rest. Only the working cores ever call this, so parked idle cores never
// interfere with the count.
static inline void sb_spin_barrier(uint32_t num_active, uint32_t *local_sense) {
  uint32_t s = *local_sense ^ 1U;
  *local_sense = s;
  // Publish this core's pre-barrier stores to the shared L1 before it announces
  // arrival. The sleep-based barriers get this for free (the long wfi drains the
  // store path); a spin barrier resumes immediately, so it must be explicit.
  __sync_synchronize();
  if ((num_active - 1) ==
      __atomic_fetch_add(&sb_bar_cnt, 1, __ATOMIC_RELAXED)) {
    // Last arriver: rearm the counter for the next round, then release.
    __atomic_store_n(&sb_bar_cnt, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&sb_bar_sense, s, __ATOMIC_RELEASE);
  } else {
#ifdef SB_PROGRESS
    // Diagnostic: core 0 periodically reports how many cores have arrived. If
    // this number keeps climbing, the barrier is merely slow (contention); if it
    // freezes below num_active, a core never arrived (true deadlock).
    uint32_t spins = 0;
    const uint32_t is_c0 = (mempool_get_core_id() == 0);
    while (__atomic_load_n(&sb_bar_sense, __ATOMIC_ACQUIRE) != s) {
      mempool_wait(NUM_CORES);
      if (is_c0 && (++spins & 0xFu) == 0) {
        printf("[bar] arrived=%u/%u\n",
               __atomic_load_n(&sb_bar_cnt, __ATOMIC_RELAXED), num_active);
      }
    }
#else
    while (__atomic_load_n(&sb_bar_sense, __ATOMIC_ACQUIRE) != s) {
      // Light backoff so 1000+ cores don't hammer the flag every cycle.
      mempool_wait(NUM_CORES);
    }
#endif
  }
  // Make sure post-barrier reads observe every core's published writes.
  __sync_synchronize();
}

// One-shot, all-core rendezvous used only for setup/teardown while every core is
// still live. Called just a couple of times (never in a tight loop), so the
// wake_up_all() barrier is safe here: isolated broadcasts cannot pile up
// outstanding wake-ups the way back-to-back loop barriers do.
static inline void sb_setup_barrier(void) { mempool_barrier(NUM_CORES); }

static void report_results(uint32_t cycles) {
  const float maxcut =
      compute_maxcut(x_curr, l2_row_ptr, l2_col_idx, num_nodes);
  const float accuracy = 100.0f * maxcut / (float)optimal_cut;

  printf("SB max-cut testbench (%s)\n", bifurcation_lattice_name);
  printf("  iterations : %d\n", SB_NUM_ITER);
  printf("  maxcut     : %d\n", (int)maxcut);
  printf("  optimal    : %d\n", optimal_cut);
  printf("  accuracy   : %d%%\n", (int)accuracy);
  printf("  cycles     : %u\n", cycles);
}

int main() {
  uint32_t core_id = mempool_get_core_id();
  const uint32_t num_cores = mempool_get_core_count();
  mempool_barrier_init(core_id);

  uint32_t spin_start;
  uint32_t spin_stop;

  // Number of cores that actually do work and take part in the loop barrier. By
  // default every core works; the g256-style case (more cores than nodes) parks
  // the surplus cores and syncs only one working core per node.
  uint32_t bar_num = num_cores;
  uint32_t idle = 0;      // idle cores are parked and never touch the barrier
  uint32_t sense = 0;     // per-core sense flag for the spin barrier

  if (num_nodes > num_cores) {
    // Multiple graph nodes per core (e.g. g4096 on 1024 cores -> 4 nodes/core).
    // Every core works.
    if (num_nodes % num_cores != 0) {
      if (core_id == 0) {
        printf("ERROR: num_nodes (%d) must divide num_cores (%d)\n", num_nodes,
               num_cores);
      }
      sb_setup_barrier();
      return 1;
    }
    const uint32_t spins_per_core = num_nodes / num_cores;
    spin_start = core_id * spins_per_core;
    spin_stop = spin_start + spins_per_core;
  } else if (num_cores > num_nodes) {
    // Multiple cores per graph node (e.g. g256 on 1024 cores -> 4 cores/node).
    // Only one primary core per node does work; the surplus cores contribute
    // nothing, so we park them and keep them out of every loop barrier.
    if (num_cores % num_nodes != 0) {
      if (core_id == 0) {
        printf("ERROR: num_cores (%d) must divide num_nodes (%d)\n", num_cores,
               num_nodes);
      }
      sb_setup_barrier();
      return 1;
    }
    const uint32_t cores_per_node = num_cores / num_nodes;
    const uint32_t node_id = core_id / cores_per_node;
    bar_num = num_nodes; // one working core per node
    if (core_id % cores_per_node == 0) {
      spin_start = node_id;
      spin_stop = node_id + 1;
    } else {
      spin_start = 0;
      spin_stop = 0;
      idle = 1;
    }
  } else {
    // One graph node per core.
    spin_start = core_id;
    spin_stop = core_id + 1;
  }

  // One-shot setup while every core is still live: core 0 loads the initial
  // spins and clears the spin-barrier state, then all cores rendezvous once via
  // the (safe, isolated) wake-up barrier before the working cores take over.
  if (core_id == 0) {
    memcpy(x_curr, l2_x_init, num_nodes * sizeof(float));
    sb_bar_cnt = 0;
    sb_bar_sense = 0;
  }
  sb_setup_barrier();

  // Park the idle cores. They do no work and must not touch any later barrier.
  // The simulation ends when a working core returns from main and raises EOC,
  // so idle cores never need to wake or return.
  if (idle) {
    while (1) {
      mempool_wfi();
    }
  }

  uint32_t time_start = 0;
  uint32_t time_end = 0;
  if (core_id == 0) {
    time_start = mempool_get_timer();
    mempool_start_benchmark();
  }

  for (uint32_t iter = 1; iter < (uint32_t)SB_NUM_ITER; iter++) {
    const float decay =
        1.0f - ((float)(iter - 1) / (float)(SB_NUM_ITER - 1));

#ifdef SB_PROGRESS
    // Diagnostic heartbeat: makes forward progress observable in the transcript
    // (a stuck barrier stops printing). Off by default so it never perturbs the
    // benchmark cycle count; enable with -DSB_PROGRESS for validation runs.
    if (core_id == 0) {
      printf("[iter %u/%u]\n", iter, (uint32_t)SB_NUM_ITER);
    }
#endif

    leaf_sb_update(x_curr, x_next, spin_start, spin_stop, l2_row_ptr,
                   l2_col_idx, sb_a, sb_b, decay, sb_c);

    sb_spin_barrier(bar_num, &sense);

    for (uint32_t spin = spin_start; spin < spin_stop; spin++) {
      x_curr[spin] = x_next[spin];
    }

    sb_spin_barrier(bar_num, &sense);
  }

  if (core_id == 0) {
    mempool_stop_benchmark();
    time_end = mempool_get_timer();
    report_results(time_end - time_start);
  }

  sb_spin_barrier(bar_num, &sense);
  return 0;
}
