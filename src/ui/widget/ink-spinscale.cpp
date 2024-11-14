// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
    A widget that allows entering a numerical value either by
    clicking/dragging on a custom Gtk::Scale or by using a
    Gtk::SpinButton. The custom Gtk::Scale differs from the stock
    Gtk::Scale in that it includes a label to save space and has a
    "slow dragging" mode triggered by the Alt key.
*/
/*
 * Authors:
 *   Tavmjong Bah <tavmjong@free.fr>
 *
 * Copyright (C) 2017 Tavmjong Bah
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <gdkmm/enums.h>
#include <utility>
#include <gdk/gdk.h>
#include <sigc++/functors/mem_fun.h>
#include <gdkmm/general.h>
#include <gdkmm/cursor.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/eventcontrollerkey.h>
#include <gtkmm/eventcontrollermotion.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/snapshot.h>
#include <gtkmm/spinbutton.h>

#include "ink-spinscale.h"
#include "ui/controller.h"
#include "ui/pack.h"
#include "ui/util.h"

namespace Controller = Inkscape::UI::Controller;

InkScale::InkScale(Glib::RefPtr<Gtk::Adjustment> adjustment, Gtk::SpinButton* spinbutton)
    : Glib::ObjectBase("InkScale")
    , parent_type(adjustment)
    , _spinbutton(spinbutton)
    , _dragging(false)
    , _drag_start(0)
    , _drag_offset(0)
{
    set_name("InkScale");

    auto const click = Gtk::GestureClick::create();
    click->set_button(0); // any
    click->set_propagation_phase(Gtk::PropagationPhase::TARGET);
    click->signal_pressed().connect(Controller::use_state(sigc::mem_fun(*this, &InkScale::on_click_pressed), *click));
    click->signal_released().connect(Controller::use_state(sigc::mem_fun(*this, &InkScale::on_click_released), *click));
    add_controller(click);

    auto const motion = Gtk::EventControllerMotion::create();
    motion->set_propagation_phase(Gtk::PropagationPhase::TARGET);
    motion->signal_enter().connect(sigc::mem_fun(*this, &InkScale::on_motion_enter));
    motion->signal_leave().connect(sigc::mem_fun(*this, &InkScale::on_motion_leave));
    motion->signal_motion().connect([this, &motion = *motion](auto &&...args) { on_motion_motion(motion, args...); });
    add_controller(motion);
}

void
InkScale::set_label(Glib::ustring label) {
    _label = std::move(label);
}

void
InkScale::snapshot_vfunc(Glib::RefPtr<Gtk::Snapshot> const &snapshot)
{
    parent_type::snapshot_vfunc(snapshot);

    if (_label.empty()) {
        return;
    }

    auto const alloc = get_allocation();

    // Get SpinButton style info...
    auto const text_color = _spinbutton->get_color();

    // Create Pango layout.
    auto layout_label = create_pango_layout(_label);
    layout_label->set_ellipsize(Pango::EllipsizeMode::END);
    layout_label->set_width(PANGO_SCALE * get_width());

    // Get y location of SpinButton text (to match vertical position of SpinButton text).
    int x, y;
    // TODO: GTK4: This func is gone (everywhere). Work out a replacement if poss – baseline maybe?
    // _spinbutton->get_layout_offsets(x, y);
    x = y = 0;
    auto btn_alloc = _spinbutton->get_allocation();
    y += btn_alloc.get_y() - alloc.get_y();

    // Fill widget proportional to value.
    double fraction = get_fraction();

    // Get through rectangle and clipping point for text.
    Gdk::Rectangle slider_area = get_range_rect();
    // If we are not sensitive/editable, we render in normal color, no clipping
    auto const clip_text_x = !_spinbutton->is_sensitive() ? 0.0
                             : slider_area.get_x() + slider_area.get_width() * fraction;

    auto const cr = snapshot->append_cairo(alloc);

    // Render text in normal text color.
    cr->save();
    cr->rectangle(clip_text_x, 0, alloc.get_width() - clip_text_x, alloc.get_height());
    cr->clip();
    Gdk::Cairo::set_source_rgba(cr, text_color);
    cr->move_to(5, y );
    layout_label->show_in_cairo_context(cr);
    cr->restore();

    if (clip_text_x == 0.0) {
        return;
    }

    // Render text, clipped, in white over bar (TODO: use same color as SpinButton progress bar).
    cr->save();
    cr->rectangle(0, 0, clip_text_x, alloc.get_height());
    cr->clip();
    cr->set_source_rgba(1, 1, 1, 1);
    cr->move_to(5, y);
    layout_label->show_in_cairo_context(cr);
    cr->restore();
}

static bool get_constrained(Gdk::ModifierType const state)
{
    return Controller::has_flag(state, Gdk::ModifierType::CONTROL_MASK);
}

Gtk::EventSequenceState InkScale::on_click_pressed(Gtk::GestureClick const &click,
                                                   int /*n_press*/, double const x, double /*y*/)
{
    auto const state = click.get_current_event_state();;
    if (!Controller::has_flag(state, Gdk::ModifierType::ALT_MASK)) {
        auto const constrained = get_constrained(state);
        set_adjustment_value(x, constrained);
    }

    // Dragging must be initialized after any adjustment due to button press.
    _dragging = true;
    _drag_start  = x;
    _drag_offset = get_width() * get_fraction(); 
    return Gtk::EventSequenceState::CLAIMED;
}

