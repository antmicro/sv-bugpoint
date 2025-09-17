#!/bin/bash
verilator --lint-only "$1" || exit 1