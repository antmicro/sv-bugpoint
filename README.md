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
If you are going to use accompanying scripts, it is recommended to add whole `scripts/` dir
to your path, since some scripts may depend on each other.

## Usage
Bugpoint reads or writes the following files in the current working directory:
#### bugpoint_input.sv
Input code that bugpoint will try to minimize. In order to get one from a multifile design,
you can use preprocessor of your choice (e.g `verilator -E -P inputs_and_other_flags... > bugpoint_input.sv`)

#### bugpoint_check.sh
Script that takes the path to a SystemVerilog file as the first argument and asserts that the same bug occurred.
It should exit with 0 if the same bug or error message is encountered and non-zero otherwise.
For inspiration see [examples/caliptra_verilation_err/bugpoint_check.sh](examples/caliptra_verilation_err/bugpoint_check.sh)
and [examples/caliptra_vcd/bugpoint_check.sh](examples/caliptra_vcd/bugpoint_check.sh).

#### bugpoint_minimized.sv
Output file that contains minimized code that is known to work (or rather, to break in expected way).

#### bugpoint_tmp.sv
Temporary file to be checked with the script. It contains code with an applied removal attempt.

#### bugpoint_trace
Verbose, tab-delimited trace with stats and aditional info about each removal attempt ([example](examples/caliptra_verilation_err/bugpoint_trace)).
It can be turned into concise, high-level summary with [bugpoint_trace_summary script](scripts/bugpoint_trace_summary) ([example](examples/caliptra_verilation_err/bugpoint_trace_summarized)).


You only need to provide `bugpoint_input.sv` and `bugpoint_check.sh` in the current working directory. After that, simply launch the `bugpoint` executable.

### bugpoint_verilator_gen script
In case of Verilator workflows, there is [bugpoint_verilator_gen script](scripts/bugpoint_verilator_gen) for automatically generating `bugpoint_input.sv` and template of `bugpoint_check.sv`
#### Usage
Run `bugpoint_verilator_gen --init`, and then run each command needed for bug reproduction with `bugpoint_verilator_gen` prepended. For example:
```sh
bugpoint_verilator_gen --init
bugpoint_verilator_gen verilator --cc -CFLAGS "-std=c++17" ...
bugpoint_verilator_gen make -C obj_dir/ -j ...
bugpoint_verilator_gen ./obj_dir/Vsim
```

The script attempts to:
- Produce an input file for sv-bugpoint with preprocessed code.
- Produce a template of the check script with:
  - a Verilator invocation adjusted to use the preprocessed source,
  - Other required commands copied (and extended with `|| exit $?` if feasible),
  - An example assertion for a simple failure case.

Script works on a best-effort basis, and it is expected that the script it produces will require some manual adjustments.
