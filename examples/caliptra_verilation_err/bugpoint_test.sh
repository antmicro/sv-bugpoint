#!/bin/sh

# test script that asserts that verilation fails with same error message as in golden file

# meant to debug case with caliptra-rtl@67fc658 and verilator@9a8e68928

verilator --cc --autoflush --timescale 1ns/1ps --timing --top-module caliptra_top_tb "$1" \
  -Wno-WIDTH -Wno-UNOPTFLAT -Wno-LITENDIAN -Wno-CMPCONST -Wno-MULTIDRIVEN -Wno-UNPACKED -Wno-ALWCOMBORDER 2>actual_stderr 

# strip volatile metadata like filenames, filelines and redundant whitespace
sed 's/[a-z_-]*.sv//g' actual_stderr -i
sed 's/:[0-9]*//g' actual_stderr -i
sed 's/[0-9]* |//g' actual_stderr -i
sed 's/[\t\n ]\+/ /g' actual_stderr -i

printf "\n\n\n"
diff golden_stderr actual_stderr --color && printf "SUCCESS\n\n\n"
EXIT_CODE=$?

[ -n "$GOLDEN" ] && cp actual_stderr golden_stderr

exit "$EXIT_CODE"
