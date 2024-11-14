// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef INK_SPIN_BUTTON_H
#define INK_SPIN_BUTTON_H

#include <gtkmm.h>
#include <sigc++/scoped_connection.h>

namespace Inkscape::UI::Widget {

class InkSpinButton : public Gtk::Widget {
public:
    InkSpinButton();
    InkSpinButton(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder);

    ~InkSpinButton() override;

    // Set "adjustment" to establish limits and step
    void set_adjustment(const Glib::RefPtr<Gtk::Adjustment>& adjustment);
    Glib::RefPtr<Gtk::Adjustment>& get_adjustment();
    // Number of decimal digits to use for formatting values
    void set_digits(int digits);
    int get_digits() const;
    void update();
    // Specify optional suffix to show after the value
    void set_suffix(const std::string& suffix, bool add_half_space = true);
    // Specify optional prefix to show in front of the value
    void set_prefix(const std::string& prefix, bool add_space = true);
    // Set to true to draw a border, false to hide it
    void set_has_frame(bool frame = true);
    // Set to true to hide insignificant zeros after decimal point
    void set_trim_zeros(bool trim);
    // Which widget to focus if defocusing this spin button;
    // if not set explicitly, next available focusable widget will be used
    void set_defocus_widget(Gtk::Widget* widget) { _defocus_widget = widget; }
    // Suppress expression evaluator
    void set_dont_evaluate(bool flag) { _dont_evaluate = flag; }
    // Set distance in pixels of drag travel to adjust full button range; the lower the value the more sensitive the dragging gets
    void set_drag_sensitivity(double distance);

    // ----------- PROPERTIES ------------
    // Glib::PropertyProxy<int> property_digits() { return prop_digits.get_proxy(); }

private:
    void construct();
    Gtk::SizeRequestMode get_request_mode_vfunc() const override;
    void measure_vfunc(Gtk::Orientation orientation, int for_size, int& minimum, int& natural, int& minimum_baseline, int& natural_baseline) const override;
    void size_allocate_vfunc(int width, int height, int baseline) override;

    Glib::RefPtr<Gtk::Adjustment> _adjustment = Gtk::Adjustment::create(0, 0, 100, 1);
    Gtk::Button _minus;
    Gtk::Label _value;
    Gtk::Button _plus;
    Gtk::Entry _entry;

    // ------------- CONTROLLERS -------------

    Glib::RefPtr<Gtk::EventControllerMotion> _motion;
    void on_motion_enter(double x, double y);
    void on_motion_leave();

    Glib::RefPtr<Gtk::EventControllerMotion> _motion_value;
    void on_motion_enter_value(double x, double y);
    void on_motion_leave_value();

    Glib::RefPtr<Gtk::GestureDrag> _drag_value;
    void on_drag_begin_value(Gdk::EventSequence* sequence);
    void on_drag_update_value(Gdk::EventSequence* sequence);
    void on_drag_end_value(Gdk::EventSequence* sequence);

    Glib::RefPtr<Gtk::EventControllerScroll> _scroll;
    void on_scroll_begin(); // Not used with mouse wheel.
    bool on_scroll(double dx, double dy);
    void on_scroll_end(); // Not used with mouse wheel.

    // Use Gesture to get access to modifier keys (rather than "clicked" signal).
    Glib::RefPtr<Gtk::GestureClick> _click_plus;
    void on_pressed_plus(int n_press, double x, double y);

    Glib::RefPtr<Gtk::GestureClick> _click_minus;
    void on_pressed_minus(int n_press, double x, double y);

    Glib::RefPtr<Gtk::EventControllerFocus> _focus;

    Glib::RefPtr<Gtk::EventControllerKey> _key_entry;
    bool on_key_pressed(guint keyval, guint keycode, Gdk::ModifierType state); // "pressed" vs GTK3 "press"
    // void on_key_released(guint keyval, guint keycode, Gdk::ModifierType state);

    void on_activate(); // "activate" fires on pressing Enter in an entry widget

    void on_changed();
    void on_editing_done();
    void enter_edit();
    void exit_edit();
    bool defocus();
    void show_arrows(bool on = true);
    bool commit_entry();
    void change_value(double inc, Gdk::ModifierType state);
    void set_value(double new_value);
    std::string format(double value, bool with_prefix_suffix, bool with_markup, bool trim_zeros) const;
    void start_spinning(double steps, Gdk::ModifierType state, Glib::RefPtr<Gtk::GestureClick>& gesture);
    void stop_spinning();

    // ---------------- DATA ------------------
    double _initial_value = 0.0; // Value of adjustment at start of drag.
    double _drag_full_travel = 300.0; // dragging sensitivity in pixels
    bool _dragged = false;      // Hack to avoid enabling entry after drag. TODO Probably not needed now.
    double _scroll_counter = 0; // Counter to control incrementing/decrementing rate
    std::string _suffix; // suffix to show after the number, if any
    std::string _prefix; // prefix to show before the number, if any
    bool _trim_zeros = true;    // hide insignificant zeros in decimal fraction
    sigc::connection _connection;
    int _buttons_width = 0;     // width of increment/decrement button
    int _entry_height = 0;      // natural height of Gtk::Entry
    int _baseline = 0;
    sigc::scoped_connection _spinning;
    Gtk::Widget* _defocus_widget = nullptr;
    bool _dont_evaluate = false; // turn off expression evaluator?
    Glib::RefPtr<Gdk::Cursor> _old_cursor;
    Glib::RefPtr<Gdk::Cursor> _current_cursor;

    // ----------- PROPERTIES ------------
    int prop_digits = 0;
};

} // namespace Inkscape::UI::Widget

#endif // INK_SPIN_BUTTON_H
