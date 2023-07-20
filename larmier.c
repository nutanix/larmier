/*
 * Copyright (c) 2019 Nutanix Inc. All rights reserved.
 *
 * Author: Felipe Franciosi <felipe@nutanix.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 only.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * TODO:
 *  Print injected error backtrace on leak detection.
 *  Support more than one error value per stub.
 *  Grow BCA dynamically.
 *  Allow BCA start point to be specified on command line.
 *  Investigate multi-threaded programs.
 */

#define _GNU_SOURCE
#include <assert.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "larmier.h"

#define VERSION "20190201.001"

#define EXIT_MASK_TEST      0x100
#define EXIT_MASK_SYSTEM    0x200
#define EXIT_MASK           (EXIT_MASK_TEST | EXIT_MASK_SYSTEM)

#define EXIT_ERR_ABNORMAL   0xFB
#define EXIT_ERR_FDLEAKS    0xFC
#define EXIT_ERR_LARMIER    0xFD
#define EXIT_ERR_VALGRIND   0xFE

#define READBUF_SIZE        4096    // Initial buffer for pipe reading

#define PERR(...) fprintf(stderr, __VA_ARGS__)
#define POUT(...) fprintf(stdout, __VA_ARGS__)

typedef struct bca_ctx {
    char *bca_name;
    bca_t *bca;
} bca_ctx_t;

typedef struct larmier_opts {
    char **valgrind_argv;
    char *stubsdir;
    char *stubslib;
    int debug;
} larmier_opts_t;

static inline int
setup_pipe(int pipefd)
{
    int err;

    // Redirect stdout and stderr to pipefd.
    err = dup2(pipefd, fileno(stdout));
    if (err != fileno(stdout)) {
        perror("dup2");
        return -1;
    }
    err = dup2(pipefd, fileno(stderr));
    if (err != fileno(stderr)) {
        perror("dup2");
        return -1;
    }

    // Close pipefd.
    (void)close(pipefd);

    return 0;
}

static void
exec_test(int pipefd, const char *bca_name, larmier_opts_t *larmier_opts)
{
    char *envp[3];
    int i = 1;
    int err;

    // Dup pipefd[1] to stdout/err and close both pipefds.
    err = setup_pipe(pipefd);
    if (err == -1) {
        return;
    }

    // Setup envp[0].
    err = asprintf(&envp[0], "%s=%s", LARMIER_BCA, bca_name);
    if (err == -1) {
        perror("asprintf");
        return;
    }

    // Setup envp[1].
    if (larmier_opts->stubslib != NULL) {
        assert(larmier_opts->stubsdir != NULL);

        err = asprintf(&envp[i++], "LD_PRELOAD=%s", larmier_opts->stubslib);
        if (err == -1) {
            perror("asprintf");
            free(envp[0]);
            return;
        }

        err = asprintf(&envp[i++], "LD_LIBRARY_PATH=%s",
                       larmier_opts->stubsdir);
        if (err == -1) {
            perror("asprintf");
            free(envp[1]);
            free(envp[0]);
            return;
        }
    }

    // Terminate envp.
    envp[i] = NULL;

    // Execute test.
    execve(larmier_opts->valgrind_argv[0], larmier_opts->valgrind_argv, envp);
    perror("execve");
}

static char *
readall(int pipefd)
{
    char buf_tmp[256];
    char *buf, *ptr;
    size_t buf_size;
    size_t bytes_read;
    size_t bytes_left;

    // Allocate initial buffer for reading the pipe's output.
    buf_size = READBUF_SIZE;
    buf = calloc(1, buf_size);
    if (buf == NULL) {
        perror("calloc");
        goto err;
    }

    // Read pipe into buf.
    bytes_left = buf_size;
    ptr = buf;
    while ((bytes_read = read(pipefd, ptr, bytes_left)) > 0) {
        // Adjust values for next iteration.
        bytes_left -= bytes_read;
        ptr += bytes_read;

        // Increase buffer size if output too long.
        if (bytes_left == 0) {
            char *new_buf;

            buf_size += READBUF_SIZE;
            new_buf = realloc(buf, buf_size);
            if (new_buf == NULL) {
                goto err;
            }
            buf = new_buf;

            (void)memset(ptr, 0, READBUF_SIZE);
            bytes_left = READBUF_SIZE;
        }
    }

    return buf;

err:
    // Drain pipe and return error or shortcounted buf.
    while (read(pipefd, buf_tmp, sizeof(buf_tmp)) > 0);
    return buf;
}

