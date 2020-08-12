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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

#include "larmier.h"

#ifndef MYSTRING
#define MYSTRING "This is my string"
#endif

int
main(int argc, char **argv)
{
    char *ptr;
    int err;
    int ret = EXIT_FAILURE;

    larmier_stub(true);

    err = asprintf(&ptr, "MYSTRING: %s", MYSTRING);
    if (err < 0) {
        perror("asprintf");
        goto out;
    }

    larmier_stub(false);

    free(ptr);

    ret = EXIT_SUCCESS;

out:

    larmier_stub(false);

    fclose(stderr);
    fclose(stdout);
    fclose(stdin);

    return ret;
}
