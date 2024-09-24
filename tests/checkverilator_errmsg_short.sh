#!/bin/bash
# SPDX-License-Identifier: Apache-2.0

# Written for verilator v5.016. It might break on other versions

increase_errmsg_compat() {
 # Dirty, best-effort hack for increasing compat with verilator versions that use different msg format
 sed 's/note In/In/g' | sed "s/'serial_adder'/serial_adder/g"
}

actual=$(verilator --cc -Wno-WIDTH --top-module serial_adder "$1" 2>&1 1>/dev/null |
sv-bugpoint-strip-verilator-errmsg | increase_errmsg_compat)

expected=$(cat <<EOF
%Error Member 'c' not found in structure
 ... In instance serial_adder
 assign foo.c = 0;
 ^
%Error Exiting due to 1 error(s)
EOF
)

printf "\n\n\n"
diff <(echo "$expected") <(echo "$actual") --color && printf "SUCCESS\n\n\n"
EXIT_CODE=$?

exit "$EXIT_CODE"
