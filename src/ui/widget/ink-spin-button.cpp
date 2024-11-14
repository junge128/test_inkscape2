// SPDX-License-Identifier: GPL-2.0-or-later
//
// Authors: Tavmjong Bah
// Mike Kowalski
//

#include "ink-spin-button.h"
#include <iomanip>

#include "ui/containerize.h"
#include "ui/util.h"
#include "util/expression-evaluator.h"

namespace Inkscape::UI::Widget {

// CSS styles for InkSpinButton
// language=CSS
auto ink_spinbutton_css = R"=====(
@define-color border-color @unfocused_borders;
@define-color bgnd-color alpha(@theme_base_color, 1.0);
@define-color focus-color alpha(@theme_selected_bg_color, 0.5);
/* :root { --border-color: lightgray; } - this is not working yet, so using nonstandard @define-color */
#InkSpinButton { border: 0 solid @border-color; border-radius: 3px; background-color: @bgnd-color; }
#InkSpinButton.frame { border: 1px solid @border-color; }
#InkSpinButton:hover button { opacity: 1; }
#InkSpinButton:focus-within { outline: 2px solid @focus-color; outline-offset: -2px; }
#InkSpinButton button { border: 0 solid alpha(@border-color, 0.30); border-radius: 2px; padding: 1px; min-width: 6px; min-height: 8px; -gtk-icon-size: 10px; background-image: none; }
#InkSpinButton button.left  { border-top-right-radius: 0; border-bottom-right-radius: 0; border-right-width: 1px; }
#InkSpinButton button.right { border-top-left-radius: 0; border-bottom-left-radius: 0; border-left-width: 1px; }
#InkSpinButton entry { border: none; border-radius: 3px; padding: 0; min-height: 13px; background-color: @bgnd-color; outline-width: 0; }
)=====";
constexpr int timeout_click = 500;
constexpr int timeout_repeat = 50;

static Glib::RefPtr<Gdk::Cursor> g_resizing_cursor;

