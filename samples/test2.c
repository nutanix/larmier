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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "larmier.h"

#ifndef MYSTRING
#define MYSTRING "This is my string"
#endif

int
main(int argc, char **argv)
{
    FILE *fp;
    char *ptr;
    int err;
    int ret = EXIT_FAILURE;

    larmier_stub(true);

    fp = tmpfile();
    if (fp == NULL) {
        perror("tmpfile");
        goto out;
    }

    ptr = strdup(MYSTRING);
    if (ptr == NULL) {
        perror("strdup");
        goto out_fp;
    }

    err = fputs(ptr, fp);
    if (err < 0) {
        perror("fputs");
        goto out_str;
    }

    ret = EXIT_SUCCESS;

out_str:
    larmier_stub(false);

    free(ptr);

out_fp:
    larmier_stub(false);

    (void)fclose(fp);

out:
    larmier_stub(false);

    (void)fclose(stderr);
    (void)fclose(stdout);
    (void)fclose(stdin);

    return ret;
}
