# sv-bugpoint

Copyright (c) 2024 [Antmicro](https://www.antmicro.com)

![sv-bugpoint-logo-github-readme](https://github.com/user-attachments/assets/94caef96-85d6-4e50-8b39-ff9d0f5fe7e7)


`sv-bugpoint` minimizes SystemVerilog code while preserving a user-defined property of that code.
For example, it can be used for acquiring minimal test cases that trigger certain bugs in SystemVerilog tooling.

It uses [Slang](https://github.com/MikePopoloski/slang) for parsing SystemVerilog into a syntax tree that is then traversed and reduced part by part.
Each removal attempt is tested with a user-supplied script to check whether it didn't change the property of interest (e.g. hide the bug, or cause a different error message).
Minimization is performed iteratively until no further changes can be applied.

`sv-bugpoint` currently only works on single-file inputs (e.g. source that's already been preprocessed).

## Building

Run:
```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
```

For faster builds you may want to use following invocation instead (you may have to install `mold`, `ninja` and `ccache` beforehand):
```sh
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_LINKER_TYPE=MOLD -G Ninja -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build build -j8
```

The dependencies will be fetched and built automatically.
The `sv-bugpoint` executable is placed in the `build/sv-bugpoint` directory.
If you are going to use the accompanying scripts, it is recommended to add the entire `scripts/` dir
to your path, as some scripts may depend on each other.

## Usage

First, you need to prepare:
- a script that takes the path to a SystemVerilog file as the first argument and asserts that the same bug (or another property you wish to preserve) is still occurring.
It should exit with 0 if the assertion is successful.
For reference, see [`examples/caliptra_verilation_err/sv-bugpoint-check.sh`](examples/caliptra_verilation_err/sv-bugpoint-check.sh) and [`examples/caliptra_vcd/sv-bugpoint-check.sh`](examples/caliptra_vcd/sv-bugpoint-check.sh).
The script can be written in any language you are comfortable with, it just needs to be an executable. See the Python example: [`examples/caliptra_verilation_err/sv-bugpoint-check.py`](examples/caliptra_verilation_err/sv-bugpoint-check.py).
- SystemVerilog code that `sv-bugpoint` will attempt to minimize. In order to get one from a multi-file design,
you can use a preprocessor of your choice (e.g `verilator -E -P [other flags...] > sv-bugpoint-input.sv`).

After that, run:

```sh
sv-bugpoint <OUT_DIR> <CHECK_SCRIPT> <INPUT_SV>
```

The output directory will be populated with:
- `sv-bugpoint-minimized.sv` - minimized code that satisfies the assertion checked by the provided script,
- `tmp/<INPUT_SV>` - a copy of the previous file with a removal attempt applied, to be checked with the provided script,
- `sv-bugpoint-trace` - verbose, tab-delimited trace with stats and additional info about each removal attempt ([example](examples/caliptra_verilation_err/sv-bugpoint-trace)).
  It can be turned into a concise, high-level summary with the [`sv-bugpoint-trace_summary script`](scripts/sv-bugpoint-trace_summary) ([example](examples/caliptra_verilation_err/sv-bugpoint-trace_summarized)).

There are flags that enable additional dumps:
- `--save-intermediates` saves each removal attempt in `<OUT_DIR>/intermediates/attempt<index>.sv`.
- `--dump-trees` saves dumps of Slang's AST.

### Automatically generating check scripts

If your goal is to debug Verilator, the [`sv-bugpoint-verilator-gen` script](scripts/sv-bugpoint-verilator-gen) can automatically generate an input test case and a check script template for you.

#### Usage

Run `sv-bugpoint-verilator-gen --init`, and then run each command needed for bug reproduction, prepended with `sv-bugpoint-verilator-gen`. For example:
```sh
sv-bugpoint-verilator-gen --init
sv-bugpoint-verilator-gen verilator --cc -CFLAGS "-std=c++17" ...
sv-bugpoint-verilator-gen make -C obj_dir/ -j ...
sv-bugpoint-verilator-gen ./obj_dir/Vsim
```

The script attempts to:
- Produce a single input file for `sv-bugpoint` with preprocessed code.
- Produce a template of the check script with:
  - a Verilator invocation adjusted to use the preprocessed source,
  - other required commands copied (and extended with `|| exit $?` if feasible),
  - an example assertion for a simple failure case.

This script works on a best-effort basis, and it is expected that the result will require some manual adjustments.

## Testing and linting

`make`, `clang-format`, `shellcheck` and `verilator` are prerequisites for testing and linting.

To run all tests, linters and format-checkers, invoke:
```
make -f check.mk
```

in project root. To auto-apply linter/formatter fixes, run:
```
make -f check.mk autofix
```

Golden files used in tests can be regenerated using:
```
GOLDEN=1 make -f check.mk test
```
