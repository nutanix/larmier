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
#include <string.h>

#include "larmier_stub.h"

LSDEF(FILE *, tmpfile)
{
    errno = ENOSPC;
    return NULL;
}

LSDEF(char *, strdup, const char *, s)
{
    errno = ENOMEM;
    return NULL;
}

LSDEF(int, fputs, const char *, s, FILE *, stream)
{
    errno = EIO;
    return -1;
}
