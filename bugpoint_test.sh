#!/bin/sh

verilator --cc -CFLAGS "-std=c++11" "$1" \
  +libext+.v+.sv +define+RV_OPENSOURCE \
  --timescale 1ns/1ps \
  -I/home/ant/tmp/caliptra_workspace/caliptra-rtl/tools/scripts \
  -Wno-WIDTH -Wno-UNOPTFLAT -Wno-LITENDIAN -Wno-CMPCONST -Wno-MULTIDRIVEN -Wno-UNPACKED -Wno-IMPLICITSTATIC \
  --top-module caliptra_top_tb \
   \
  -exe test_caliptra_top_tb.cpp --autoflush --trace --trace-structs || exit 1
#+define+CALIPTRA_INTERNAL_TRNG
cp /home/ant/tmp/caliptra_workspace//caliptra-rtl//src/integration/tb/test_caliptra_top_tb.cpp obj_dir/
make -j -e -C obj_dir/ -f Vcaliptra_top_tb.mk OPT_FAST="-O0" OPT_GLOBAL="-O0" VM_PARALLEL_BUILDS=1 2>/dev/null

cleanup() {
  jobs -p | xargs -r kill -9 # kill background jobs
  wait
}

rm -f sim.vcd
mkfifo sim.vcd
trap cleanup EXIT INT HUP TERM
timeout 3 ./obj_dir/Vcaliptra_top_tb &
timeout 3 gawk '/var wire .* timer1_timeout_period\[/ {ID_TO_NAME[$4]=$5} {for(id in ID_TO_NAME) if($0 ~ id"$") print $1, ID_TO_NAME[id] } /^#/ && gensub("^#", "", "g")+0 >= 40{exit}' sim.vcd > actual_out || exit 1
cleanup
diff golden_out actual_out --color=always && printf "SUCCESS\n\n\n"
EXIT_CODE=$?
[ -n "$GOLDEN" ] && cp actual_out golden_out
exit "$EXIT_CODE"