static int
has_fd_leaks(char *valgrind_buf)
{
    char *line, *eol;

    if (valgrind_buf == NULL) {
        return 0;
    }

    line = strstr(valgrind_buf, " Open file descriptor ");
    if (line != NULL) {
        // Check if this leak is "LastTest.log.tmp".
        eol = strchr(line, '\n');
        *eol = '\0';
        if (strstr(line, "Testing/Temporary/LastTest.log.tmp") == NULL) {
            // Definitely a leak.
            *eol = '\n';
            return 1;
        }

        // The leak was a ctest bug, ignore and check rest of buffer.
        *eol = '\n';
        return has_fd_leaks(eol+1);
    }

    return 0;
}

static inline void
vgbuf_dump(larmier_opts_t *larmier_opts, char *valgrind_buf)
{
    if (larmier_opts->debug < 2) {
        return;
    }

    POUT("********************************\n");
    POUT("Valgrind buf:\n");
    POUT("%s", valgrind_buf);
    POUT("********************************\n");
}

static inline void
bca_dump(larmier_opts_t *larmier_opts, bca_ctx_t *bca_ctx)
{
    int i;

    assert(larmier_opts != NULL);
    assert(bca_ctx != NULL);

    if (larmier_opts->debug < 3) {
        return;
    }

    POUT("********************************\n");
    POUT("Name: '%s'\n", bca_ctx->bca_name);
    POUT("Count: %hu\n", bca_ctx->bca->count);

    for (i = 0; i < bca_ctx->bca->count; i++) {
        POUT("%c", bca_ctx->bca->map[i] + '0');
    }
    POUT("\n");

    POUT("********************************\n");
}

static int
larmier_loop(bca_ctx_t *bca_ctx, larmier_opts_t *larmier_opts)
{
    int pipefd[2];
    int status;
    int i;
    int err;
    pid_t pid;
    char *valgrind_buf;

    assert(bca_ctx != NULL);
    assert(larmier_opts != NULL);
    assert(larmier_opts->valgrind_argv != NULL);

    // Create a pipe to communicate with valgrind et al.
    err = pipe(pipefd);
    if (err == -1) {
        perror("pipe");
        goto err;
    }

    // Spawn and execute valgrind with test program.
    pid = fork();
    switch (pid) {
    case -1:
        perror("fork");
        goto err;
    case 0:
        // Child doesn't need pipefd[0].
        (void)close(pipefd[0]);

        // Execute the test under valgrind.
        exec_test(pipefd[1], bca_ctx->bca_name, larmier_opts);
        exit(EXIT_ERR_LARMIER);
    }

    // Parent doesn't write into pipefd[1].
    (void)close(pipefd[1]);

    // Read from child's stdout/stderr into a buffer.
    valgrind_buf = readall(pipefd[0]);

    // Parent doesn't need pipefd[0] anymore either.
    (void)close(pipefd[0]);

    // Wait for valgrind to exit.
    (void)waitpid(pid, &status, 0);

    // Potentially exit early if valgrind encountered errors.
    err = EXIT_MASK_SYSTEM;
    if (WEXITSTATUS(status) == EXIT_ERR_LARMIER) {
        // Larmier failed.
        err |= EXIT_ERR_LARMIER;
        goto err_early;
    }
    if (!WIFEXITED(status)) {
        // Test terminated abnormally.
        err |= EXIT_ERR_ABNORMAL;
        goto err_early;
    }
    if (WEXITSTATUS(status) == EXIT_ERR_VALGRIND) {
        // Valgrind found leaks.
        err |= EXIT_ERR_VALGRIND;
        goto err_early;
    }
    if (has_fd_leaks(valgrind_buf)) {
        // Valgrind reported fd leaks.
        err |= EXIT_ERR_FDLEAKS;
        goto err_early;
    }

    // Maybe dump valgrind buffer.
    vgbuf_dump(larmier_opts, valgrind_buf);

    // Maybe dump bca.
    bca_dump(larmier_opts, bca_ctx);

    // Free buffer.
    free(valgrind_buf);

    // Flip last non-zero call.
    for (i = bca_ctx->bca->count - 1; i >= 0; i--) {
        if (bca_ctx->bca->map[i] == 0) {
            bca_ctx->bca->map[i] = 1;
            (void)memset(&bca_ctx->bca->map[i + 1], 0, BCA_MAP_LEN - i - 1);
            break;
        }
    }

    if (i < 0) {
        // Nothing else to flip, use actual exit status.
        return (EXIT_MASK_TEST | WEXITSTATUS(status));
    }

    // Reset counter.
    bca_ctx->bca->count = 0;

    return 0;

err:
    return (EXIT_MASK_SYSTEM | EXIT_ERR_LARMIER);

err_early:
    vgbuf_dump(larmier_opts, valgrind_buf);
    bca_dump(larmier_opts, bca_ctx);
    free(valgrind_buf);

    return err;
}

