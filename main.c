#define _POSIX_C_SOURCE 200809L

#include "analyzers/plugin_api.h"

#include <dirent.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define PLUGIN_DIRECTORY "./plugins"

#define COLOR_CYAN "\033[36m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_RESET "\033[0m"

typedef struct {
    char *path;
    void *handle;
    plugin_name_fn name;
    plugin_extension_fn extension;
    plugin_info_fn info;
    plugin_analyze_fn analyze;
} Plugin;

typedef struct {
    Plugin *items;
    size_t count;
    size_t capacity;
} PluginRegistry;

static int use_color;
static int interactive_terminal;

/* Clear an interactive terminal without invoking an external command. */
/* Limpia la pantalla si estamos en una terminal interactiva. */
static void clear_screen(void)
{
    if (interactive_terminal) {
        fputs("\033[2J\033[H", stdout);
        fflush(stdout);
    }
}

/* Return true for a shared-library filename. */
/* Revisa si el nombre termina en .so. */
static int is_shared_library(const char *name)
{
    size_t length = strlen(name);
    return length > 3 && strcmp(name + length - 3, ".so") == 0;
}

/* Find an already loaded library by its path. */
/* Busca si esa ruta ya está cargada. */
static int has_path(const PluginRegistry *registry, const char *path)
{
    for (size_t index = 0; index < registry->count; index++) {
        if (strcmp(registry->items[index].path, path) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Find the first plugin supporting an extension. */
/* Busca el primer plugin que acepta esa extensión. */
static Plugin *find_extension(PluginRegistry *registry, const char *extension)
{
    for (size_t index = 0; index < registry->count; index++) {
        if (strcasecmp(registry->items[index].extension(), extension) == 0) {
            return &registry->items[index];
        }
    }
    return NULL;
}

/* Load a library and retain its functions until program exit. */
/* Carga un plugin y guarda sus funciones para usarlas después. */
static int load_plugin(PluginRegistry *registry, char *path)
{
    _Static_assert(sizeof(plugin_name_fn) == sizeof(void *),
                   "Plugin function pointers must match dlsym pointers");
    _Static_assert(sizeof(plugin_extension_fn) == sizeof(void *),
                   "Plugin function pointers must match dlsym pointers");
    _Static_assert(sizeof(plugin_info_fn) == sizeof(void *),
                   "Plugin function pointers must match dlsym pointers");
    _Static_assert(sizeof(plugin_analyze_fn) == sizeof(void *),
                   "Plugin function pointers must match dlsym pointers");

    void *handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        fprintf(stderr, "Aviso: no se pudo cargar el complemento %s.\n", path);
        return 0;
    }

    Plugin plugin = {0};
    plugin.path = path;
    plugin.handle = handle;
    void *symbol = dlsym(handle, "plugin_name");
    memcpy(&plugin.name, &symbol, sizeof(plugin.name));
    symbol = dlsym(handle, "plugin_extension");
    memcpy(&plugin.extension, &symbol, sizeof(plugin.extension));
    symbol = dlsym(handle, "info");
    memcpy(&plugin.info, &symbol, sizeof(plugin.info));
    symbol = dlsym(handle, "analyze");
    memcpy(&plugin.analyze, &symbol, sizeof(plugin.analyze));

    if (!plugin.name || !plugin.extension || !plugin.info || !plugin.analyze ||
        !plugin.name() || !*plugin.name() || !plugin.info() ||
        !plugin.extension() || plugin.extension()[0] != '.' ||
        !plugin.extension()[1]) {
        fprintf(stderr, "Aviso: el complemento %s tiene una interfaz inválida.\n",
                path);
        dlclose(handle);
        return 0;
    }

    if (find_extension(registry, plugin.extension())) {
        fprintf(stderr, "Aviso: ya existe un analizador para %s; se omitió %s.\n",
                plugin.extension(), path);
        dlclose(handle);
        return 0;
    }

    if (registry->count == registry->capacity) {
        size_t capacity = registry->capacity ? registry->capacity * 2 : 4;
        Plugin *resized = realloc(registry->items, capacity * sizeof(*resized));
        if (!resized) {
            fputs("Error: memoria insuficiente para cargar complementos.\n", stderr);
            dlclose(handle);
            return -1;
        }
        registry->items = resized;
        registry->capacity = capacity;
    }

    registry->items[registry->count++] = plugin;
    return 1;
}

/* Scan the directory for libraries that have not been loaded yet. */
/* Revisa la carpeta y carga los plugins nuevos. */
static int reload_plugins(PluginRegistry *registry)
{
    struct dirent **entries = NULL;
    int entry_count = scandir(PLUGIN_DIRECTORY, &entries, NULL, alphasort);
    if (entry_count < 0) {
        fprintf(stderr, "Error: no se pudo abrir el directorio %s.\n",
                PLUGIN_DIRECTORY);
        return -1;
    }

    int loaded = 0;
    int failed = 0;
    for (int index = 0; index < entry_count; index++) {
        const char *name = entries[index]->d_name;
        if (is_shared_library(name)) {
            size_t length = strlen(PLUGIN_DIRECTORY) + 1 + strlen(name) + 1;
            char *path = malloc(length);
            if (!path) {
                fputs("Error: memoria insuficiente para buscar complementos.\n",
                      stderr);
                failed = 1;
            } else {
                snprintf(path, length, "%s/%s", PLUGIN_DIRECTORY, name);
                if (!has_path(registry, path)) {
                    int result = load_plugin(registry, path);
                    if (result > 0) {
                        loaded++;
                        path = NULL; /* The registry now owns this path.
                                      * La ruta ya queda a cargo del registro. */
                    } else if (result < 0) {
                        failed = 1;
                    }
                }
                free(path);
            }
        }
        free(entries[index]);
    }
    free(entries);

    printf("Complementos nuevos cargados: %d. Total disponible: %zu.\n",
           loaded, registry->count);
    return failed ? -1 : loaded;
}

/* Show the metadata exported by each loaded plugin. */
/* Muestra los formatos y datos de cada plugin cargado. */
static void list_plugins(const PluginRegistry *registry)
{
    if (registry->count == 0) {
        puts("No hay formatos disponibles. Use la opción de recargar complementos.");
        return;
    }
    puts("Formatos disponibles:");
    for (size_t index = 0; index < registry->count; index++) {
        const Plugin *plugin = &registry->items[index];
        printf("  %zu. %s%s%s (%s)\n     %s\n", index + 1,
               use_color ? COLOR_GREEN : "", plugin->name(),
               use_color ? COLOR_RESET : "", plugin->extension(), plugin->info());
    }
}

/* Read one menu or path input line, removing its line ending. */
/* Lee lo que escribe el usuario y quita el salto de línea. */
static char *read_input(const char *prompt)
{
    char *line = NULL;
    size_t capacity = 0;
    fputs(prompt, stdout);
    fflush(stdout);
    if (getline(&line, &capacity, stdin) < 0) {
        free(line);
        return NULL;
    }
    line[strcspn(line, "\r\n")] = '\0';
    return line;
}

/* Select an analyzer from the final extension in the file's basename. */
/* Elige el plugin según la extensión del archivo y lo ejecuta. */
static void analyze_file(PluginRegistry *registry)
{
    char *path = read_input("Ruta del archivo: ");
    if (!path) {
        puts("No se pudo leer la ruta del archivo.");
        return;
    }
    if (!*path) {
        puts("Ingrese una ruta de archivo.");
        free(path);
        return;
    }

    const char *basename = strrchr(path, '/');
    basename = basename ? basename + 1 : path;
    const char *extension = strrchr(basename, '.');
    Plugin *plugin = extension ? find_extension(registry, extension) : NULL;
    if (!plugin) {
        puts("No hay un analizador disponible para ese formato.");
    } else {
        clear_screen();
        printf("%sAnalizando con %s...%s\n", use_color ? COLOR_CYAN : "",
               plugin->name(), use_color ? COLOR_RESET : "");
        if (plugin->analyze(path, stdout) != 0) {
            puts("El análisis no se completó.");
        }
    }
    free(path);
}

/* Release every loaded library after its metadata is no longer needed. */
/* Cierra los plugins cargados y libera su memoria. */
static void free_plugins(PluginRegistry *registry)
{
    for (size_t index = 0; index < registry->count; index++) {
        dlclose(registry->items[index].handle);
        free(registry->items[index].path);
    }
    free(registry->items);
}

/* Run the menu until the user exits. */
/* Muestra el menú y atiende las opciones hasta salir. */
int main(void)
{
    PluginRegistry registry = {0};
    interactive_terminal = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
    use_color = isatty(STDOUT_FILENO) && getenv("NO_COLOR") == NULL;
    reload_plugins(&registry);

    for (;;) {
        printf("\n%s=== Analizador de archivos ===%s\n"
               "1. Ver formatos disponibles\n"
               "2. Recargar complementos\n"
               "3. Extraer información de un archivo\n"
               "4. Salir\n",
               use_color ? COLOR_CYAN : "", use_color ? COLOR_RESET : "");
        char *choice = read_input("Elija una opción: ");
        if (!choice) {
            puts("\nFin de la entrada. Saliendo.");
            break;
        }
        int wait_for_user = 0;
        if (strcmp(choice, "1") == 0) {
            clear_screen();
            list_plugins(&registry);
            wait_for_user = 1;
        } else if (strcmp(choice, "2") == 0) {
            clear_screen();
            reload_plugins(&registry);
            wait_for_user = 1;
        } else if (strcmp(choice, "3") == 0) {
            clear_screen();
            analyze_file(&registry);
            wait_for_user = 1;
        } else if (strcmp(choice, "4") == 0) {
            printf("%sHasta luego.%s\n", use_color ? COLOR_YELLOW : "",
                   use_color ? COLOR_RESET : "");
            free(choice);
            break;
        } else {
            puts("Opción inválida. Ingrese un número del 1 al 4.");
        }
        free(choice);
        if (wait_for_user && interactive_terminal) {
            char *continue_input = read_input("\nPresione Enter para volver al menú...");
            if (!continue_input) {
                puts("\nFin de la entrada. Saliendo.");
                break;
            }
            free(continue_input);
            clear_screen();
        }
    }

    free_plugins(&registry);
    return 0;
}
