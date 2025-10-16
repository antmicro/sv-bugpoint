INPUT_DIR="input_files"

.PHONY: test_dont_abort_after_err
test_dont_abort_after_err:
	@$(MAKE) -k test

.PHONY: test
test: test_short test_caliptra test_comment_dir test_tricky_missing_newline test_irremovable_verilator_config test_comment_dir2

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

.PHONY: test_empty
test_empty:
	@timeout 15s ./run_test empty checkexit0.sh ${INPUT_DIR}/short_in/empty.sv

.PHONY: test_comment_dir
test_comment_dir:
	@./run_test comment_dir checkverilator_run_finish.sh ${INPUT_DIR}/comment_dir_in.sv

.PHONY: test_comment_dir2
test_comment_dir2:
	@./run_test comment_dir2 checkverilator_run_finish.sh ${INPUT_DIR}/comment_dir_in2.sv

.PHONY: test_tricky_missing_newline
test_tricky_missing_newline:
	@./run_test tricky_missing_newline checkgrep.sh input_files/tricky_missing_newline.sv

.PHONY: test_irremovable_verilator_config
test_irremovable_verilator_config:
	@./run_test test_irremovable_verilator_config checkgrep.sh input_files/irremovable_cfg.sv

.PHONY: test_labels
test_labels:
	@./run_test labels checkverilator_run_finish.sh ${INPUT_DIR}/labels.sv

.PHONY: test_dpi
test_dpi:
	@./run_test dpi checkverilator_lint.sh ${INPUT_DIR}/dpi.sv

.PHONY: test_argument
test_argument:
	@./run_test argument checkverilator_run_finish.sh ${INPUT_DIR}/argument.sv && \
	awk -F'\t' '/functionArgRemover/ && $$1 > 1 {print "check on trace failed - functionArgRemover should need only one pass"; exit(1)}' out/argument/debug/trace

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
