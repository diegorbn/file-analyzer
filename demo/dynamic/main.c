#define DYNAMIC
#define _POSIX_C_SOURCE 200809L
#ifndef REGISTRY_H
#define REGISTRY_H

#include "plugin_api.h"

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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#ifdef DYNAMIC
#include <dlfcn.h>
#include <dirent.h>
#include <limits.h>
#else
#define DECLARE(x) \
    const char *x##_plugin_name(void); \
    const char *x##_plugin_extension(void); \
    const char *x##_info(void); \
    int x##_analyze(const char *, FILE *);
DECLARE(txt)
DECLARE(csv)
#ifdef WITH_LOG
DECLARE(log)
#endif
#endif

Plugin *find_plugin(Registry *registry, const char *extension)
{
    for (int i = 0; i < registry->count; i++) {
        if (!strcasecmp(registry->items[i].extension(), extension))
            return &registry->items[i];
    }
    return NULL;
}

int process_file(Plugin *plugin, const char *path, FILE *output)
{
    if (!plugin || !path || !output)
        return -1;
    return plugin->analyze(path, output);
}

#ifdef DYNAMIC
static int load_path(Registry *registry, const char *path)
{
    for (int i = 0; i < registry->count; i++) {
        if (!strcmp(registry->items[i].path, path))
            return 0;
    }
    if (registry->count == 64) {
        fprintf(stderr, "Registro de analizadores lleno\n");
        return -1;
    }

    void *handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        fprintf(stderr, "No se pudo cargar %s: %s\n", path, dlerror());
        return -1;
    }
    Plugin plugin = {0};
    plugin.handle = handle;
    _Static_assert(sizeof(void *) == sizeof(plugin_name_fn), "dlsym pointer size");
    /* POSIX permits dlsym's result to represent a function pointer. */
#define RESOLVE(member, symbol_name) do { \
    dlerror(); \
    void *symbol = dlsym(handle, symbol_name); \
    const char *error = dlerror(); \
    if (error || !symbol) { \
        fprintf(stderr, "Complemento inválido %s: falta %s\n", path, symbol_name); \
        dlclose(handle); \
        return -1; \
    } \
    memcpy(&plugin.member, &symbol, sizeof(plugin.member)); \
} while (0)
    RESOLVE(name, "plugin_name");
    RESOLVE(extension, "plugin_extension");
    RESOLVE(info, "info");
    RESOLVE(analyze, "analyze");
#undef RESOLVE
    if (!plugin.name() || !*plugin.name() || !plugin.info() ||
        !plugin.extension() || plugin.extension()[0] != '.' ||
        !plugin.extension()[1]) {
        fprintf(stderr, "Metadatos del complemento inválidos: %s\n", path);
        dlclose(handle);
        return -1;
    }
    if (find_plugin(registry, plugin.extension())) {
        fprintf(stderr, "Extensión duplicada: %s\n", plugin.extension());
        dlclose(handle);
        return -1;
    }
    plugin.path = strdup(path);
    if (!plugin.path) {
        dlclose(handle);
        return -1;
    }
    registry->items[registry->count++] = plugin;
    return 1;
}

int acquire(Registry *registry, const char *extension, const char *directory)
{
    if (find_plugin(registry, extension))
        return 0;
    const char *name = !strcmp(extension, ".txt") ? "txt" :
                       !strcmp(extension, ".csv") ? "csv" :
                       !strcmp(extension, ".log") ? "log" : NULL;
    if (!name)
        return -1;
    char path[PATH_MAX];
    if (snprintf(path, sizeof(path), "%s/%s_plugin.so", directory, name) >= (int)sizeof(path))
        return -1;
    if (load_path(registry, path) < 0)
        return -1;
    return find_plugin(registry, extension) ? 0 : -1;
}

