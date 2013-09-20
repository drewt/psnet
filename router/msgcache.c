/* Copyright 2013 Drew Thoreson */

/* This file is part of psnet
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * psnet is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * psnet.  If not, see <http://www.gnu.org/licenses/>
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "deltalist.h"
#include "misc.h"

#define ID_STRLEN 5

static unsigned long delta_hash (const void *msg);
static int delta_equals (const void *a, const void *b);
static void delta_act (const void *msg);

static struct delta_list msg_cache = {
    .resolution = 1,
    .interval = 10,
    .size = 0,
    .delta = 0,
    .delta_head = NULL,
    .delta_tail = NULL,
    .hash = delta_hash,
    .equals = delta_equals,
    .act = delta_act,
    .free = free
};

static unsigned long delta_hash (const void *msg)
{
    const char *s = msg;
    unsigned long hash = 5381;
    int c;

    while ((c = *s++))
        hash = ((hash << 5) + hash) + c; // hash * 33 + c

    return hash;
}

static int delta_equals (const void *a, const void *b)
{
    const char *s0 = a;
    const char *s1 = b;

    return !strcmp (s0, s1);
}

static void delta_act (const void *msg)
{
#ifdef PSNETLOG
    const char *id = msg;
    printf (ANSI_RED "X %s\n" ANSI_RESET, id);
#endif
}

int cache_msg (char *id)
{
    int rc = delta_update (&msg_cache, id);
#ifdef PSNETLOG
    if (!rc)
        printf (ANSI_GREEN "C %s\n" ANSI_RESET, id);
#endif
    return rc;
}

void msg_cache_init (void)
{
    delta_init (&msg_cache);
}

unsigned int msg_cache_size (void)
{
    return delta_size (&msg_cache);
}
