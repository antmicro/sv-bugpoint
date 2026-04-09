#!/bin/sh
# SPDX-License-Identifier: Apache-2.0

# assert that certain part of input wasn't removed

grep "finish" "$@" -q && exit 0 # for remove_covergroups.sv

exit 1