int refresh(Registry *registry, const char *directory)
{
    struct dirent **entries = NULL;
    int count = scandir(directory, &entries, NULL, alphasort);
    int errors = 0;
    if (count < 0) {
        perror(directory);
        return -1;
    }
    for (int i = 0; i < count; i++) {
        size_t length = strlen(entries[i]->d_name);
        if (length > 3 && !strcmp(entries[i]->d_name + length - 3, ".so")) {
            char path[PATH_MAX];
            if (snprintf(path, sizeof(path), "%s/%s", directory, entries[i]->d_name) >= (int)sizeof(path) ||
                load_path(registry, path) < 0)
                errors++;
        }
        free(entries[i]);
    }
    free(entries);
    return errors ? -1 : 0;
}
#else
int acquire(Registry *registry, const char *extension, const char *directory)
{
    (void)directory;
    if (find_plugin(registry, extension))
        return 0;
    Plugin plugin = {0};
#define CHOOSE(x) if (!strcmp(extension, "." #x)) { \
    plugin.name = x##_plugin_name; plugin.extension = x##_plugin_extension; \
    plugin.info = x##_info; plugin.analyze = x##_analyze; \
}
    CHOOSE(txt)
    else CHOOSE(csv)
#ifdef WITH_LOG
    else CHOOSE(log)
#endif
    else return -1;
#undef CHOOSE
    if (registry->count == 64)
        return -1;
    registry->items[registry->count++] = plugin;
    return 0;
}

int refresh(Registry *registry, const char *directory)
{
    if (acquire(registry, ".txt", directory) || acquire(registry, ".csv", directory))
        return -1;
#ifdef WITH_LOG
    if (acquire(registry, ".log", directory))
        return -1;
#endif
    return 0;
}
#endif

int unload(Registry *registry, const char *extension)
{
    for (int i = 0; i < registry->count; i++) {
        if (strcasecmp(registry->items[i].extension(), extension))
            continue;
#ifdef DYNAMIC
        void *handle = registry->items[i].handle;
        free(registry->items[i].path);
        registry->items[i] = registry->items[--registry->count];
        if (dlclose(handle)) {
            fprintf(stderr, "dlclose: %s\n", dlerror());
            return -1;
        }
        return 0;
#else
        fprintf(stderr, "Los analizadores estáticos no se pueden descargar.\n");
        return -1;
#endif
    }
    fprintf(stderr, "Analizador no cargado: %s\n", extension);
    return -1;
}

void cleanup(Registry *registry)
{
#ifdef DYNAMIC
    while (registry->count) {
        Plugin plugin = registry->items[--registry->count];
        free(plugin.path);
        if (dlclose(plugin.handle))
            fprintf(stderr, "Error al descargar la biblioteca (dlclose): %s\n", dlerror());
    }
#else
    registry->count = 0;
#endif
}


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void analyze_file(Registry *registry, const char *path)
{
    const char *base = strrchr(path, '/');
    const char *extension = strrchr(base ? base + 1 : path, '.');
    Plugin *plugin = extension ? find_plugin(registry, extension) : NULL;
    if (!plugin)
        puts("No hay un analizador para esta extensión");
    else if (process_file(plugin, path, stdout))
        puts("Error al analizar el archivo");
}

int main(int argc, char **argv)
{
    const char *directory = argc > 1 ? argv[1] : "./plugins";
    Registry registry = {0};
    char *line = NULL;
    size_t capacity = 0;
    refresh(&registry, directory);
    puts("Analizador de archivos: list, refresh, analyze RUTA, unload .EXT, quit");

    while (1) {
        printf("> ");
        fflush(stdout);
        if (getline(&line, &capacity, stdin) < 0)
            break;
        line[strcspn(line, "\r\n")] = 0;
        if (!strcmp(line, "quit"))
            break;
        if (!strcmp(line, "list")) {
            if (!registry.count)
                puts("No hay analizadores cargados");
            for (int i = 0; i < registry.count; i++)
                printf("%s %s: %s\n", registry.items[i].extension(),
                       registry.items[i].name(), registry.items[i].info());
        } else if (!strcmp(line, "refresh")) {
            refresh(&registry, directory);
            printf("Analizadores cargados: %d\n", registry.count);
        } else if (!strncmp(line, "unload ", 7)) {
            unload(&registry, line + 7);
        } else if (!strncmp(line, "analyze ", 8)) {
            analyze_file(&registry, line + 8);
        } else {
            puts("Comandos: list, refresh, analyze RUTA, unload .EXT, quit");
        }
    }
    free(line);
    cleanup(&registry);
    return 0;
}
