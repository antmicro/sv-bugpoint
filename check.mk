SHELL := /bin/bash

.PHONY: test_and_lint_dont_abort_after_err
test_and_lint_dont_abort_after_err:
	@$(MAKE) -f check.mk -k test_and_lint

.PHONY: test_and_lint
test_and_lint: test lint

.PHONY: autofmt
autofmt:
	clang-format-14 -i source/*.cpp source/*.hpp

.PHONY: test
test:
	@$(MAKE) -C tests

.PHONY: lint
lint: fmt_check

.PHONY: fmt_check
fmt_check:
	for i in source/*.cpp source/*.hpp; do \
	  diff "$$i" <(clang-format-14 "$$i")  --label "original $$i" --label "formatted $$i" --color=always -u; \
	done
	@echo # blank line for consistency
