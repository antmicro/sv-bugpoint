#!/bin/bash
filename=$(basename "$1")
name="${filename%.*}"
verilator --binary "$1" > /dev/null 2> /dev/null && timeout 15s "obj_dir/V$name" | grep "Verilog \$finish"