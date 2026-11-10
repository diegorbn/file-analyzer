APP = benchmark
TYPES = txt csv
include common/build.mk

all: bin/runner
bin/runner: runner/runner.c common/build.mk Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@
test: all
	python3 tests/test_project.py
