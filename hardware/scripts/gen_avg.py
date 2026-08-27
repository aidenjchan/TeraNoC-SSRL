#!/usr/bin/env python3

# Copyright 2022 ETH Zurich and University of Bologna.
# Solderpad Hardware License, Version 0.51, see LICENSE for details.
# SPDX-License-Identifier: SHL-0.51

# This script takes a set of .csv files in one of the results folders and
# generates the average performances over all the cores used.
# Author: Marco Bertuletti <mbertuletti@iis.ee.ethz.ch>

import os
import csv
import argparse

ext = ('.csv')

parser = argparse.ArgumentParser()
parser.add_argument(
    '--folder',
    '-f',
    help='Name of the results folder with traces to be averaged.'
)
args = parser.parse_args()

os.chdir(args.folder)
path = os.getcwd()
print(path)

remove_keys = {'core', 'section', 'start', 'end',
               'snitch_load_latency', 'snitch_load_region',
               'snitch_load_tile', 'snitch_store_region',
               'snitch_store_tile'}

for fname in os.listdir(path):
    if not fname.endswith(ext):
        continue
    with open(fname, 'r') as f:
        reader = csv.DictReader(f)
        headers = reader.fieldnames
        rows = list(reader)

    sections = sorted(set(int(r['section']) for r in rows))

    print("\n")
    print("*******************************")
    print("**    AVERAGE PERFORMANCE    **")
    print("*******************************")
    print("")

    for section in sections:
        print("Section %d:\n" % section)
        section_rows = [r for r in rows if int(r['section']) == section]
        n = len(section_rows)
        if n == 0:
            continue
        keys = [k for k in headers if k not in remove_keys]
        for key in keys:
            try:
                values = []
                for r in section_rows:
                    val = r[key]
                    if val == '' or val == 'nan':
                        values.append(0.0)
                    else:
                        values.append(float(val))
                avg = sum(values) / len(values)
            except (ValueError, TypeError, ZeroDivisionError):
                continue
            print("%-30s %4.4f" % (key, avg))
