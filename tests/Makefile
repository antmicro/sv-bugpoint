SHELL=/bin/bash -o pipefail
INPUT_DIR="input_files"

TEST_CASES := \
	test_short_exit0 \
	test_extern_inline \
	test_truncator \
	test_short_verilator_errmsg \
	test_short_multi_file_verilator_errmsg \
	test_short_multi_file_flag_y_verilator_errmsg \
	test_short_multi_file_flag_f_verilator_errmsg \
	test_short_exit1 \
	test_generate \
	test_if_body_replacer \
	test_short_grep \
	test_empty \
	test_comment_dir \
	test_comment_dir2 \
	test_tricky_missing_newline \
	test_irremovable_verilator_config \
	test_labels \
	test_dpi \
	test_argument \
	test_caliptra_exit0 \
	test_calipta_exit1 \
	test_caliptra_grep_default \
	test_caliptra_grep_1_at_once \
	test_caliptra_grep \
	test_disable_line_remover \
	test_param_replacer \
	test_remove_covergroup \
	test_remove_property \
	test_remove_sequence \
	test_obfuscate_short_in \
	test_obfuscate_short_in_multi_file

.PHONY: test test_dont_abort_after_err $(TEST_CASES)

test_dont_abort_after_err:
	@$(MAKE) -k test

test: $(TEST_CASES)

test_short_exit0:
	@./run_test short_exit0 checkexit0.sh ${INPUT_DIR}/short_in.sv

test_extern_inline:
	@./run_test extern_inline checkverilator_run_finish.sh ${INPUT_DIR}/extern_inline.sv --n-at-once 1 && \
	awk -F'\t' '/externInliner/ && $$1 > 1 {print "check on trace failed - externInliner should need only one pass"; exit(1)}' out/extern_inline/debug/trace

test_truncator:
	@./run_test truncator checkexit0.sh ${INPUT_DIR}/short_in.sv
	@awk -F'\t' '$$2=="fileTruncator" && $$4=="1"{found=1} END{if(!found){print "FAILED: fileTruncator pass not committed" > "/dev/stderr"; exit(1)}}' out/truncator/debug/trace

test_short_verilator_errmsg:
	@verilator --version >/dev/null || (printf "FAILED: verilator not found\n"; exit 1)
	@verilator --version | grep -q "5.016" || printf "NOTE: verilator version != 5.016. This may cause following test to fail\n"
	@./run_test short_verilator_errmsg checkverilator_errmsg_short.sh ${INPUT_DIR}/short_in.sv

test_short_multi_file_verilator_errmsg:
	@verilator --version >/dev/null || (printf "FAILED: verilator not found\n"; exit 1)
	@verilator --version | grep -q "5.016" || printf "NOTE: verilator version != 5.016. This may cause following test to fail\n"
	@./run_test short_multifile_verilator_errmsg checkverilator_errmsg_short.sh ${INPUT_DIR}/short_in/a.sv ${INPUT_DIR}/short_in/subdir/a.sv ${INPUT_DIR}/short_in/c.sv ${INPUT_DIR}/short_in/d.sv

test_short_multi_file_flag_y_verilator_errmsg:
	@verilator --version >/dev/null || (printf "FAILED: verilator not found\n"; exit 1)
	@verilator --version | grep -q "5.016" || printf "NOTE: verilator version != 5.016. This may cause following test to fail\n"
	@./run_test short_multifile_flag_y_verilator_errmsg checkverilator_errmsg_short.sh -y ${INPUT_DIR}/short_in -y ${INPUT_DIR}/short_in/subdir

test_short_multi_file_flag_f_verilator_errmsg:
	@verilator --version >/dev/null || (printf "FAILED: verilator not found\n"; exit 1)
	@verilator --version | grep -q "5.016" || printf "NOTE: verilator version != 5.016. This may cause following test to fail\n"
	@./run_test short_multifile_flag_f_verilator_errmsg checkverilator_errmsg_short.sh -f ${INPUT_DIR}/short_in/filelist.f

