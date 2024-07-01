#!/bin/sh

verilator --cc --autoflush --timescale 1ns/1ps --timing --top-module caliptra_top_tb "$1" \
  -Wno-WIDTH -Wno-UNOPTFLAT -Wno-LITENDIAN -Wno-CMPCONST -Wno-MULTIDRIVEN -Wno-UNPACKED -Wno-ALWCOMBORDER 2>actual_stderr 

# if [ $? -ne 0 ]; then
#   EXIT_CODE=$?
# elif; then
#   cmp actual_stderr golden_stderr
#   EXIT_CODE=$?
# fi
sed 's/[a-z_-]*.sv//g' actual_stderr -i
sed 's/:[0-9]*//g' actual_stderr -i
sed 's/[0-9]* |//g' actual_stderr -i
sed 's/[\t\n ]\+/ /g' actual_stderr -i
printf "\n\n\n"
diff golden_stderr actual_stderr --color && printf "SUCCESS\n\n\n"
EXIT_CODE=$?
# cat actual_stderr

[ -n "$GOLDEN" ] && cp actual_stderr golden_stderr

exit "$EXIT_CODE"

