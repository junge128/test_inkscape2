// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Go over a widget representing a menu, & set tooltips on its items from app label-to-tooltip map.
 * Optionally (per Preference) shift Gtk::MenuItems with icons to align with Toggle & Radio buttons
 */
/*
 * Authors:
 *   Tavmjong Bah       <tavmjong@free.fr>
 *   Patrick Storz      <eduard.braun2@gmx.de>
 *   Daniel Boles       <dboles.src+inkscape@gmail.com>
 *
 * Copyright (C) 2020-2023 Authors
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 * Read the file 'COPYING' for more information.
 *
 */

#ifndef SEEN_DESKTOP_MENU_SET_TOOLTIPS_SHIFT_ICONS_H
#define SEEN_DESKTOP_MENU_SET_TOOLTIPS_SHIFT_ICONS_H

namespace Gtk {
class Widget;
} // namespace Gtk

bool set_tooltips_and_shift_icons(Gtk::Widget &menu, bool shift_icons);

#endif // SEEN_DESKTOP_MENU_SET_TOOLTIPS_SHIFT_ICONS_H

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
