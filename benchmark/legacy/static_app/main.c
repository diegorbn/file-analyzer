#define _POSIX_C_SOURCE 200809L

#include "analyzers/plugin_api.h"

#include <dirent.h>

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
static void clear_screen(void) {
    if (interactive_terminal) {
        fputs("\033[2J\033[H", stdout);
        fflush(stdout);
    }
}

/* Find the first plugin supporting an extension. */
/* Busca el primer plugin que acepta esa extensión. */
static Plugin *find_extension(PluginRegistry *registry, const char *extension) {
    for (size_t index = 0; index < registry->count; index++) {
        if (strcasecmp(registry->items[index].extension(), extension) == 0) {
            return &registry->items[index];
        }
    }
    return NULL;
}

#define DECLARE(prefix)                                                        \
    const char *prefix##_plugin_name(void);                                    \
    const char *prefix##_plugin_extension(void);                               \
    const char *prefix##_info(void);                                           \
    int prefix##_analyze(const char *, FILE *);
DECLARE(txt)
DECLARE(csv)
DECLARE(log)

static int reload_plugins(PluginRegistry *registry) {
    if (registry->count) {
        puts("Los analizadores estáticos se fijan al compilar.");
        return 0;
    }
    static Plugin plugins[] = {{NULL, NULL, csv_plugin_name,
                                csv_plugin_extension, csv_info, csv_analyze},
                               {NULL, NULL, log_plugin_name,
                                log_plugin_extension, log_info, log_analyze},
                               {NULL, NULL, txt_plugin_name,
                                txt_plugin_extension, txt_info, txt_analyze}};
    registry->items = plugins;
    registry->count = sizeof(plugins) / sizeof(plugins[0]);
    return (int)registry->count;
}

/* Show the metadata exported by each loaded plugin. */
/* Muestra los formatos y datos de cada plugin cargado. */
static void list_plugins(const PluginRegistry *registry) {
    if (registry->count == 0) {
        puts("No hay formatos disponibles. Use la opción de recargar "
             "complementos.");
        return;
    }
    puts("Formatos disponibles:");
    for (size_t index = 0; index < registry->count; index++) {
        const Plugin *plugin = &registry->items[index];
        printf("  %zu. %s%s%s (%s)\n     %s\n", index + 1,
               use_color ? COLOR_GREEN : "", plugin->name(),
               use_color ? COLOR_RESET : "", plugin->extension(),
               plugin->info());
    }
}

/* Read one menu or path input line, removing its line ending. */
/* Lee lo que escribe el usuario y quita el salto de línea. */
static char *read_input(const char *prompt) {
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
static void analyze_file(PluginRegistry *registry) {
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
static void free_plugins(PluginRegistry *registry) { (void)registry; }

/* Run the menu until the user exits. */
/* Muestra el menú y atiende las opciones hasta salir. */
int main(void) {
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
            char *continue_input =
                read_input("\nPresione Enter para volver al menú...");
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
