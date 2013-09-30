/* Copyright 2013 Drew Thoreson */

/* This file is part of libpsnet
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * libpsnet is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * libpsnet.  If not, see <http://www.gnu.org/licenses/>
 */

#ifndef _PARSE_H
#define _PARSE_H

#include "jsmn.h"
#include "types.h"

#define JSMN_NTOK 256

enum psnet_status { PSNET_OKAY, PSNET_ERROR };

/*
 * Returns the size and status fields from a psnet header.
 *
 * @status location to store the value of the status field
 * @size location to store the value of the size field
 * @msg the header to be parsed
 */
int parse_header(int *status, size_t *size, char *msg);

/*
 * Parses a node array into a list of struct psnet_list_entry.
 *
 * @head the struct list_head to which the parsed elements are to be added
 * @msg the node list to be parsed
 * @nentries the maximum number of entries to parse
 */
int parse_node_list(struct list_head *head, char *msg, int nentries);

int parse_message(const char *msg, jsmntok_t *tok, size_t *ntok);

#endif
