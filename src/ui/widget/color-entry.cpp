// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Entry widget for typing color value in css form
 *//*
 * Authors:
 *   Tomasz Boczkowski <penginsbacon@gmail.com>
 *
 * Copyright (C) 2014 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#include <glibmm.h>
#include <glibmm/i18n.h>
#include <iomanip>
#include <iostream>

#include "color-entry.h"
#include "util-string/ustring-format.h"

namespace Inkscape {
namespace UI {
namespace Widget {

ColorEntry::ColorEntry(std::shared_ptr<Colors::ColorSet> colors)
    : _colors(std::move(colors))
    , _updating(false)
    , _updatingrgba(false)
    , _prevpos(0)
    , _lastcolor()
{
    set_name("ColorEntry");

    _color_changed_connection = _colors->signal_changed.connect(sigc::mem_fun(*this, &ColorEntry::_onColorChanged));
    signal_activate().connect(sigc::mem_fun(*this, &ColorEntry::_onColorChanged));
    get_buffer()->signal_inserted_text().connect(sigc::mem_fun(*this, &ColorEntry::_inputCheck));
    _onColorChanged();

    // add extra character for pasting a hash, '#11223344'
    set_max_length(9);
    set_width_chars(8);
    set_tooltip_text(_("Hexadecimal RGBA value of the color"));
}

ColorEntry::~ColorEntry()
{
    _color_changed_connection.disconnect();
}

void ColorEntry::_inputCheck(guint pos, const gchar * /*chars*/, guint n_chars)
{
    // remember position of last character, so we can remove it.
    // we only overflow by 1 character at most.
    _prevpos = pos + n_chars - 1;
}

void ColorEntry::on_changed()
{
    if (_updating || _updatingrgba) {
        return;
    }

    auto new_color = Colors::Color::parse(get_text());

    _updatingrgba = true;
    if (new_color) {
        _colors->setAll(*new_color);
    }
    _updatingrgba = false;
}


void ColorEntry::_onColorChanged()
{
    if (_updatingrgba) {
        return;
    }

    if (_colors->isEmpty()) {
        set_text(_("N/A"));
        return;
    }

    _lastcolor = _colors->getAverage();;
    auto text = _lastcolor->toString();

    std::string old_text = get_text();
    if (old_text != text) {
        _updating = true;
        set_text(text);
        _updating = false;
    }
}
}
}
}
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
