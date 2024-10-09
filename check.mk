SHELL := /bin/bash

.PHONY: test_and_lint_dont_abort_after_err
test_and_lint_dont_abort_after_err:
	@$(MAKE) -f check.mk -k test_and_lint

.PHONY: test_and_lint
test_and_lint: test lint

.PHONY: autofix
autofix: autofmt shellcheck_autofix

.PHONY: autofmt
autofmt:
	clang-format-14 -i source/*.cpp source/*.hpp

.PHONY: test
test:
	@$(MAKE) -C tests

.PHONY: lint
lint: fmt_check shellcheck

.PHONY: fmt_check
fmt_check:
	FMT_FAILED=0; \
	for i in source/*.cpp source/*.hpp; do \
	  diff "$$i" <(clang-format-14 "$$i")  --label "original $$i" --label "formatted $$i" --color=always -u || FMT_FAILED=1; \
	done; \
	exit "$$FMT_FAILED"
	@echo # blank line for consistency


SCRIPTS=scripts/* tests/*.sh tests/run_test \
	examples/caliptra_vcd/sv-bugpoint-check.sh \
	examples/caliptra_verilation_err/sv-bugpoint-check.sh

.PHONY: shellcheck
shellcheck:
	shellcheck $(SCRIPTS) --color

.PHONY: shellcheck
shellcheck_autofix:
	shellcheck $(SCRIPTS) -f diff --color || exit 0
	shellcheck $(SCRIPTS) -f diff | git apply --allow-empty
