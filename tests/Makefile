.PHONY: test_dont_abort_after_err
test_dont_abort_after_err:
	@$(MAKE) -k test

.PHONY: test
test: test_short test_caliptra

.PHONY: test_short
test_short: test_short_exit0 test_short_exit1 test_short_grep test_short_verilator_errmsg

.PHONY: test_short_exit0
test_short_exit0:
	@./run_test short_exit0 checkexit0.sh short_in.sv

.PHONY: test_short_verilator_errmsg
test_short_verilator_errmsg:
	@verilator --version >/dev/null || (printf "FAILED: verilator not found\n"; exit 1)
	@verilator --version | grep -q "5.016" || printf "NOTE: verilator version != 5.016. This may cause following test to fail\n"
	@./run_test short_verilator_errmsg checkverilator_errmsg_short.sh short_in.sv

.PHONY: test_short_exit1
test_short_exit1:
	@./run_test short_exit1 checkexit1.sh short_in.sv

.PHONY: test_short_grep
test_short_grep:
	@./run_test short_grep checkgrep.sh short_in.sv

.PHONY: test_caliptra
test_caliptra: test_caliptra_exit0 test_caliptra_grep

.PHONY: test_caliptra_exit0
test_caliptra_exit0:
	@./run_test caliptra_exit0 checkexit0.sh caliptra_in.sv

# we don't test caliptra_exit1 by default as it would take very long
.PHONY: test_caliptra_exit1
test_calipta_exit1:
	@./run_test caliptra_exit1 checkexit1.sh caliptra_in.sv

.PHONY: test_caliptra_grep
test_caliptra_grep:
	@./run_test caliptra_grep checkgrep.sh caliptra_in.sv
