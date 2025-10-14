#!/bin/sh
# SPDX-License-Identifier: Apache-2.0

# assert that certain part of input wasn't removed

grep "DOE_NOP    = 2'b00" "$@" -q && exit 0 # for caliptra_in.sv
grep "input cin" "$@" && exit 0 # for short_in.sv
grep 'irremovable comment with no newline' "$@" && exit 0 # for tricky_missing_newline.sv
awk '/^`verilator_config$/{p=1} p' "$@" | grep 'irremovable verilator_config section' && exit 0

exit 1
