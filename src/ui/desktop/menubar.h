// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Desktop main menu bar code.
 */
/*
 * Authors:
 *   Tavmjong Bah       <tavmjong@free.fr>
 *   Alex Valavanis     <valavanisalex@gmail.com>
 *   Patrick Storz      <eduard.braun2@gmx.de>
 *   Krzysztof Kosiński <tweenk.pl@gmail.com>
 *   Kris De Gussem     <Kris.DeGussem@gmail.com>
 *   Sushant A.A.       <sushant.co19@gmail.com>
 *
 * Copyright (C) 2018 Authors
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 * Read the file 'COPYING' for more information.
 */

#ifndef SEEN_DESKTOP_MENUBAR_H
#define SEEN_DESKTOP_MENUBAR_H

#include <glibmm/refptr.h>

namespace Glib {
class Quark;
} // namespace Glib

namespace Gio {
class MenuModel;
class Menu;
} // namespace Gio;

void build_menu();

enum class UseIcons {
    never = -1, // Match existing preference numbering.
    as_requested,
    always,
};

// Rebuild menu with icons enabled or disabled. Recursive.
void rebuild_menu(Glib::RefPtr<Gio::MenuModel> const &menu, Glib::RefPtr<Gio::Menu> const &menu_copy,
                  UseIcons useIcons, Glib::Quark const &quark, Glib::RefPtr<Gio::Menu>& recent_files);

#endif // SEEN_DESKTOP_MENUBAR_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