static inline void
bca_ctx_destroy(bca_ctx_t *bca_ctx)
{
    (void)munmap(bca_ctx->bca, LARMIER_LEN);
    (void)shm_unlink(bca_ctx->bca_name);
    free(bca_ctx->bca_name);
    free(bca_ctx);
}

static inline void *
bca_ctx_create(void)
{
    bca_ctx_t *bca_ctx;
    int bca_fd;
    int err;

    // Allocate context for branch control array.
    bca_ctx = calloc(1, sizeof(*bca_ctx));
    if (bca_ctx == NULL) {
        perror("calloc");
        return NULL;
    }

    // Define unique name for bca shm entry.
    err = asprintf(&bca_ctx->bca_name, "larmier_%u", getpid());
    if (err == -1) {
        perror("asprintf");
        goto err;
    }

    // Open an shm entry for bca.
    bca_fd = shm_open(bca_ctx->bca_name, O_CREAT | O_RDWR, 0666);
    if (bca_fd == -1) {
        perror("shm_open");
        goto err_free;
    }

    // Set initial bca size.
    (void)ftruncate(bca_fd, LARMIER_LEN);

    // Map and zero out bca area from fd.
    bca_ctx->bca = mmap(NULL, LARMIER_LEN, PROT_READ | PROT_WRITE,
                        MAP_SHARED, bca_fd, 0);
    if (bca_ctx->bca == MAP_FAILED) {
        perror("mmap");
        goto err_close;
    }
    (void)memset(bca_ctx->bca, 0, LARMIER_LEN);

    // Done.
    (void)close(bca_fd);
    return bca_ctx;

err_close:
    (void)close(bca_fd);
    (void)shm_unlink(bca_ctx->bca_name);

err_free:
    free(bca_ctx->bca_name);

err:
    free(bca_ctx);
    return NULL;
}

static int
larmier(larmier_opts_t *larmier_opts)
{
    bca_ctx_t *bca_ctx;
    int err;

    assert(larmier_opts != NULL);
    assert(larmier_opts->valgrind_argv != NULL);

    // Create branch control array context.
    bca_ctx = bca_ctx_create();
    if (bca_ctx == NULL) {
        return -1;
    }

    // Loop exploring branches.
    do {
        err = larmier_loop(bca_ctx, larmier_opts);
    } while ((err & EXIT_MASK) == 0);

    // Clean up.
    bca_ctx_destroy(bca_ctx);

    if (larmier_opts->debug > 0) {
        POUT("Larmier exit status: 0x%X\n", err);
    }

    return (err & ~EXIT_MASK);
}

static char *
which(const char *name)
{
    char *spath, *spath_iter;
    char *exe = NULL;

    assert(name != NULL);

    // Fetch PATH from environ.
    spath = getenv("PATH");
    if (spath == NULL) {
        goto out;
    }

    // Copy environ to our heap for mangling.
    spath_iter = spath = strdup(spath);
    if (spath_iter == NULL) {
        goto out;
    }
    // nb. the above keeps the copy in 'spath' for free()ing.

    // Iterate over PATH searching for the executable.
    while ((spath_iter = strtok(spath_iter, ":")) != NULL) {
        // Allocate an 'exe' string with a full path to the executable.
        if (asprintf(&exe, "%s/%s", spath_iter, name) == -1) {
            perror("asprintf");
            exe = NULL;
            goto out;
        }

        // Check if there's an executable valgrind at 'exe'.
        if (access(exe, X_OK) == 0) {
            // Found.
            goto out;
        }

        // Not found. Adjust variables for next iteration.
        free(exe);
        spath_iter = NULL;
    }

out:
    free(spath);
    return exe;
}

