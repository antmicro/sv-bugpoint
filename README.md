# The bugpoint

Bugpoint extracts minimized test cases out of code that triggers bugs in Verilator.
It uses Slang for parsing SystemVerilog into a syntax tree that is then traversed and stripped part by part.
Every removal attempt is tested with a user supplied script to check whether it didn't hide the bug, or break
code in a different way. Minimization is done in a loop until no further changes can be done.
Bugpoint currently only works on single-file inputs (e.g. already preprocessed source).


## Build
```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
```

Dependencies should be fetched and built automatically.
Bugpoint executable is placed in `build/bugpoint`.

## Usage
Bugpoint reads or writes the following files in the current working directory:
#### bugpoint_input.sv
Input code that bugpoint will try to minimize.
In order to get one from a multifile design, you can preprocess it by
replacing `--cc` or `--binary` flags in the Verilator invocation you use
with `-E -P` (`-P` is optional, but makes output more concise) and
redirecting stdout to `bugpoint_input.sv`.

#### bugpoint_check.sh
Script that takes the path to a SystemVerilog file as the first argument and tests it with Verilator.
It should exit with 0 if the same bug or error message is encountered and non-zero otherwise.
For inspiration see [examples/caliptra_verilation_err/bugpoint_check.sh](examples/caliptra_verilation_err/bugpoint_check.sh)
and [examples/caliptra_vcd/bugpoint_check.sh](examples/caliptra_vcd/bugpoint_check.sh).

#### bugpoint_minimized.sv
Output file that contains minimized code that is known to work (or rather, to break in expected way).

#### bugpoint_tmp.sv
Temporary file to be checked with the script. It contains code with an applied removal attempt.

#### bugpoint_stats
Tab-delimited minimization statistics. For readability, it is recommeded to view it with `column -t bugpoint_stats`

You only need to provide `bugpoint_input.sv` and `bugpoint_check.sh` in the current working directory. After that, simply launch the `bugpoint` executable.

**NOTE:** This workflow is not the most convenient one, and is guaranteed to change in future.
Ideally, Bugpoint would create preprocessed code itself, and not require to write check
scripts for common types of failures.
