#!/usr/bin/env python3

# Copyright 2026 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51

"""Run GEMV f16/f32 terapool benchmarks sequentially.

Each job runs from the hardware/ directory:
  app=<app> make benchmark config=terapool

Build the apps first, e.g. from software/apps/baremetal/:
  make gemv_f16 gemv_f32 config=terapool COMPILER=llvm
"""

import argparse
import os
import subprocess
import sys
from datetime import datetime
from pathlib import Path

HARDWARE_DIR = Path(__file__).resolve().parent
CONFIG = "terapool"

DEFAULT_APPS = [
    "gemv_f16",
    "gemv_f32",
]


def log(msg: str) -> None:
    ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    print(f"[{ts}] {msg}", flush=True)


def run_benchmark(app: str, cwd: Path, dry_run: bool) -> int:
    cmd = ["make", "benchmark", f"config={CONFIG}"]
    env = {"app": app}

    log(f"RUN: app={app} {' '.join(cmd)} (cwd={cwd})")
    if dry_run:
        return 0

    run_env = os.environ.copy()
    run_env.update(env)

    proc = subprocess.run(
        cmd,
        cwd=str(cwd),
        env=run_env,
    )
    return proc.returncode


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run gemv_f16 and gemv_f32 terapool benchmarks back to back.",
    )
    parser.add_argument(
        "--apps",
        nargs="+",
        default=DEFAULT_APPS,
        help="App names to benchmark (default: gemv_f16 gemv_f32)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print planned commands without running make",
    )
    parser.add_argument(
        "--stop-on-failure",
        action="store_true",
        help="Stop after the first failed benchmark",
    )
    args = parser.parse_args()

    apps = args.apps
    total = len(apps)
    passed = 0
    failed = 0
    failed_apps = []

    log(f"===== Running {total} GEMV benchmarks (config={CONFIG}) =====")

    for i, app in enumerate(apps, 1):
        log("─" * 48)
        log(f"[{i}/{total}] Starting: {app}")

        rc = run_benchmark(app, HARDWARE_DIR, args.dry_run)
        if rc == 0:
            log(f"[{i}/{total}] PASSED: {app}")
            passed += 1
        else:
            log(f"[{i}/{total}] FAILED: {app} (exit code {rc})")
            failed += 1
            failed_apps.append(app)
            if args.stop_on_failure:
                break

    log("═" * 48)
    log(f"SUMMARY: {total} total | {passed} passed | {failed} failed")
    if failed_apps:
        log("Failed apps:")
        for app in failed_apps:
            log(f"  - {app}")
    if not args.dry_run:
        log(f"Results are in: {HARDWARE_DIR / 'results'}/")
    log("═" * 48)

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
