# The bugpoint

Bugpoint is tool meant to extract minimized example of code that triggers certain verilator bug, out of big design.
It uses slang for getting syntax tree that is then traversed and stripped part-by-part.
Every removal attempt is tested with user suplied script in order to check whether it wouldn't hide bug, or break
code in different way. Minimization is done in loop as long as any changes are being done.
For simplicity, bugpoint works on one-file inputs (e.g. already preprocessed source).


## Build
```sh
cmake -B build
cmake --build build -j8
```

Dependencies should be fetched and built automatically.
Bugpoint executable is placed in `build/bugpoint`.

## Usage
Bugpoint reads or writes following files in working dir:
#### bugpoint_input.sv
Input code that bugpoint will try to minimize.
In order to get one from multifile design, you can preprocess it, by
replacing `--cc` or `--binary` flags in verilator invocation you use,
with `-E -P` (`-P` is optional, but makes output more concise) and
redirecting stdout to bugpoint_input.sv

#### bugpoint_test.sh
Script that takes path to (system)verilog file as first arg and tests it.
It should exit with 0 if code triggers the same bug and not-zero otherwise.
For inspiration see [examples/caliptra_verilation_err/bugpoint_test.sh](examples/caliptra_verilation_err/bugpoint_test.sh)
and [examples/caliptra_vcd/bugpoint_test.sh](examples/caliptra_vcd/bugpoint_test.sh).

#### bugpoint_minimized.sv
Output file that contains minimized code that is known to work (or rather, to break in expected way).

#### bugpoint_test.sv
Temporary output file that contains code with applied removal attempt, that is going to be tested.

#### bugpoint_stats
Tab-delimited minimization statistics. For readability, it is recommeded to view it with `column -t bugpoint_stats`

After you get proper `bugpoint_input.sv` and `bugpoint_test.sh` in working dir, you can just launch `bugpoint` executable.

NOTE: This workflow is not the most convienient one, and is likely to change in future.
Ideally, bugpoint would create preprocessed code itself, and not require to write test
script in case of common types of failures.
