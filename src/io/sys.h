// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_SYS_H
#define SEEN_SYS_H

/*
 * System abstraction utility routines
 *
 * WARNING:
 *   Most of these routines should not be used. Filenames should always
 *   be std::string, not utf8 encoded. Filenames should be converted
 *   to/from Glib::ustring when used in the GUI.
 *
 * Authors:
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (C) 2004-2005 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <cstdio>
#include <string>

#include <glib.h>
#include <glibmm/ustring.h>

/*#####################
## U T I L I T Y
#####################*/

namespace Inkscape::IO {

void dump_fopen_call( char const *utf8name, char const *id );

FILE *fopen_utf8name( char const *utf8name, char const *mode );

bool file_test( char const *utf8name, GFileTest test );

bool file_is_writable( char const *utf8name);

Glib::ustring sanitizeString(char const *str);

Glib::ustring get_file_extension(Glib::ustring const &path);

std::string get_file_extension(std::string const &path);
void remove_file_extension(std::string &path);

} // namespace Inkscape::IO

#endif // SEEN_SYS_H
