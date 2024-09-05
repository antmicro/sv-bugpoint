# sv-bugpoint

sv-bugpoint extracts minimized test cases out of code that triggers bugs in Verilator.
It uses Slang for parsing SystemVerilog into a syntax tree that is then traversed and stripped part by part.
Every removal attempt is tested with a user supplied script to check whether it didn't hide the bug, or break
code in a different way. Minimization is done in a loop until no further changes can be done.
sv-bugpoint currently only works on single-file inputs (e.g. already preprocessed source).


## Build
```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
```

Dependencies should be fetched and built automatically.
sv-bugpoint executable is placed in `build/sv-bugpoint`.
If you are going to use accompanying scripts, it is recommended to add whole `scripts/` dir
to your path, since some scripts may depend on each other.

## Usage
First, you need to prepare:
- a script that takes the path to a SystemVerilog file as the first argument and asserts that the same bug occurred (or another property you wish to preserve).
It should exit with 0 if the assertion is successful.
For inspiration, see [examples/caliptra_verilation_err/sv-bugpoint-check.sh](examples/caliptra_verilation_err/sv-bugpoint-check.sh)
and [examples/caliptra_vcd/sv-bugpoint-check.sh](examples/caliptra_vcd/sv-bugpoint-check.sh).
- SystemVerilog code that sv-bugpoint will try to minimize. In order to get one from a multi-file design,
you can use a preprocessor of your choice (e.g `verilator -E -P inputs_and_other_flags... > sv-bugpoint-input.sv`)

After that, simply launch `sv-bugpoint outDir/ checkscript.sh input.sv`.

The output directory will be populated with:
- `sv-bugpoint-minimized.sv` - minimized code that satisfies the assertion checked by the provided script,
- `sv-bugpoint-tmp.sv` - a copy of the previous file with a removal attempt applied, to be checked with the provided script,
- `sv-bugpoint-trace` - verbose, tab-delimited trace with stats and aditional info about each removal attempt ([example](examples/caliptra_verilation_err/sv-bugpoint-trace)).
  It can be turned into a concise, high-level summary with the [sv-bugpoint-trace_summary](scripts/sv-bugpoint-trace_summary) ([example](examples/caliptra_verilation_err/sv-bugpoint-trace_summarized)) script.

There are flags that enable additional dumps:
- `--save-intermediates` saves each removal attempt in `outDir/intermediates/attempt<index>.sv`.
- `--dump-trees` saves dumps of Slang's AST.

### sv-bugpoint-verilator-gen script
In case of Verilator workflows, there is [sv-bugpoint-verilator-gen script](scripts/sv-bugpoint-verilator-gen) for automatically generating an input test case, and a check script template.
#### Usage
Run `sv-bugpoint-verilator-gen --init`, and then run each command needed for bug reproduction with `sv-bugpoint-verilator-gen` prepended. For example:
```sh
sv-bugpoint-verilator-gen --init
sv-bugpoint-verilator-gen verilator --cc -CFLAGS "-std=c++17" ...
sv-bugpoint-verilator-gen make -C obj_dir/ -j ...
sv-bugpoint-verilator-gen ./obj_dir/Vsim
```

The script attempts to:
- Produce an input file for sv-bugpoint with preprocessed code.
- Produce a template of the check script with:
  - a Verilator invocation adjusted to use the preprocessed source,
  - Other required commands copied (and extended with `|| exit $?` if feasible),
  - An example assertion for a simple failure case.

Script works on a best-effort basis, and it is expected that the script it produces will require some manual adjustments.

## Tests
In order to lanuch tests go to `tests/` directory and type `make`
