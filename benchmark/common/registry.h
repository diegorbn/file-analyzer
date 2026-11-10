#ifndef REGISTRY_H
#define REGISTRY_H

#include "../analyzers/plugin_api.h"

typedef struct {
    char *path;
    void *handle;
    plugin_name_fn name;
    plugin_extension_fn extension;
    plugin_info_fn info;
    plugin_analyze_fn analyze;
} Plugin;

typedef struct {
    Plugin items[64];
    int count;
} Registry;

int acquire(Registry *registry, const char *extension, const char *directory);
int refresh(Registry *registry, const char *directory);
int unload(Registry *registry, const char *extension);
Plugin *find_plugin(Registry *registry, const char *extension);
int process_file(Plugin *plugin, const char *path, FILE *output);
void cleanup(Registry *registry);

#endif
