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
 */

#ifndef LARMIER_STUB_H
#define LARMIER_STUB_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef __USE_GNU
#define __USE_GNU
#endif

#ifndef UNW_LOCAL_ONLY
#define UNW_LOCAL_ONLY
#endif

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <libunwind.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "larmier.h"

#ifdef LARMIER_DEBUG

#include <stdio.h>

static void
print_trace(void)
{
    unw_context_t ctx;
    unw_cursor_t cur;
    unw_word_t ip;
    Dl_info di;

    unw_getcontext(&ctx);
    unw_init_local(&cur, &ctx);

    do {
        unw_get_reg(&cur, UNW_REG_IP, &ip);
        dladdr((void *)ip, &di);
        printf("ip = %lx (%s) (%s)\n", (long)ip, di.dli_fname, di.dli_sname);
    } while (unw_step(&cur) > 0);
}

#else

static void
print_trace(void) { }

#endif /* LARMIER_DEBUG */

static bool stub_off __attribute__((unused)) = false;
static bool in_dlsym __attribute__((unused)) = false;

#define PROG_NAME_MAX 256

static bool
dont_stub(void)
{
    static char exe[PROG_NAME_MAX + 1];
    unw_context_t ctx;
    unw_cursor_t cur;
    unw_word_t ip;
    Dl_info di;
    const char *ptr;
    char *ptr2;

    // If LARMIER_STUB is not set or set to zero, don't stub.
    ptr = getenv("LARMIER_STUB");
    if (ptr == NULL || strcmp(ptr, "0") == 0) {
        return true;
    }

    // Initialise libunwind context and cursor.
    unw_getcontext(&ctx);
    unw_init_local(&cur, &ctx);

    // Figure out the name of this executable.
    if (exe[0] == '\0') {
        // The below is probably better than keeping a copy of cur.
        while (unw_step(&cur)) {
            unw_get_reg(&cur, UNW_REG_IP, &ip);
            dladdr((void *)ip, &di);
        }

        // strcpy(exe, di.dli_fname);
        ptr = di.dli_fname;
        ptr2 = exe;
        do {
            *ptr2++ = *ptr++;
        } while (*ptr != '\0' && (ptr2 - exe < PROG_NAME_MAX));

        // This block moved up the cursor, so reset it.
        unw_init_local(&cur, &ctx);
    }

    // Determine whether we're coming from another library.
    unw_step(&cur); // skip current stack
    unw_step(&cur); // skip stub stack
    unw_get_reg(&cur, UNW_REG_IP, &ip);
    dladdr((void *)ip, &di);

    // strcmp(exe, di.dli_fname);
    ptr = di.dli_fname;
    ptr2 = exe;
    while (*ptr != '\0' && *ptr2 != '\0' && *ptr == *ptr2) {
        ptr++;
        ptr2++;
    }
    if (*ptr == '\0' && *ptr2 == '\0') {
        // We came from the main prog, so stub.
        return false;
    }

    return true;
}

static inline void *
larmier_get_bca(void)
{
    char *bca_name = getenv(LARMIER_BCA);
    bca_t *bca = MAP_FAILED;
    int bca_fd;

    if (bca_name != NULL) {
        bca_fd = shm_open(bca_name, O_RDWR, 0600);
        if (bca_fd < 0) {
            goto out;
        }

        bca = mmap(NULL, LARMIER_LEN, PROT_READ | PROT_WRITE,
                   MAP_SHARED, bca_fd, 0);
        (void)close(bca_fd);
    }

out:
    return bca;
}

static inline void
larmier_put_bca(bca_t *bca)
{
    if (bca == MAP_FAILED) {
        return;
    }

    (void)munmap(bca, LARMIER_LEN);
}

#define PP_NARGM(...) PP_NARG_(__VA_ARGS__, PP_RSEQ_NM())

#define PP_NARG(...) \
         PP_NARG_(__VA_ARGS__,PP_RSEQ_N())

