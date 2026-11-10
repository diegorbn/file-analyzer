#define _POSIX_C_SOURCE 200809L
#include "../common/registry.h"

#include <stdio.h>
#include <time.h>

static double now(void)
{
    struct timespec time;
    if (clock_gettime(CLOCK_MONOTONIC, &time))
        return -1;
    return time.tv_sec + time.tv_nsec / 1e9;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s DATASET_DIR PLUGIN_DIR\n", argv[0]);
        return 2;
    }
    Registry registry = {0};
    double activation[2], processing[2];
    const char *extensions[] = {"txt", "csv"};
    int failed = 0;

    for (int phase = 0; phase < 2; phase++) {
        char suffix[8];
        snprintf(suffix, sizeof(suffix), ".%s", extensions[phase]);
        double start = now();
        if (start < 0 || acquire(&registry, suffix, argv[2])) {
            failed = 1;
            break;
        }
        double end = now();
        if (end < 0) {
            failed = 1;
            break;
        }
        activation[phase] = (end - start) * 1e6;
        Plugin *plugin = find_plugin(&registry, suffix);
        start = now();
        if (start < 0) {
            failed = 1;
            break;
        }
        for (int i = 0; i < 10; i++) {
            char path[4096];
            if (snprintf(path, sizeof(path), "%s/%s/%05d.%s", argv[1],
                         extensions[phase], i, extensions[phase]) >= (int)sizeof(path) ||
                process_file(plugin, path, stdout)) {
                fprintf(stderr, "Analysis failed: %s %d\n", extensions[phase], i);
                failed = 1;
                break;
            }
        }
        end = now();
        if (end < 0)
            failed = 1;
        processing[phase] = (end - start) * 1000;
        if (failed)
            break;
    }
    cleanup(&registry);
    if (failed || ferror(stdout) || fflush(stdout))
        return 1;
    fprintf(stderr, "METRICS %.6f %.6f %.6f %.6f 20\n", activation[0],
            activation[1], processing[0], processing[1]);
    return 0;
}
