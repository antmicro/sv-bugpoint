#!/bin/sh
# SPDX-License-Identifier: Apache-2.0

# test script that asserts that suplied file can be verilated, compiled
# and that `timer1_timeout_period` signals are constantly toggling (by scrapping
# vcd with awk and comparing it with golden file).

# Meant to debug case with caliptra-rtl@3e0d8ee and verilator@v5.010.
# See https://github.com/chipsalliance/caliptra-rtl/issues/79

verilator --cc -CFLAGS "-std=c++11" "$1" \
  +libext+.v+.sv +define+RV_OPENSOURCE \
  --timescale 1ns/1ps \
  -I"$CALIPTRA_ROOT"/tools/scripts \
  -Wno-WIDTH -Wno-UNOPTFLAT -Wno-LITENDIAN -Wno-CMPCONST -Wno-MULTIDRIVEN -Wno-UNPACKED -Wno-IMPLICITSTATIC \
  --top-module caliptra_top_tb \
  -exe test_caliptra_top_tb.cpp --autoflush --trace --trace-structs || exit 1

cp "$CALIPTRA_ROOT"/src/integration/tb/test_caliptra_top_tb.cpp obj_dir/
make -j -e -C obj_dir/ -f Vcaliptra_top_tb.mk OPT_FAST="-O0" OPT_GLOBAL="-O0" VM_PARALLEL_BUILDS=1 2>/dev/null

cleanup() {
  jobs -p | xargs -r kill -9 # kill background jobs
  wait
}

rm -f sim.vcd
mkfifo sim.vcd
trap cleanup EXIT INT HUP TERM

timeout 3 ./obj_dir/Vcaliptra_top_tb &
# extract all changes to timer1_timeout_period[0] and timer1_timeout_period[1] before timestamp "40"
timeout 3 awk '
/var wire .* timer1_timeout_period\[/ { ID_TO_NAME[$4]=$5 }
NF == 2 && $2 in ID_TO_NAME { print $1, ID_TO_NAME[$2] }
/^#/ && gensub("^#", "", "g")+0 >= 40{exit}
' sim.vcd > actual_out || exit 1

cleanup
diff golden_out actual_out --color=always && printf "SUCCESS\n\n\n"
EXIT_CODE=$?
[ -n "$GOLDEN" ] && cp actual_out golden_out
exit "$EXIT_CODE"