#define PP_NARG_(...) \
         PP_ARG_N(__VA_ARGS__)

#define PP_ARG_N( \
          _1, _2, _3, _4, _5, _6, _7, _8, _9,_10, \
         _11,_12,_13,_14,_15,_16,_17,_18,_19,_20, \
         _21,_22,_23,_24,_25,_26,_27,_28,_29,_30, \
         _31,_32,_33,_34,_35,_36,_37,_38,_39,_40, \
         _41,_42,_43,_44,_45,_46,_47,_48,_49,_50, \
         _51,_52,_53,_54,_55,_56,_57,_58,_59,_60, \
         _61,_62,_63,N,...) N

#define PP_RSEQ_N() \
         31,31,30,30,                   \
         29,29,28,28,27,27,26,26,25,25, \
         24,24,23,23,22,22,21,21,20,20, \
         19,19,18,18,17,17,16,16,15,15, \
         14,14,13,13,12,12,11,11,10,10, \
         9,9,8,8,7,7,6,6,5,5, \
         4,4,3,3,2,2,1,1,0,0

#define PP_RSEQ_NM() \
         62,61,60,                      \
         59,58,57,56,55,54,53,52,51,50, \
         49,48,47,46,45,44,43,42,41,40, \
         39,38,37,36,35,34,33,32,31,30, \
         29,28,27,26,25,24,23,22,21,20, \
         19,18,17,16,15,14,13,12,11,10, \
         9,8,7,6,5,4,3,2,1,0

#define GET_1(X, ...) X
#define GET_2(_1, X, ...) X
#define GET_3(_1, _2, X, ...) X
#define GET_4(_1, _2, _3, X, ...) X
#define GET_5(_1, _2, _3, _4, X, ...) X

#define CONCAT(x, y) x ## _ ## y
#define EVAL(x, y) CONCAT(x, y)
#define GET_NTHM(...) EVAL(GET, PP_NARGM(__VA_ARGS__)) (__VA_ARGS__)

#define _LREP0tau void
#define _LREP0ta void
#define _LREP0a

#define _LREPtau(t, a) t a __attribute__((unused))
#define _LREPta(t, a) t a
#define _LREPa(t, a) a

#define _LEXP0(x, ...) _LREP0##x
#define _LEXP1(x, t, a, ...) _LREP##x(t, a)
#define _LEXP2(x, t, a, ...) _LREP##x(t, a), _LEXP1(x, __VA_ARGS__)
#define _LEXP3(x, t, a, ...) _LREP##x(t, a), _LEXP2(x, __VA_ARGS__)
#define _LEXP4(x, t, a, ...) _LREP##x(t, a), _LEXP3(x, __VA_ARGS__)

#define _LEXP(n, x, ...) _LEXP##n(x, __VA_ARGS__)