Gtk::EventSequenceState InkScale::on_click_released(Gtk::GestureClick const & /*click*/,
                                                    int /*n_press*/, double /*x*/, double /*y*/)
{
    _dragging = false;
    return Gtk::EventSequenceState::CLAIMED;
}

void InkScale::on_motion_enter(double /*x*/, double /*y*/)
{
    set_cursor("n-resize");
}

void InkScale::on_motion_motion(Gtk::EventControllerMotion const &motion, double x, double /*y*/)
{
    if (!_dragging) {
        return;
    }

    auto const state = motion.get_current_event_state();
    if (!Controller::has_flag(state, Gdk::ModifierType::ALT_MASK)) {
        // Absolute change
        auto const constrained = get_constrained(state);
        set_adjustment_value(x, constrained);
    } else {
        // Relative change
        double xx = (_drag_offset + (x - _drag_start) * 0.1);
        set_adjustment_value(xx);
    }
}

void InkScale::on_motion_leave()
{
    set_cursor(Glib::RefPtr<Gdk::Cursor>{});
}

double
InkScale::get_fraction() {
    Glib::RefPtr<Gtk::Adjustment> adjustment = get_adjustment();
    double upper = adjustment->get_upper();
    double lower = adjustment->get_lower();
    double value = adjustment->get_value();
    double fraction = (value - lower)/(upper - lower);
    return fraction;
}

void
InkScale::set_adjustment_value(double x, bool constrained) {
    Glib::RefPtr<Gtk::Adjustment> adjustment = get_adjustment();
    double upper = adjustment->get_upper();
    double lower = adjustment->get_lower();
    double range = upper-lower;

    Gdk::Rectangle slider_area = get_range_rect();
    double fraction = (x - slider_area.get_x()) / (double)slider_area.get_width();
    double value = fraction * range + lower;
    
    if (constrained) {
        // TODO: do we want preferences for (any of) these?
        if (fmod(range + 1, 16) == 0) {
                value = round(value/16) * 16;
        } else if (range >= 1000 && fmod(upper, 100) == 0) {
                value = round(value/100) * 100;
        } else if (range >= 100 && fmod(upper, 10) == 0) {
                value = round(value/10) * 10;
        } else if (range > 20 && fmod(upper, 5) == 0) {
                value = round(value / 5) * 5;
        } else if (range > 2) {
                value = round(value);
        } else if (range <= 2) {
                value = round(value * 10) / 10;
        }
    }

    adjustment->set_value( value );
}

/*******************************************************************/

InkSpinScale::InkSpinScale(double value, double lower,
                           double upper, double step_increment,
                           double page_increment, double page_size)
    : InkSpinScale{Gtk::Adjustment::create(value,
                                           lower,
                                           upper,
                                           step_increment,
                                           page_increment,
                                           page_size)}
{
    // TODO: Why does the ctor from doubles do this stuff but the other doesnʼt?
    _spinbutton->set_valign(Gtk::Align::FILL);

    auto const key = Gtk::EventControllerKey::create();
    key->signal_key_released().connect(sigc::mem_fun(*this, &InkSpinScale::on_key_released));
    add_controller(key);
}

InkSpinScale::InkSpinScale(Glib::RefPtr<Gtk::Adjustment> adjustment)
    : _adjustment(std::move(adjustment))
    , _spinbutton{Gtk::make_managed<Gtk::SpinButton>(_adjustment)}
    , _scale{Gtk::make_managed<InkScale>(_adjustment, _spinbutton)}
{
    g_assert (_adjustment->get_upper() - _adjustment->get_lower() > 0);

    set_name("InkSpinScale");

    _spinbutton->set_numeric();

    Inkscape::UI::pack_end(*this, *_spinbutton, Inkscape::UI::PackOptions::shrink       );
    Inkscape::UI::pack_end(*this, *_scale,      Inkscape::UI::PackOptions::expand_widget);
}

void
InkSpinScale::set_label(Glib::ustring label) {
    _scale->set_label(std::move(label));
}

void
InkSpinScale::set_digits(int digits) {
    _spinbutton->set_digits(digits);
}

int
InkSpinScale::get_digits() const {
    return _spinbutton->get_digits();
}

void
InkSpinScale::set_focus_widget(GtkWidget * focus_widget) {
    _focus_widget = focus_widget;
}

// Return focus to canvas.
void InkSpinScale::on_key_released(unsigned keyval, unsigned /*keycode*/, Gdk::ModifierType /*state*/)
{
    switch (keyval) {
        case GDK_KEY_Escape:
        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
            if (_focus_widget) {
                gtk_widget_grab_focus( _focus_widget );
            }
            break;
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
// vim:filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99:
