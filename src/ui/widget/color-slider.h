// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * A slider with colored background - implementation.
 *//*
 * Authors:
 *   see git history
 *
 * Copyright (C) 2018-2024 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_COLOR_SLIDER_H
#define SEEN_COLOR_SLIDER_H

#include <glibmm/refptr.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/gesture.h> // Gtk::EventSequenceState
#include <sigc++/signal.h>

#include <sigc++/scoped_connection.h>
#include "colors/spaces/components.h"

namespace Gtk {
class Builder;
class EventControllerMotion;
class GestureClick;
} // namespace Gtk

namespace Inkscape::Colors {
class ColorSet;
} // namespace Inkscape::Colors

namespace Inkscape::UI::Widget {

/*
 * A slider with colored background
 */
class ColorSlider : public Gtk::DrawingArea {
public:
    ColorSlider(
        BaseObjectType *cobject,
        Glib::RefPtr<Gtk::Builder> const &builder,
        std::shared_ptr<Colors::ColorSet> color,
        Colors::Space::Component component);
    ~ColorSlider() override = default;

    double getScaled() const;
    void setScaled(double value);
protected:
    friend class ColorPageChannel;

    std::shared_ptr<Colors::ColorSet> _colors;
    Colors::Space::Component _component;
private:
    void draw_func(Cairo::RefPtr<Cairo::Context> const &cr, int width, int height);

    void on_click_pressed(Gtk::GestureClick const &click, int n_press, double x, double y);
    void on_motion(Gtk::EventControllerMotion const &motion, double x, double y);
    void update_component(double x, double y, Gdk::ModifierType const state);

    sigc::scoped_connection _changed_connection;
    sigc::signal<void ()> signal_value_changed;

    int _arrow_x, _arrow_y = 0;

    // Memory buffers for the painted gradient
    std::vector<unsigned int> _gr_buffer;
    Glib::RefPtr<Gdk::Pixbuf> _gradient;
};

} // namespace Inkscape::UI::Widget

#endif // SEEN_COLOR_SLIDER_H

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