#define _LSDEFn(lib, n, type, name, ...)                        \
    static inline type                                          \
    lstub_##name(_LEXP(n, ta, __VA_ARGS__));                    \
                                                                \
    __attribute__ ((visibility ("default"))) type               \
    name(_LEXP(n, ta, __VA_ARGS__))                             \
    {                                                           \
        bca_t *bca;                                             \
        bool stub_off_old = stub_off;                           \
        type ret;                                               \
        void *libp;                                             \
        type (*func)();                                         \
        stub_off = true;                                        \
        libp = dlopen(lib, RTLD_GLOBAL | RTLD_LAZY);            \
        func = dlsym(libp, #name);                              \
        dlclose(libp);                                          \
        stub_off = stub_off_old;                                \
        if (stub_off || dont_stub()) {                          \
            return func(_LEXP(n, a, __VA_ARGS__));              \
        }                                                       \
        stub_off = true;                                        \
        bca = larmier_get_bca();                                \
        if (bca != MAP_FAILED) {                                \
            if (bca->map[bca->count++] == 0) {                  \
                print_trace();                                  \
                ret = lstub_##name(_LEXP(n, a, __VA_ARGS__));   \
                goto out;                                       \
            }                                                   \
        }                                                       \
        ret = func(_LEXP(n, a, __VA_ARGS__));                   \
    out:                                                        \
        larmier_put_bca(bca);                                   \
        stub_off = stub_off_old;                                \
        return ret;                                             \
    }                                                           \
                                                                \
    static inline type                                          \
    lstub_##name(_LEXP(n, tau, __VA_ARGS__))

#define LSDEF_calloc()                                          \
    static inline void *                                        \
    lstub_calloc(size_t nmemb, size_t size);                    \
                                                                \
    __attribute__ ((visibility ("default"))) void *             \
    calloc(size_t nmemb, size_t size)                           \
    {                                                           \
        bca_t *bca;                                             \
        void *(*func)();                                        \
        void *ret;                                              \
        if (in_dlsym) {                                         \
            /* Special case: dlsym() calls calloc() */          \
            /* It would loop, but it copes with ENOMEM. */      \
            errno = ENOMEM;                                     \
            return NULL;                                        \
        }                                                       \
        in_dlsym = true;                                        \
        func = dlsym(RTLD_NEXT, "calloc");                      \
        in_dlsym = false;                                       \
        bca = larmier_get_bca();                                \
        if (!stub_off && !dont_stub() && bca != MAP_FAILED) {   \
            if (bca->map[bca->count++] == 0) {                  \
                print_trace();                                  \
                ret = lstub_calloc(nmemb, size);                \
                goto out;                                       \
            }                                                   \
        }                                                       \
        ret = func(nmemb, size);                                \
    out:                                                        \
        larmier_put_bca(bca);                                   \
        return ret;                                             \
    }                                                           \
                                                                \
    static inline void *                                        \
    lstub_calloc(size_t nmemb, size_t size)

#define _LSDEFv(n, type, name, vname, ...)                      \
    static inline type                                          \
    lstub_##name(_LEXP(n, ta, __VA_ARGS__), ...);               \
                                                                \
    __attribute__ ((visibility ("default"))) type               \
    name(_LEXP(n, ta, __VA_ARGS__), ...)                        \
    {                                                           \
        bca_t *bca;                                             \
        va_list ap;                                             \
        type (*func)() = dlsym(RTLD_NEXT, #vname);              \
        type ret;                                               \
        bool stub_off_old = stub_off;                           \
        va_start(ap, GET_NTHM(__VA_ARGS__));                    \
        if (stub_off || dont_stub()) {                          \
            ret = func(_LEXP(n, a, __VA_ARGS__), ap);           \
            va_end(ap);                                         \
            return ret;                                         \
        }                                                       \
        stub_off = true;                                        \
        bca = larmier_get_bca();                                \
        if (bca != MAP_FAILED) {                                \
            if (bca->map[bca->count++] == 0) {                  \
                print_trace();                                  \
                ret = lstub_##name(_LEXP(n, a, __VA_ARGS__), ap);\
                goto out;                                       \
            }                                                   \
        }                                                       \
        ret = func(_LEXP(n, a, __VA_ARGS__), ap);               \
    out:                                                        \
        va_end(ap);                                             \
        larmier_put_bca(bca);                                   \
        stub_off = stub_off_old;                                \
        return ret;                                             \
    }                                                           \
                                                                \
    static inline type                                          \
    lstub_##name(_LEXP(n, tau, __VA_ARGS__), ...)

#define LSDEF(type, name, ...)  _LSDEFn("libc.so.6", PP_NARG(__VA_ARGS__), type, name, __VA_ARGS__)
#define LSDEFlib(lib, type, name, ...)  _LSDEFn(lib, PP_NARG(__VA_ARGS__), type, name, __VA_ARGS__)
#define LSDEFv(type, name, vname, ...) _LSDEFv(PP_NARG(__VA_ARGS__), type, name, vname, __VA_ARGS__)

#endif /* LARMIER_STUB_H */
