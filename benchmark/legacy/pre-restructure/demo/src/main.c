#define _POSIX_C_SOURCE 200809L
#include "../../common/registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void analyze_file(Registry *registry, const char *path)
{
    const char *base = strrchr(path, '/');
    const char *extension = strrchr(base ? base + 1 : path, '.');
    Plugin *plugin = extension ? find_plugin(registry, extension) : NULL;
    if (!plugin)
        puts("No analyzer for this extension");
    else if (process_file(plugin, path, stdout))
        puts("Analysis failed");
}

int main(int argc, char **argv)
{
    const char *directory = argc > 1 ? argv[1] : "demo/plugins-active";
    Registry registry = {0};
    char *line = NULL;
    size_t capacity = 0;
    refresh(&registry, directory);
    puts("File Analyzer: list, refresh, analyze PATH, unload .EXT, quit");

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
                puts("No analyzers loaded");
            for (int i = 0; i < registry.count; i++)
                printf("%s %s: %s\n", registry.items[i].extension(),
                       registry.items[i].name(), registry.items[i].info());
        } else if (!strcmp(line, "refresh")) {
            refresh(&registry, directory);
            printf("Loaded analyzers: %d\n", registry.count);
        } else if (!strncmp(line, "unload ", 7)) {
            unload(&registry, line + 7);
        } else if (!strncmp(line, "analyze ", 8)) {
            analyze_file(&registry, line + 8);
        } else {
            puts("Commands: list, refresh, analyze PATH, unload .EXT, quit");
        }
    }
    free(line);
    cleanup(&registry);
    return 0;
}