static char *
valgrind_get(const char *upath)
{
    // Check user-supplied path.
    if (upath != NULL) {
        if (access(upath, X_OK) == -1) {
            return NULL;
        }
        return strdup(upath);
    }

    // Lookup 'valgrind' in system path.
    return which("valgrind");
}

static void
help (char *argv0)
{
    assert(argv0 != NULL);

    PERR("LARMIER %s\n", VERSION);
    PERR("Usage: %s [ opts ] < cmd [ args ... ] >\n", argv0);
    PERR("   Valid opts:\n");
    PERR("       -h              Display this help and exit\n");
    PERR("       -d[d...]        Increase debug level\n");
    PERR("       -v <valgrind>   Path to valgrind (default: search $PATH)\n");
    PERR("       -l <stubs_lib>  Name of stubs shared library\n");
}

static void
valgrind_argv_dump(char **valgrind_argv) {
    char **tmp;

    if (valgrind_argv == NULL) {
        return;
    }

    for (tmp = valgrind_argv; *tmp != NULL; tmp++) {
        POUT("'%s'", *tmp);
        if (*(tmp+1) != NULL) {
            POUT(" ");
        }
    }
    POUT("\n");
}

static void
valgrind_argv_destroy(char **valgrind_argv) {
    char **tmp;

    if (valgrind_argv == NULL) {
        return;
    }

    for (tmp = valgrind_argv; *tmp != NULL; tmp++) {
        free(*tmp);
    }

    free(valgrind_argv);
}

static char **
valgrind_argv_setup(const char *valgrind, const char *stubslib,
                    int argc, char **argv)
{
    char **valgrind_argv;
    int valg_args;
    int i;

    assert(valgrind != NULL);
    assert(argc > 0);
    assert(argv != NULL);

#define VALG_ARGDUP(i, ...)                             \
    do {                                                \
        int err;                                        \
        err = asprintf(&valgrind_argv[i], __VA_ARGS__); \
        if (err == -1) {                                \
            perror("asprintf");                         \
            valgrind_argv[i] = NULL;                    \
            goto err;                                   \
        }                                               \
    } while (0)

#define VALG_ARGS 9
    // Determine exact number of valgrind args.
    valg_args = VALG_ARGS;
    if (stubslib == NULL) {
        valg_args--;
    }

    // Allocate new argv array for valgrind.
    valgrind_argv = calloc(1, sizeof(char *) * (valg_args + argc + 1));
    if (valgrind_argv == NULL) {
        perror("calloc");
        return NULL;
    }

    // Fill in argv array with valgrind-related entries.
    VALG_ARGDUP(0, "%s", valgrind);
    VALG_ARGDUP(1, "--track-fds=yes");
    VALG_ARGDUP(2, "--leak-check=full");
    VALG_ARGDUP(3, "--show-leak-kinds=all");
    VALG_ARGDUP(4, "--error-exitcode=%d", EXIT_ERR_VALGRIND);
    VALG_ARGDUP(5, "--suppressions=dlsym.supp");
    VALG_ARGDUP(6, "--track-origins=yes");
    VALG_ARGDUP(7, "--fair-sched=yes");
    if (stubslib != NULL) {
        VALG_ARGDUP(8, "--soname-synonyms=somalloc=%s", stubslib);
    }

    // Fill in argv array with test-related entries.
    for (i = 0; i < argc; i++) {
        VALG_ARGDUP(valg_args + i, "%s", argv[i]);
    }

#undef VALG_ARGS
#undef VALG_ARGDUP

    return valgrind_argv;

err:
    valgrind_argv_destroy(valgrind_argv);

    return NULL;
}

