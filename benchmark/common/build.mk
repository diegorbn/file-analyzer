CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O0
TYPES ?= txt csv
ANALYZER_OBJECTS = $(addprefix build/,$(addsuffix .o,$(TYPES)))
PLUGIN_FILES = $(addprefix plugins-built/,$(addsuffix _plugin.so,$(TYPES)))

.PHONY: all static dynamic plugins clean clean-plugins rebuild-plugins
all: static dynamic plugins
static: bin/$(APP)-static
dynamic: bin/$(APP)-dynamic plugins
plugins: $(PLUGIN_FILES)

build/%.o: analyzers/%_plugin.c analyzers/plugin_api.h common/build.mk Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -fPIC -Dplugin_name=$*_plugin_name -Dplugin_extension=$*_plugin_extension -Dinfo=$*_info -Danalyze=$*_analyze -c $< -o $@

plugins-built/%_plugin.so: analyzers/%_plugin.c analyzers/plugin_api.h common/build.mk Makefile
	@mkdir -p $(@D) plugins-active
	$(CC) $(CFLAGS) -fPIC -shared $< -o $@

bin/$(APP)-static: src/main.c common/registry.c common/registry.h common/build.mk Makefile $(ANALYZER_OBJECTS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(REGISTRY_FLAGS) src/main.c common/registry.c $(ANALYZER_OBJECTS) -o $@

bin/$(APP)-dynamic: src/main.c common/registry.c common/registry.h common/build.mk Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(REGISTRY_FLAGS) -DDYNAMIC src/main.c common/registry.c -ldl -o $@

clean-plugins:
	rm -f $(PLUGIN_FILES)

# Force rebuilding without racing a parallel clean against compilation.
rebuild-plugins:
	$(MAKE) -B plugins

clean:
	rm -rf build bin
	rm -f $(PLUGIN_FILES)
