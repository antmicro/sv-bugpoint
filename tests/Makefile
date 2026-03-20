INPUT_DIR="input_files"

.PHONY: test_dont_abort_after_err
test_dont_abort_after_err:
	@$(MAKE) -k test

.PHONY: test
test: test_short test_caliptra test_comment_dir test_tricky_missing_newline test_irremovable_verilator_config test_comment_dir2

.PHONY: test_short
test_short: test_short_exit0 test_extern_inline test_truncator test_short_exit1 test_short_grep test_short_verilator_errmsg test_short_multi_file_verilator_errmsg test_short_multi_file_flag_y_verilator_errmsg test_short_multi_file_flag_f_verilator_errmsg test_generate

.PHONY: test_short_exit0
test_short_exit0:
	@./run_test short_exit0 checkexit0.sh ${INPUT_DIR}/short_in.sv

.PHONY: test_extern_inline
test_extern_inline:
	@./run_test extern_inline checkverilator_run_finish.sh ${INPUT_DIR}/extern_inline.sv --n-at-once 1 && \
	awk -F'\t' '/externInliner/ && $$1 > 1 {print "check on trace failed - externInliner should need only one pass"; exit(1)}' out/extern_inline/debug/trace

.PHONY: test_truncator
test_truncator:
	@./run_test truncator checkexit0.sh ${INPUT_DIR}/short_in.sv
	@awk -F'\t' '$$2=="fileTruncator" && $$4=="1"{found=1} END{if(!found){print "FAILED: fileTruncator pass not committed" > "/dev/stderr"; exit(1)}}' out/truncator/debug/trace

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

.PHONY: test_generate
test_generate:
	@./run_test generate checkverilator_run_finish.sh ${INPUT_DIR}/generate.sv

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

.PHONY: test_caliptra_grep_default
test_caliptra_grep_default:
	@./run_test caliptra_grep_default checkgrep.sh ${INPUT_DIR}/caliptra_in.sv

.PHONY: test_caliptra_grep_1_at_once
test_caliptra_grep_1_at_once:
	@./run_test caliptra_grep_1_at_once checkgrep.sh ${INPUT_DIR}/caliptra_in.sv --n-at-once 1

.PHONY: test_caliptra_grep
test_caliptra_grep: test_caliptra_grep_default test_caliptra_grep_1_at_once
	@# check if speculative reduction (used by default) lead to significant improvement
	@# in terms of number of check script runs
	@printf "TEST: caliptra_grep (speculative vs 1-at-once perf)\n"
	@wc -l out/caliptra_grep_default/debug/trace out/caliptra_grep_1_at_once/debug/trace --total never | awk '\
	NR==1{speculative=$$1} \
	NR==2{serial=$$1} \
	END{ \
		ratio=serial/speculative; \
		expected_ratio=8; \
		if (ratio >= expected_ratio) { \
			printf("PASSED: speculative scheme significantly reduces number of check script runs (ratio = %.2f, expected >= %.2f)\n", ratio, expected_ratio); \
		} else { \
			printf("FAILED: speculative scheme should significantly reduce number of check script runs (ratio = %.2f, expected >= %.2f)\n", ratio, expected_ratio); \
			exit(1); \
		} \
	}'

.PHONY: test_disable_line_remover
test_disable_line_remover:
	@./run_test disable_line_remover checkgrep.sh ${INPUT_DIR}/disable_line_remover.sv --fno-line-remover

.PHONY: test_remove_covergroup
test_remove_covergroup:
	@./run_test remove_covergroup checkgrepfinish.sh ${INPUT_DIR}/remove_covergroup.sv
