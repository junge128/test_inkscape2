// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Ralf Stephan <ralf@ark.in-berlin.de>
 *   Abhishek Sharma
 *
 * Copyright (C) Authors 2000-2005
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "color-picker.h"
#include <glibmm/ustring.h>
#include <gtkmm/enums.h>
#include <gtkmm/menubutton.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/widget.h>
#include <utility>

#include "desktop.h"
#include "document-undo.h"
#include "inkscape.h"
#include "ui/util.h"
#include "ui/widget/color-notebook.h"
#include "ui/widget/color-preview.h"

static bool _in_use = false;

namespace Inkscape::UI::Widget {

ColorPicker::ColorPicker(Glib::ustring title,
                         Glib::ustring const &tip,
                         Colors::Color const &initial,
                         bool const undo,
                         bool use_transparency)
    : _preview(Gtk::make_managed<ColorPreview>(initial.toRGBA()))
    , _title(std::move(title))
    , _undo(undo)
    , _colors(std::make_shared<Colors::ColorSet>(nullptr, use_transparency))
{
    // set tooltip if given, otherwise leave original tooltip in place (from external button)
    if (!tip.empty()) {
        set_tooltip_text(tip);
    }

    _colors->set(initial);
    _construct();
}

ColorPicker::ColorPicker(BaseObjectType *cobject, Glib::RefPtr<Gtk::Builder> const &,
                         Glib::ustring title, bool use_transparency)
    : Gtk::MenuButton(cobject)
    , _preview(Gtk::make_managed<ColorPreview>(0x0))
    , _title(std::move(title))
    , _colors(std::make_shared<Colors::ColorSet>(nullptr, use_transparency))
{
    _construct();
}

void ColorPicker::_construct() {
    // match min height with that of the current theme button and enforce square shape for our color picker
    Gtk::Button button;
    auto height = button.measure(Gtk::Orientation::VERTICAL).sizes.minimum;
    set_name("ColorPicker");
    restrict_minsize_to_square(*this, height);

    _preview->setStyle(ColorPreview::Outlined);
    set_child(*_preview);

    // postpone color selector creation until popup is open
    _popup.signal_show().connect([this](){
        if (!_color_selector) {
            _color_selector = Gtk::make_managed<ColorNotebook>(_colors);
            _color_selector->set_label(_title);
            _color_selector->set_margin(4);
            _popup.set_child(*_color_selector);
        }
    });
    set_popover(_popup);

    _colors->signal_changed.connect(sigc::mem_fun(*this, &ColorPicker::_onSelectedColorChanged));
    _colors->signal_released.connect(sigc::mem_fun(*this, &ColorPicker::_onSelectedColorChanged));
}

ColorPicker::~ColorPicker() = default;

void ColorPicker::setTitle(Glib::ustring title) {
    _title = std::move(title);
}

void ColorPicker::setColor(Colors::Color const &color)
{
    if (_in_use) return;

    _updating = true;
    set_preview(color.toRGBA());
    _colors->set(color);
    _updating = false;
}

void ColorPicker::open() {
    popup();
}

void ColorPicker::close() {
    popdown();
}

void ColorPicker::_onSelectedColorChanged()
{
    if (_updating || _in_use)
        return;

    auto color = _colors->get();
    if (!color) return;

    set_preview(color->toRGBA());

    if (_undo && SP_ACTIVE_DESKTOP) {
        DocumentUndo::done(SP_ACTIVE_DESKTOP->getDocument(), /* TODO: annotate */ "color-picker.cpp:122", "");
    }

    _in_use = true;
    _changed_signal.emit(*color);
    on_changed(*color);
    _in_use = false;
}

void ColorPicker::on_changed(Colors::Color const &) {}

void ColorPicker::set_preview(std::uint32_t rgba)
{
    bool has_alpha = _colors->getAlphaConstraint().value_or(true);
    _preview->setRgba32(has_alpha ? rgba : rgba | 0xff);
}

} // namespace Inkscape::UI::Widget

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
