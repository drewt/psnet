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

#ifndef _MISC_H
#define _MISC_H

#ifdef DAEMON
#define ANSI_YELLOW ""
#define ANSI_GREEN  ""
#define ANSI_RED    ""
#define ANSI_RESET  ""
#else
#define ANSI_YELLOW "\x1b[33m"
#define ANSI_GREEN  "\x1b[32m"
#define ANSI_RED    "\x1b[31m"
#define ANSI_RESET  "\x1b[0m"
#endif

void daemonize (const char *log_file);

#endif