test_short_exit1:
	@./run_test short_exit1 checkexit1.sh ${INPUT_DIR}/short_in.sv

test_generate:
	@./run_test generate checkverilator_run_finish.sh ${INPUT_DIR}/generate.sv

test_if_body_replacer:
	@./run_test if_body_replacer check_if_body_replacer.sh ${INPUT_DIR}/if_body_replacer.sv && \
	awk -F'\t' '$$1 ~ /^[0-9]+$$/ && $$1 > 3 {print "check on trace failed - ifBodyReplacer should need at most three passes"; exit(1)}' out/if_body_replacer/debug/trace

test_short_grep:
	@./run_test short_grep checkgrep.sh ${INPUT_DIR}/short_in.sv

test_empty:
	@timeout 15s ./run_test empty checkexit0.sh ${INPUT_DIR}/short_in/empty.sv

test_comment_dir:
	@./run_test comment_dir checkverilator_run_finish.sh ${INPUT_DIR}/comment_dir_in.sv

test_comment_dir2:
	@./run_test comment_dir2 checkverilator_run_finish.sh ${INPUT_DIR}/comment_dir_in2.sv

test_tricky_missing_newline:
	@./run_test tricky_missing_newline checkgrep.sh input_files/tricky_missing_newline.sv

test_irremovable_verilator_config:
	@./run_test test_irremovable_verilator_config checkgrep.sh input_files/irremovable_cfg.sv

test_labels:
	@./run_test labels checkverilator_run_finish.sh ${INPUT_DIR}/labels.sv

test_dpi:
	@./run_test dpi checkverilator_lint.sh ${INPUT_DIR}/dpi.sv

test_argument:
	@./run_test argument checkverilator_run_finish.sh ${INPUT_DIR}/argument.sv && \
	awk -F'\t' '/functionArgRemover/ && $$1 > 1 {print "check on trace failed - functionArgRemover should need only one pass"; exit(1)}' out/argument/debug/trace

test_caliptra_exit0:
	@./run_test caliptra_exit0 checkexit0.sh ${INPUT_DIR}/caliptra_in.sv

test_calipta_exit1:
	@./run_test caliptra_exit1 checkexit1.sh ${INPUT_DIR}/caliptra_in.sv

test_caliptra_grep_default:
	@./run_test caliptra_grep_default checkgrep.sh ${INPUT_DIR}/caliptra_in.sv

test_caliptra_grep_1_at_once:
	@./run_test caliptra_grep_1_at_once checkgrep.sh ${INPUT_DIR}/caliptra_in.sv --n-at-once 1

test_caliptra_grep: test_caliptra_grep_default test_caliptra_grep_1_at_once
	@# check if speculative reduction (used by default) lead to significant improvement
	@# in terms of number of check script runs
	@printf "TEST: caliptra_grep (speculative vs 1-at-once perf)\n"
	@wc -l out/caliptra_grep_default/debug/trace out/caliptra_grep_1_at_once/debug/trace | gawk '\
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

test_disable_line_remover:
	@./run_test disable_line_remover checkgrep.sh ${INPUT_DIR}/disable_line_remover.sv --fno-line-remover

test_param_replacer:
	@./run_test param check_param_replacer.sh ${INPUT_DIR}/param.sv

test_remove_covergroup:
	@./run_test remove_covergroup checkgrepfinish.sh ${INPUT_DIR}/remove_covergroup.sv

test_remove_property:
	@./run_test remove_property checkgrepfinish.sh ${INPUT_DIR}/remove_property.sv

test_remove_sequence:
	@./run_test remove_sequence checkgrepfinish.sh ${INPUT_DIR}/remove_sequence.sv

test_obfuscate_short_in:
	@./run_test_obfuscate obfuscate_short_in ${INPUT_DIR}/short_in.sv

test_obfuscate_short_in_multi_file:
	@./run_test_obfuscate obfuscate_short_in_multi_file ${INPUT_DIR}/short_in/a.sv ${INPUT_DIR}/short_in/subdir/a.sv ${INPUT_DIR}/short_in/c.sv ${INPUT_DIR}/short_in/d.sv
