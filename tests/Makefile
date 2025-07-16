INPUT_DIR="input_files"

.PHONY: test_dont_abort_after_err
test_dont_abort_after_err:
	@$(MAKE) -k test

.PHONY: test
test: test_short test_caliptra

.PHONY: test_short
test_short: test_short_exit0 test_short_exit1 test_short_grep test_short_verilator_errmsg test_short_multi_file_verilator_errmsg test_short_multi_file_flag_y_verilator_errmsg test_short_multi_file_flag_f_verilator_errmsg

.PHONY: test_short_exit0
test_short_exit0:
	@./run_test short_exit0 checkexit0.sh ${INPUT_DIR}/short_in.sv

.PHONY: test_short_verilator_errmsg
test_short_verilator_errmsg:
	@verilator --version >/dev/null || (printf "FAILED: verilator not found\n"; exit 1)
	@verilator --version | grep -q "5.016" || printf "NOTE: verilator version != 5.016. This may cause following test to fail\n"
	@./run_test short_verilator_errmsg checkverilator_errmsg_short.sh ${INPUT_DIR}/short_in.sv

.PHONY: test_short_multi_file_verilator_errmsg
test_short_multi_file_verilator_errmsg:
	@verilator --version >/dev/null || (printf "FAILED: verilator not found\n"; exit 1)
	@verilator --version | grep -q "5.016" || printf "NOTE: verilator version != 5.016. This may cause following test to fail\n"
	@./run_test short_multifile_verilator_errmsg checkverilator_errmsg_short.sh ${INPUT_DIR}/short_in/a.sv ${INPUT_DIR}/short_in/subdir/a.sv ${INPUT_DIR}/short_in/c.sv ${INPUT_DIR}/short_in/d.sv

.PHONY: test_short_multi_file_flag_y_verilator_errmsg
test_short_multi_file_flag_y_verilator_errmsg:
	@verilator --version >/dev/null || (printf "FAILED: verilator not found\n"; exit 1)
	@verilator --version | grep -q "5.016" || printf "NOTE: verilator version != 5.016. This may cause following test to fail\n"
	@./run_test short_multifile_flag_y_verilator_errmsg checkverilator_errmsg_short.sh -y ${INPUT_DIR}/short_in -y ${INPUT_DIR}/short_in/subdir

.PHONY: test_short_multi_file_flag_f_verilator_errmsg
test_short_multi_file_flag_f_verilator_errmsg:
	@verilator --version >/dev/null || (printf "FAILED: verilator not found\n"; exit 1)
	@verilator --version | grep -q "5.016" || printf "NOTE: verilator version != 5.016. This may cause following test to fail\n"
	@./run_test short_multifile_flag_f_verilator_errmsg checkverilator_errmsg_short.sh -f ${INPUT_DIR}/short_in/filelist.f

.PHONY: test_short_exit1
test_short_exit1:
	@./run_test short_exit1 checkexit1.sh ${INPUT_DIR}/short_in.sv

.PHONY: test_short_grep
test_short_grep:
	@./run_test short_grep checkgrep.sh ${INPUT_DIR}/short_in.sv

.PHONY: test_caliptra
test_caliptra: test_caliptra_exit0 test_caliptra_grep

.PHONY: test_caliptra_exit0
test_caliptra_exit0:
	@./run_test caliptra_exit0 checkexit0.sh ${INPUT_DIR}/caliptra_in.sv

# we don't test caliptra_exit1 by default as it would take very long
.PHONY: test_caliptra_exit1
test_calipta_exit1:
	@./run_test caliptra_exit1 checkexit1.sh ${INPUT_DIR}/caliptra_in.sv

.PHONY: test_caliptra_grep
test_caliptra_grep:
	@./run_test caliptra_grep checkgrep.sh ${INPUT_DIR}/caliptra_in.sv
