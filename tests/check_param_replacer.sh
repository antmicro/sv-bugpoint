#!/bin/sh
# SPDX-License-Identifier: Apache-2.0

filename=$(basename "$1")
name="${filename%.*}"
# we don't want '$finish' asserts so we have to check the output
verilator --binary "$1" > /dev/null 2> /dev/null && timeout 15s "obj_dir/V$name" | grep '5,7,10,10,10,7,01010,11111,3' && exit 0

exit 1
