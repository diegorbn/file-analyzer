#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

/* Run one scripted application execution; stdout is the measured resource
 * record. */
int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr,
                "usage: runner working-directory input stdout stderr\n");
        return 2;
    }
    int in = open(argv[2], O_RDONLY),
        out = open(argv[3], O_WRONLY | O_CREAT | O_TRUNC, 0600),
        err = open(argv[4], O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (in < 0 || out < 0 || err < 0) {
        perror("open");
        return 2;
    }
    struct timespec start, end;
    if (clock_gettime(CLOCK_MONOTONIC, &start))
        return 2;
    pid_t pid = fork();
    if (pid == 0) {
        if (chdir(argv[1]) || dup2(in, 0) < 0 || dup2(out, 1) < 0 ||
            dup2(err, 2) < 0)
            _exit(126);
        close(in);
        close(out);
        close(err);
        execl("./file-analyzer", "file-analyzer", (char *)NULL);
        _exit(127);
    }
    if (pid < 0) {
        perror("fork");
        return 2;
    }
    close(in);
    close(out);
    close(err);
    int status;
    struct rusage usage;
    if (wait4(pid, &status, 0, &usage) < 0 ||
        clock_gettime(CLOCK_MONOTONIC, &end)) {
        perror("wait4");
        return 2;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
        fprintf(stderr, "child failed (status %d)\n", status);
        return 1;
    }
    printf("%.6f,%ld\n",
           (end.tv_sec - start.tv_sec) * 1000.0 +
               (end.tv_nsec - start.tv_nsec) / 1000000.0,
           usage.ru_maxrss);
    return 0;
}
