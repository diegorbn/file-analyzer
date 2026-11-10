CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2
TYPES = txt csv log
.PHONY: all static dynamic clean generate test run
all: static dynamic build/runner
static: build/static/file-analyzer
dynamic: build/dynamic/file-analyzer $(addprefix build/dynamic/plugins/,$(addsuffix _plugin.so,$(TYPES)))
build/static/%.o: ../analyzers/%_plugin.c ../analyzers/plugin_api.h Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -fPIC -Dplugin_name=$*_plugin_name -Dplugin_extension=$*_plugin_extension -Dinfo=$*_info -Danalyze=$*_analyze -c $< -o $@
build/static/main.o: static_app/main.c static_app/analyzers/plugin_api.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@
build/static/file-analyzer: build/static/main.o $(addprefix build/static/,$(addsuffix .o,$(TYPES)))
	$(CC) $(CFLAGS) $^ -o $@
build/dynamic/file-analyzer: ../main.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -ldl -o $@
build/dynamic/plugins/%_plugin.so: ../analyzers/%_plugin.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -fPIC -shared $< -o $@
build/runner: runner/runner.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -lm -o $@
generate:
	python3 tools/generate.py
test: all generate
	python3 tools/test.py
run: all generate
	python3 tools/benchmark.py
clean:
	rm -rf build datasets results
