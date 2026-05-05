#!/bin/sh
# SPDX-License-Identifier: Apache-2.0

grep 'fallback else body survived' "$@" -q || exit 1
grep 'body-specific comment' "$@" -q || exit 1
grep 'standalone if body survived' "$@" -q || exit 1
grep 'else-if chain final body survived' "$@" -q || exit 1

exit 0
