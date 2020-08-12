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

#ifndef LARMIER_H
#define LARMIER_H

#define LARMIER_BCA     "LARMIER_BCA"
#define LARMIER_LEN     4096
#define BCA_MAP_LEN     (LARMIER_LEN - sizeof(uint16_t))

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

void
larmier_stub(bool on);

void
larmier_stub(bool on)
{
    if (on) {
        setenv("LARMIER_STUB", "1", 1);
    } else {
        setenv("LARMIER_STUB", "0", 1);
    }
}

typedef struct {
    uint16_t count;
    char map[BCA_MAP_LEN];
} __attribute__((packed)) bca_t;

#endif /* LARMIER_H */
