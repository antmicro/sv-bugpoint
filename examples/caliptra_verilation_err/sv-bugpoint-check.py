#!/usr/bin/env python3
"""
SPDX-License-Identifier: Apache-2.0

test script that asserts that verilation fails with same error message as in golden file

meant to debug case with caliptra-rtl@67fc658 and verilator@9a8e68928
"""

import subprocess
import sys
import shutil
import shlex
import os


def verilate_and_save_errmsg(verilator_argv):
    """run verilator, strip out volatile metadata from stderr and save it to file"""
    subprocess.Popen(
        ["sv-bugpoint-strip-verilator-errmsg"],
        stdin=subprocess.Popen(verilator_argv, stderr=subprocess.PIPE).stderr,
        stdout=open("actual_stderr", "w", encoding="utf-8"),
    ).communicate()


def check():
    """check if actual equals golden, print diff if not"""
    diff_proc = subprocess.run(
        ["diff", "golden_stderr", "actual_stderr", "--color"], check=False
    )
    return diff_proc.returncode == 0


CMD = """
verilator --cc --autoflush --timescale 1ns/1ps --timing --top-module caliptra_top_tb \
  -Wno-WIDTH -Wno-UNOPTFLAT -Wno-LITENDIAN -Wno-CMPCONST -Wno-MULTIDRIVEN -Wno-UNPACKED -Wno-ALWCOMBORDER
"""

verilate_and_save_errmsg(shlex.split(CMD) + [sys.argv[1]])
print("\n\n\n", end="")
if check():
    print("SUCCESS\n\n\n", end="")
    sys.exit(0)
else:
    if "GOLDEN" in os.environ:
        shutil.copyfile("actual_stderr", "golden_stderr")
    sys.exit(1)