static void
larmier_opts_destroy(larmier_opts_t *larmier_opts)
{
    assert(larmier_opts != NULL);

    valgrind_argv_destroy(larmier_opts->valgrind_argv);

    free(larmier_opts->stubsdir);
    free(larmier_opts->stubslib);
    free(larmier_opts);
}

static larmier_opts_t *
larmier_opts_parse(int argc, char **argv)
{
    larmier_opts_t *larmier_opts;
    char *valgrind = NULL;
    char *stubslib = NULL;
    int opt;

    assert(argc > 0);
    assert(argv != NULL);

    // Allocate a set of opts.
    larmier_opts = calloc(1, sizeof(*larmier_opts));
    if (larmier_opts == NULL) {
        PERR("Error allocating memory for opts: %m");
        return NULL;
    }

#define PARSE_OPTS_S(name, desc)                            \
    do {                                                    \
        if (name != NULL) {                                 \
            PERR(desc " already specified (%s)\n", name);   \
            goto err;                                       \
        }                                                   \
        name = strdup(optarg);                              \
        if (name == NULL) {                                 \
            PERR("Unable to strdup " desc " (%m)\n");       \
            goto err;                                       \
        }                                                   \
    } while (0)

    // Parse arguments.
    while ((opt = getopt(argc, argv, "hdv:l:")) != -1) {
        switch (opt) {
        case 'v':
            PARSE_OPTS_S(valgrind, "valgrind path");
            break;
        case 'd':
            larmier_opts->debug++;
            break;
        case 'l':
            PARSE_OPTS_S(stubslib, "stubs library name");
            break;
        case 'h':
        default:
            help(argv[0]);
            goto err;
        }
    }

#undef PARSE_OPTS_S

    // Ensure we have a valid test program.
    if (argc <= optind) {
        help(argv[0]);
        goto err;
    }
    if (access(argv[optind], X_OK) == -1) {
        PERR("Invalid test program at '%s'\n", argv[optind]);
        goto err;
    }

    // Ensure we have a valid stubslib and annotate its directory.
    if (stubslib != NULL) {
        if (access(stubslib, R_OK) == -1) {
            PERR("Invalid stubs library at '%s'\n", stubslib);
            goto err;
        }

        larmier_opts->stubslib = strdup(stubslib);
        if (larmier_opts->stubslib == NULL) {
            PERR("Error duplicating stubslib '%s': %m", stubslib);
            goto err;
        }

        larmier_opts->stubsdir = strdup(dirname(stubslib));
        if (larmier_opts->stubsdir == NULL) {
            PERR("Error duplicating stubsdir '%s': %m", stubslib);
            goto err;
        }
    }

    // Ensure we have a valid valgrind.
    if (valgrind == NULL) {
        valgrind = valgrind_get(NULL);
        if (valgrind == NULL) {
            PERR("Unable to locate valgrind in $PATH\n");
            goto err;
        }
    }

    // Create an argv array for valgrind and test program.
    larmier_opts->valgrind_argv = valgrind_argv_setup(valgrind,
                                                      larmier_opts->stubslib,
                                                      argc-optind,
                                                      &argv[optind]);
    if (larmier_opts->valgrind_argv == NULL) {
        goto err;
    }

    // Maybe debug valgrind_argv.
    if (larmier_opts->debug > 0) {
        valgrind_argv_dump(larmier_opts->valgrind_argv);
    }

    // Release temporary resources.
    free(valgrind);
    free(stubslib);

    return larmier_opts;

err:
    free(valgrind);
    free(stubslib);
    free(larmier_opts->stubsdir);
    free(larmier_opts->stubslib);
    free(larmier_opts);

    return NULL;
}

int
main(int argc, char **argv)
{
    larmier_opts_t *larmier_opts;
    int ret = EXIT_SUCCESS;

    // Parse arguments.
    larmier_opts = larmier_opts_parse(argc, argv);
    if (larmier_opts == NULL) {
        return EXIT_FAILURE;
    }

    // Run tests under valgrind.
    if (larmier(larmier_opts) != 0) {
        goto err;
    }

out:
    larmier_opts_destroy(larmier_opts);

    return ret;

err:
    ret = EXIT_FAILURE;
    goto out;
}
