// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Build a set of color sliders for a given color space
 *//*
 * Copyright (C) 2024 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_SP_COLOR_SLIDERS_H
#define SEEN_SP_COLOR_SLIDERS_H

#include <gtkmm/box.h>

#include <sigc++/scoped_connection.h>
#include "ui/widget/color-slider.h"
#include "ui/widget/spinbutton.h"

using namespace Inkscape::Colors;

namespace Inkscape::Colors {
class Color;
class ColorSet;
namespace Space {
class AnySpace;
}
}

namespace Gtk {
class Expander;
class Builder;
}

namespace Inkscape::UI::Widget {
class ColorWheel;
class InkSpinButton;
class ColorSlider;
class ColorWheelBase;
class ColorPageChannel;

class ColorPage : public Gtk::Box
{
public:
    ColorPage(std::shared_ptr<Space::AnySpace> space, std::shared_ptr<ColorSet> colors);
protected:
    Glib::RefPtr<Gtk::Builder> _builder;

    std::shared_ptr<Space::AnySpace> _space;
    std::shared_ptr<ColorSet> _selected_colors;
    std::shared_ptr<ColorSet> _specific_colors;

    std::vector<std::unique_ptr<ColorPageChannel>> _channels;
private:
    sigc::scoped_connection _specific_changed_connection;
    sigc::scoped_connection _selected_changed_connection;
    ColorWheel* _color_wheel = nullptr;
    sigc::scoped_connection _color_wheel_changed;
    Gtk::Expander& _expander;
};

class ColorPageChannel
{
public:
    ColorPageChannel(
        std::shared_ptr<Colors::ColorSet> color,
        Gtk::Label &label,
        ColorSlider &slider,
        InkSpinButton &spin);
    ColorPageChannel(const ColorPageChannel&) = delete;
private:
    Gtk::Label &_label;
    ColorSlider &_slider;
    InkSpinButton &_spin;
    Glib::RefPtr<Gtk::Adjustment> _adj;
    std::shared_ptr<Colors::ColorSet> _color;
    sigc::scoped_connection _adj_changed;
    sigc::scoped_connection _slider_changed;
    sigc::scoped_connection _color_changed;
};

} // namespace Inkscape::UI::Widget

#endif /* !SEEN_SP_COLOR_SLIDERS_H */

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim:filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99:
