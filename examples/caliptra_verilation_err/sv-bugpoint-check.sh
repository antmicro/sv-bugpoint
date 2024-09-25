#!/bin/sh
# SPDX-License-Identifier: Apache-2.0

# test script that asserts that verilation fails with same error message as in golden file

# meant to debug case with caliptra-rtl@67fc658 and verilator@9a8e68928

verilator --cc --autoflush --timescale 1ns/1ps --timing --top-module caliptra_top_tb "$1" \
  -Wno-WIDTH -Wno-UNOPTFLAT -Wno-LITENDIAN -Wno-CMPCONST -Wno-MULTIDRIVEN -Wno-UNPACKED -Wno-ALWCOMBORDER \
  2>&1 >/dev/null | sv-bugpoint-strip-verilator-errmsg > actual_stderr

printf "\n\n\n"
diff golden_stderr actual_stderr --color && printf "SUCCESS\n\n\n"
EXIT_CODE=$?

[ -n "$GOLDEN" ] && cp actual_stderr golden_stderr

exit "$EXIT_CODE"
