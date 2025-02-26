#!/bin/sh
# SPDX-License-Identifier: Apache-2.0

# assert that certain part of input wasn't removed

grep "DOE_NOP    = 2'b00" "$@" -q && exit 0 # for caliptra_in.sv
grep "input cin" "$@" && exit 0 # for short_in.sv

exit 1
