#!/bin/bash
verilator --binary "$1" > /dev/null 2> /dev/null && timeout 1s obj_dir/Vcomment_dir_in > /dev/null