void InkSpinButton::construct() {
    set_name("InkSpinButton");

    _minus.set_name("InkSpinButton-Minus");
    _minus.add_css_class("left");
    _value.set_name("InkSpinButton-Value");
    _plus.set_name("InkSpinButton-Plus");
    _plus.add_css_class("right");
    _entry.set_name("InkSpinButton-Entry");
    _entry.set_alignment(0.5);
    _entry.set_max_width_chars(3); // let it shrink, we can always stretch it

    _value.set_expand();
    _entry.set_expand();

    _minus.set_margin(0);
    _minus.set_size_request(8, -1);
    _value.set_margin(0);
    _plus.set_margin(0);
    _plus.set_size_request(8, -1);
    _minus.set_can_focus(false);
    _plus.set_can_focus(false);

    _minus.set_icon_name("go-previous-symbolic");
    _plus.set_icon_name("go-next-symbolic");

    containerize(*this);
    _minus.insert_at_end(*this);
    _value.insert_at_end(*this);
    _entry.insert_at_end(*this);
    _plus.insert_at_end(*this);

    set_focus_child(_entry);

    static Glib::RefPtr<Gtk::CssProvider> provider;
    if (!provider) {
        provider = Gtk::CssProvider::create();
        provider->load_from_data(ink_spinbutton_css);
        auto const display = Gdk::Display::get_default();
        Gtk::StyleContext::add_provider_for_display(display, provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

    // ------------- CONTROLLERS -------------

    // This is mouse movement. Shows/hides +/- buttons.
    // Shows/hides +/- buttons.
    _motion = Gtk::EventControllerMotion::create();
    _motion->signal_enter().connect(sigc::mem_fun(*this, &InkSpinButton::on_motion_enter));
    _motion->signal_leave().connect(sigc::mem_fun(*this, &InkSpinButton::on_motion_leave));
    add_controller(_motion);

    // This is mouse movement. Sets cursor.
    _motion_value = Gtk::EventControllerMotion::create();
    _motion_value->signal_enter().connect(sigc::mem_fun(*this, &InkSpinButton::on_motion_enter_value));
    _motion_value->signal_leave().connect(sigc::mem_fun(*this, &InkSpinButton::on_motion_leave_value));
    _value.add_controller(_motion_value);

    // This is mouse drag movement. Changes value.
    _drag_value = Gtk::GestureDrag::create();
    _drag_value->signal_begin().connect(sigc::mem_fun(*this, &InkSpinButton::on_drag_begin_value));
    _drag_value->signal_update().connect(sigc::mem_fun(*this, &InkSpinButton::on_drag_update_value));
    _drag_value->signal_end().connect(sigc::mem_fun(*this, &InkSpinButton::on_drag_end_value));
    _drag_value->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    _value.add_controller(_drag_value);

    // Changes value.
    _scroll = Gtk::EventControllerScroll::create();
    _scroll->signal_scroll_begin().connect(sigc::mem_fun(*this, &InkSpinButton::on_scroll_begin));
    _scroll->signal_scroll().connect(sigc::mem_fun(*this, &InkSpinButton::on_scroll), false);
    _scroll->signal_scroll_end().connect(sigc::mem_fun(*this, &InkSpinButton::on_scroll_end));
    _scroll->set_flags(Gtk::EventControllerScroll::Flags::BOTH_AXES); // Mouse wheel is on y.
    add_controller(_scroll);

    _click_minus = Gtk::GestureClick::create();
    _click_minus->signal_pressed().connect(sigc::mem_fun(*this, &InkSpinButton::on_pressed_minus));
    _click_minus->signal_released().connect([this](int, double, double){ stop_spinning(); });
    _click_minus->signal_unpaired_release().connect([this](auto, auto, auto, auto){ stop_spinning(); });
    _click_minus->set_propagation_phase(Gtk::PropagationPhase::CAPTURE); // Steal from default handler.
    _minus.add_controller(_click_minus);

    _click_plus = Gtk::GestureClick::create();
    _click_plus->signal_pressed().connect(sigc::mem_fun(*this, &InkSpinButton::on_pressed_plus));
    _click_plus->signal_released().connect([this](int, double, double){ stop_spinning(); });
    _click_plus->signal_unpaired_release().connect([this](auto, auto, auto, auto){ stop_spinning(); });
    _click_plus->set_propagation_phase(Gtk::PropagationPhase::CAPTURE); // Steal from default handler.
    _plus.add_controller(_click_plus);

    _focus = Gtk::EventControllerFocus::create();
    _focus->signal_enter().connect([this]() {
        // show editable button if '*this' is focused, but not its entry
        if (_focus->is_focus()) {
            set_focusable(false);
            enter_edit();
        }
    });
    _focus->signal_leave().connect([this]() {
        commit_entry();
        exit_edit();
        set_focusable(true);
    });
    add_controller(_focus);
    _entry.set_focus_on_click(false);
    _entry.set_focusable(false);
    // _entry.set_activates_default(false);
    _entry.set_can_focus();
    set_can_focus();
    set_focusable();
    set_focus_on_click();

    _key_entry = Gtk::EventControllerKey::create();
    _key_entry->signal_key_pressed().connect(sigc::mem_fun(*this, &InkSpinButton::on_key_pressed), false); // Before default handler.
    _entry.add_controller(_key_entry);

    //                  GTK4
    // -------------   SIGNALS   -------------

    _entry.signal_activate().connect([this] { on_activate(); });

    // Value (button) NOT USED, Click handled by zero length drag.
    // m_value->signal_clicked().connect(sigc::mem_fun(*this, &SpinButton::on_value_clicked));

    auto m = _minus.measure(Gtk::Orientation::HORIZONTAL);
    _buttons_width = m.sizes.natural;
    m = _entry.measure(Gtk::Orientation::VERTICAL);
    _entry_height = m.sizes.natural;
    _baseline = m.baselines.natural;

    set_has_frame();
    show_arrows(false);
    _entry.hide();

    _connection = _adjustment->signal_value_changed().connect([this](){ update(); });
    update();
}

InkSpinButton::InkSpinButton():
    Glib::ObjectBase("InkSpinButton") {
    // prop_digits(*this, "digits", 0) {

    construct();
}

InkSpinButton::InkSpinButton(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder):
    Glib::ObjectBase("InkSpinButton"),
    Gtk::Widget(cobject) {
    // prop_digits(*this, "digits", 0) {

    construct();
}

InkSpinButton::~InkSpinButton() = default;

Gtk::SizeRequestMode InkSpinButton::get_request_mode_vfunc() const {
    return Gtk::Widget::get_request_mode_vfunc();
}

void InkSpinButton::measure_vfunc(Gtk::Orientation orientation, int for_size, int& minimum, int& natural, int& minimum_baseline, int& natural_baseline) const {

    auto delta = prop_digits > 0 ? pow(10.0, -prop_digits) : 0;
    auto low = format(_adjustment->get_lower() + delta, true, false, true);
    auto high = format(_adjustment->get_upper() - delta, true, false, true);
    auto text = low.size() > high.size() ? low : high;

    // http://developer.gnome.org/pangomm/unstable/classPango_1_1Layout.html
    auto layout = const_cast<InkSpinButton*>(this)->create_pango_layout("\u2009" + text + "\u2009");

    int text_width = 0;
    int text_height = 0;
    // get the text dimensions
    layout->get_pixel_size(text_width, text_height);

    if (orientation == Gtk::Orientation::HORIZONTAL) {
        minimum_baseline = natural_baseline = -1;
        // always measure, so gtk doesn't complain
        auto m = _minus.measure(orientation);
        auto p = _plus.measure(orientation);
        auto _ = _entry.measure(orientation);
        _ = _value.measure(orientation);

        // always reserve space for inc/dec buttons:
        minimum = natural = text_width + 2 * _buttons_width;
    }
    else {
        minimum_baseline = natural_baseline = _baseline;
        auto height = std::max(text_height, _entry_height);
        minimum = height;
        natural = int(1.5 * height);
    }
}

void InkSpinButton::size_allocate_vfunc(int width, int height, int baseline) {
    Gtk::Allocation allocation;
    allocation.set_height(height);
    allocation.set_width(_buttons_width);
    allocation.set_x(0);
    allocation.set_y(0);

    int left = 0;
    int right = width;

    if (_minus.get_visible()) {
        _minus.size_allocate(allocation, baseline);
        left += allocation.get_width();
    }
    if (_plus.get_visible()) {
        allocation.set_x(width - allocation.get_width());
        _plus.size_allocate(allocation, baseline);
        right -= allocation.get_width();
    }

    allocation.set_x(left);
    allocation.set_width(right - left);
    if (_value.get_visible()) {
        _value.size_allocate(allocation, baseline);
    }
    if (_entry.get_visible()) {
        _entry.size_allocate(allocation, baseline);
    }
}


Glib::RefPtr<Gtk::Adjustment>& InkSpinButton::get_adjustment() {
    return _adjustment;
}

void InkSpinButton::set_adjustment(const Glib::RefPtr<Gtk::Adjustment>& adjustment) {
    if (!adjustment) return;

    _connection.disconnect();
    _adjustment = adjustment;
    _connection = _adjustment->signal_value_changed().connect(sigc::mem_fun(*this, &InkSpinButton::update));
    update();
}

void InkSpinButton::set_digits(int digits) {
    prop_digits = digits;
    // prop_digits.set_value(digits);
    update();
}

int InkSpinButton::get_digits() const {
    return prop_digits;
    // return prop_digits.get_value();
}

void InkSpinButton::set_prefix(const std::string& prefix, bool add_space) {
    if (add_space && !prefix.empty()) {
        _prefix = prefix + " ";
    }
    else {
        _prefix = prefix;
    }
    update();
}

void InkSpinButton::set_suffix(const std::string& suffix, bool add_half_space) {
    if (add_half_space && !suffix.empty()) {
        // thin space
        _suffix = "\u2009" + suffix;
    }
    else {
        _suffix = suffix;
    }
    update();
}

void InkSpinButton::set_has_frame(bool frame) {
    if (frame) {
        add_css_class("frame");
    }
    else {
        remove_css_class("frame");
    }
}

void InkSpinButton::set_trim_zeros(bool trim) {
    if (_trim_zeros != trim) {
        _trim_zeros = trim;
        update();
    }
}

static void trim_zeros(std::string& ret) {
    while (ret.find('.') != std::string::npos &&
        (ret.substr(ret.length() - 1, 1) == "0" || ret.substr(ret.length() - 1, 1) == ".")) {
        ret.pop_back();
    }
}

std::string InkSpinButton::format(double value, bool with_prefix_suffix, bool with_markup, bool trim_zeros) const {
    std::stringstream ss;
    ss.imbue(std::locale("C"));
    ss << std::fixed << std::setprecision(prop_digits) << value;
    auto number = ss.str();
    if (trim_zeros) {
        UI::Widget::trim_zeros(number);
    }

    if (with_prefix_suffix && (!_suffix.empty() || !_prefix.empty())) {
        if (with_markup) {
            std::stringstream markup;
            if (!_prefix.empty()) {
                markup << "<span alpha='50%'>" << _prefix << "</span>";
            }
            markup << "<span>" << number << "</span>";
            if (!_suffix.empty()) {
                markup << "<span alpha='50%'>" << _suffix << "</span>";
            }
            return markup.str();
        }
        else {
            return _prefix + number + _suffix;
        }
    }

    return number;
}

void InkSpinButton::update() {
    if (!_adjustment) return;

    auto value = _adjustment->get_value();
    auto text = format(value, false, false, _trim_zeros);
    _entry.set_text(text);
    if (_suffix.empty() && _prefix.empty()) {
        _value.set_text(text);
    }
    else {
        _value.set_markup(format(value, true, true, _trim_zeros));
    }

    _minus.set_sensitive(_adjustment->get_value() > _adjustment->get_lower());
    _plus.set_sensitive(_adjustment->get_value() < _adjustment->get_upper());
}

// ---------------- CONTROLLERS -----------------

// ------------------  MOTION  ------------------

void InkSpinButton::on_motion_enter(double x, double y) {
    if (_focus->contains_focus()) return;

    show_arrows(true);
}

void InkSpinButton::on_motion_leave() {
    if (_focus->contains_focus()) return;

    show_arrows(false);

    if (_entry.is_visible()) {
        // We left spinbutton, save value and update.
        commit_entry();
        exit_edit();
    }
}

// ---------------  MOTION VALUE  ---------------

void InkSpinButton::on_motion_enter_value(double x, double y) {
    _old_cursor = get_cursor();
    if (!g_resizing_cursor) {
        g_resizing_cursor = Gdk::Cursor::create(Glib::ustring("ew-resize"));
    }
    _current_cursor = g_resizing_cursor;
    set_cursor(_current_cursor);
}

void InkSpinButton::on_motion_leave_value() {
    _current_cursor = _old_cursor;
    set_cursor(_current_cursor);
}

// ---------------   DRAG VALUE  ----------------

static double get_accel_factor(Gdk::ModifierType state) {
    double scale = 1.0;
    // Ctrl modifier slows down, Shift speeds up
    if ((state & Gdk::ModifierType::CONTROL_MASK) == Gdk::ModifierType::CONTROL_MASK) {
        scale = 0.1;
    } else if ((state & Gdk::ModifierType::SHIFT_MASK) == Gdk::ModifierType::SHIFT_MASK) {
        scale = 10.0;
    }
    return scale;
}

void InkSpinButton::on_drag_begin_value(Gdk::EventSequence* sequence) {
    _initial_value = _adjustment->get_value();
}

void InkSpinButton::on_drag_update_value(Gdk::EventSequence* sequence) {
    double dx = 0.0;
    double dy = 0.0;
    _drag_value->get_offset(dx, dy);

    // If we don't move, then it probably was a button click.
    auto delta = 1; // tweak this value to reject real clicks, or else we'll change value inadvertently
    if (std::abs(dx) > delta || std::abs(dy) > delta) {
        auto max_dist = 300.0; // distance to travel to adjust full range
        auto range = _adjustment->get_upper() - _adjustment->get_lower();
        auto state = _drag_value->get_current_event_state();
        auto distance = std::hypot(dx, dy);
        auto angle = std::atan2(dx, dy);
        auto grow = angle > M_PI_4 || angle < -M_PI+M_PI_4;
        if (!grow) distance = -distance;
// printf("drag: %f, %f, angle: %f,  sin: %f,  cos: %f, mul: %f\n", dx, dy, angle, sin(angle), 0.0);
        auto value = _initial_value + get_accel_factor(state) * distance / max_dist * range + _adjustment->get_lower();
        set_value(value);
        _dragged = true;
    }
}

void InkSpinButton::on_drag_end_value(Gdk::EventSequence* sequence) {
    double dx = 0.0;
    double dy = 0.0;
    _drag_value->get_offset(dx, dy);

    if (dx == 0 && !_dragged) {
        // Must have been a click!
        enter_edit();
    }
    _dragged = false;
}

void InkSpinButton::show_arrows(bool on) {
    _minus.set_visible(on);
    _plus.set_visible(on);
}

bool InkSpinButton::commit_entry() {
    try {
        double value = 0.0;
        auto text = get_text(_entry);
        if (_dont_evaluate) {
            value = std::stod(text);
        } else {
            value = Util::ExpressionEvaluator{text}.evaluate().value;
        }
        _adjustment->set_value(value);
        return true;
    }
    catch (const std::exception& e) {
        g_message("Expression error: %s", e.what());
    }
    return false;
}

void InkSpinButton::exit_edit() {
    _entry.hide();
    _minus.hide();
    _value.show();
    _plus.hide();
}

inline void InkSpinButton::enter_edit() {
    show_arrows(false);
    _value.hide();
    _entry.select_region(0, _entry.get_text_length());
    _entry.show();
    // postpone it, it won't work immediately:
    Glib::signal_idle().connect_once([this](){_entry.grab_focus();}, Glib::PRIORITY_HIGH_IDLE);
}

bool InkSpinButton::defocus() {
    if (_focus->contains_focus()) {
        // move focus away
        if (_defocus_widget) {
            if (_defocus_widget->grab_focus()) return true;
        }
        for (auto widget = this->get_next_sibling(); widget; widget = widget->get_next_sibling()) {
            if (widget != this && widget->get_can_focus()) {
                if (widget->grab_focus()) return true;
            }
        }
        for (auto widget = this->get_prev_sibling(); widget; widget = widget->get_prev_sibling()) {
            if (widget != this && widget->get_can_focus()) {
                if (widget->grab_focus()) return true;
            }
        }
    }
    return false;
}

// ------------------  SCROLL  ------------------

void InkSpinButton::on_scroll_begin() {
    _scroll_counter = 0;
    set_cursor("none");
}

bool InkSpinButton::on_scroll(double dx, double dy) {
    // grow direction: up or right
    auto delta = std::abs(dx) > std::abs(dy) ? -dx : dy;
    _scroll_counter += delta;
    // this is a threshold to control rate at which scrolling increments/decrements current value;
    // the larger the threshold, the slower the rate; it may need to be tweaked on different platforms
#ifdef _WIN32
    // default for mouse wheel on windows
    constexpr double threshold = 1.0;
#elif defined __APPLE__
    // scrolling is very sensitive on macOS
    constexpr double threshold = 5.0;
#else
    //todo: default for Linux
    constexpr double threshold = 1.0;
#endif
    if (std::abs(_scroll_counter) >= threshold) {
        auto inc = std::round(_scroll_counter / threshold);
        _scroll_counter = 0;
        auto state = _scroll->get_current_event_state();
        change_value(inc, state);
    }
    return true;
}

void InkSpinButton::on_scroll_end() {
    _scroll_counter = 0;
    set_cursor(_current_cursor);
}

void InkSpinButton::set_value(double new_value) {
    _adjustment->set_value(new_value);
}

void InkSpinButton::change_value(double inc, Gdk::ModifierType state) {
    double scale = get_accel_factor(state);
    set_value(_adjustment->get_value() + _adjustment->get_step_increment() * scale * inc);
}

// ------------------   KEY    ------------------

bool InkSpinButton::on_key_pressed(guint keyval, guint keycode, Gdk::ModifierType state) {
   switch (keyval) {
     case GDK_KEY_Escape: // Defocus
         //todo: should Esc undo?
         return defocus();

   // signal "activate" uses this key, so we won't see it:
   // case GDK_KEY_Return:
       // break;

   case GDK_KEY_Up:
       change_value(1, state);
       return true;

   case GDK_KEY_Down:
       change_value(-1, state);
       return true;

   default:
     break;
   }

   return false;
}

// ------------------  CLICK   ------------------

void InkSpinButton::on_pressed_plus(int n_press, double x, double y) {
    auto state = _click_plus->get_current_event_state();
    double inc = (state & Gdk::ModifierType::BUTTON3_MASK) == Gdk::ModifierType::BUTTON3_MASK ? 5 : 1;
    change_value(inc, state);
    start_spinning(inc, state, _click_plus);
}

void InkSpinButton::on_pressed_minus(int n_press, double x, double y) {
    auto state = _click_minus->get_current_event_state();
    double inc = (state & Gdk::ModifierType::BUTTON3_MASK) == Gdk::ModifierType::BUTTON3_MASK ? 5 : 1;
    change_value(-inc, state);
    start_spinning(-inc, state, _click_minus);
}

void InkSpinButton::on_activate() {
    commit_entry();
}

void InkSpinButton::on_changed() {
    // NOT USED
}

void InkSpinButton::on_editing_done() {
    // NOT USED
}

void InkSpinButton::start_spinning(double steps, Gdk::ModifierType state, Glib::RefPtr<Gtk::GestureClick>& gesture) {

    _spinning = Glib::signal_timeout().connect([=,this]() {
        change_value(steps, state);
        // speed up
        _spinning = Glib::signal_timeout().connect([=,this]() {
            change_value(steps, state);
            //TODO: find a way to read mouse button state
            auto active = gesture->is_active();
            auto btn = gesture->get_current_button();
            if (!active || !btn) return false;
            return true;
        }, timeout_repeat);
        return false;
    }, timeout_click);
}

void InkSpinButton::stop_spinning() {
    if (_spinning) _spinning.disconnect();
}

void InkSpinButton::set_drag_sensitivity(double distance) {
    _drag_full_travel = distance;
}

} // namespace Inkscape::UI::Widget
