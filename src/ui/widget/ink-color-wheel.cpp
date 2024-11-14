//   SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * HSLuv color wheel widget, based on the web implementation at
 * https://www.hsluv.org
 *//*
 * Authors:
 *   Tavmjong Bah
 *   Massinissa Derriche <massinissa.derriche@gmail.com>
 *
 * Copyright (C) 2018, 2021 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "ui/widget/ink-color-wheel.h"

#include <2geom/angle.h>
#include <2geom/coord.h>
#include <algorithm>
#include <cstring>
#include <gdkmm/display.h>
#include <gdkmm/general.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/eventcontrollerkey.h>
#include <gtkmm/eventcontrollermotion.h>
#include <gtkmm/gestureclick.h>
#include <sigc++/functors/mem_fun.h>
#include <utility>

#include "colors/spaces/enum.h"
#include "colors/spaces/hsluv.h"
#include "colors/utils.h"
#include "ui/controller.h"
#include "ui/dialog/color-item.h"
#include "ui/util.h"
#include "ui/widget/bin.h"

using namespace Inkscape::Colors;
using Inkscape::Colors::Space::Luv;
using Inkscape::Colors::Space::Type;

namespace Inkscape::UI::Widget {

// Sizes in pixels
constexpr static int const SIZE = 400;
constexpr static int const OUTER_CIRCLE_RADIUS = 190;
constexpr static double MAX_HUE = 360.0;
constexpr static double MAX_SATURATION = 100.0;
constexpr static double MAX_LIGHTNESS = 100.0;
constexpr static double MIN_HUE = 0.0;
constexpr static double MIN_SATURATION = 0.0;
constexpr static double MIN_LIGHTNESS = 0.0;
constexpr static double OUTER_CIRCLE_DASH_SIZE = 10.0;
constexpr static double VERTEX_EPSILON = 0.01;
constexpr static double marker_radius = 4.0;
constexpr static double focus_line_width = 1.0;
constexpr static double focus_padding = 3.0;
static auto const focus_dash = std::vector{1.5};

/** Represents a vertex of the Luv color polygon (intersection of bounding lines). */
struct Intersection final
{
    Intersection();

    Intersection(int line_1, int line_2, Geom::Point &&intersection_point, Geom::Angle start_angle)
        : line1{line_1}
        , line2{line_2}
        , point{intersection_point}
        , polar_angle{point}
        , relative_angle{polar_angle - start_angle}
    {
    }

    int line1 = 0; ///< Index of the first of the intersecting lines.
    int line2 = 0; ///< Index of the second of the intersecting lines.
    Geom::Point point; ///< The geometric position of the intersection.
    Geom::Angle polar_angle = 0.0; ///< Polar angle of the point (in radians).
    /** Angle relative to the polar angle of the point at which the boundary of the polygon
     *  passes the origin at the minimum distance (i.e., where an expanding origin-centered
     *  circle inside the polygon starts touching an edge of the polygon.)
     */
    Geom::Angle relative_angle = 0.0;
};

static double lerp(double v0, double v1, double t0, double t1, double t);
static ColorPoint lerp(ColorPoint const &v0, ColorPoint const &v1, double t0, double t1, double t);
static double luminance(Color const &color);
static Geom::Point to_pixel_coordinate(Geom::Point const &point, double scale, double resize);
static Geom::Point from_pixel_coordinate(Geom::Point const &point, double scale, double resize);
static std::vector<Geom::Point> to_pixel_coordinate(std::vector<Geom::Point> const &points, double scale,
                                                    double resize);
static void draw_vertical_padding(ColorPoint p0, ColorPoint p1, int padding, bool pad_upwards, guint32 *buffer,
                                  int height, int stride);

/* Base Color Wheel */

ColorWheelBase::ColorWheelBase(Type type, std::vector<double> initial_color)
    : Gtk::AspectFrame(0.5, 0.5, 1.0, false)
    , _bin{Gtk::make_managed<UI::Widget::Bin>()}
    , _values{type, std::move(initial_color)}
    , _drawing_area{Gtk::make_managed<Gtk::DrawingArea>()} {
    construct();
}

void ColorWheelBase::construct() {
    set_name("ColorWheel");
    add_css_class("flat");

    _drawing_area->set_focusable(true);
    _drawing_area->set_expand(true);
    _bin->connectAfterResize(sigc::mem_fun(*this, &ColorWheelBase::on_drawing_area_size));
    _drawing_area->set_draw_func(sigc::mem_fun(*this, &ColorWheelBase::on_drawing_area_draw ));
    _drawing_area->property_has_focus().signal_changed().connect([this]{ _drawing_area->queue_draw(); });
    _bin->set_child(_drawing_area);
    set_child(*_bin);

    auto const click = Gtk::GestureClick::create();
    click->set_button(0); // any
    click->signal_pressed().connect(Controller::use_state([this](auto &&...args) { return on_click_pressed(args...); }, *click));
    click->signal_released().connect(Controller::use_state([this](auto &, auto &&...args) { return on_click_released(args...); }, *click));
    _drawing_area->add_controller(click);

    auto const motion = Gtk::EventControllerMotion::create();
    motion->signal_motion().connect([this, &motion = *motion](auto &&...args) { return _on_motion(motion, args...); });
    _drawing_area->add_controller(motion);

    auto const key = Gtk::EventControllerKey::create();
    key->signal_key_pressed().connect(sigc::mem_fun(*this, &ColorWheelBase::on_key_pressed), true);
    key->signal_key_released().connect(sigc::mem_fun(*this, &ColorWheelBase::on_key_released));
    _drawing_area->add_controller(key);
}

ColorWheelBase::ColorWheelBase(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder, Type type, std::vector<double> initial_color):
    Gtk::AspectFrame(cobject),
    _bin(Gtk::make_managed<Bin>()),
    _values(type, initial_color),
    _drawing_area{Gtk::make_managed<Gtk::DrawingArea>()} {

    construct();
}

void ColorWheelBase::_on_motion(Gtk::EventControllerMotion const &motion, double x, double y) {
    if (!_adjusting) return;

    auto state = motion.get_current_event_state();
    if (!Controller::has_flag(state, Gdk::ModifierType::BUTTON1_MASK)) {
        // lost button release event
        on_click_released(0, x, y);
        return;
    }

    on_motion(motion, x, y);
}

sigc::connection ColorWheelBase::connect_color_changed(sigc::slot<void ()> slot)
{
    return _signal_color_changed.connect(std::move(slot));
}

void ColorWheelBase::color_changed()
{
    _signal_color_changed.emit();
    _drawing_area->queue_draw();
}

void ColorWheelBase::queue_drawing_area_draw()
{
    _drawing_area->queue_draw();
}

Gtk::Allocation ColorWheelBase::get_drawing_area_allocation() const
{
    return _drawing_area->get_allocation();
}

bool ColorWheelBase::drawing_area_has_focus() const
{
    return _drawing_area->has_focus();
}

void ColorWheelBase::focus_drawing_area()
{
    _drawing_area->grab_focus();
}

void ColorWheelBase::on_key_released(unsigned keyval, unsigned /*keycode*/, Gdk::ModifierType state)
{
    switch (keyval) {
        case GDK_KEY_Up:
        case GDK_KEY_KP_Up:
        case GDK_KEY_Down:
        case GDK_KEY_KP_Down:
        case GDK_KEY_Left:
        case GDK_KEY_KP_Left:
        case GDK_KEY_Right:
        case GDK_KEY_KP_Right:
            _adjusting = false;
    }
}

/* HSL Color Wheel */

bool ColorWheelHSL::setColor(Color const &color,
                           bool const /*overrideHue*/, bool const emit)
{
    if (_values.set(color, true)) {
        _triangle_corners.reset();
        _marker_point.reset();
        if (emit)
            color_changed();
        else
            queue_drawing_area_draw();
        return true;
    }
    return false;
}

void ColorWheelHSL::update_ring_source()
{
    if (_radii && _source_ring) return;

    auto const [width, height] = *_cache_size;
    auto const cx = width  / 2.0;
    auto const cy = height / 2.0;

    auto const stride = Cairo::ImageSurface::format_stride_for_width(Cairo::Surface::Format::RGB24, width);
    _source_ring.reset();
    _buffer_ring.resize(height * stride / 4);

    auto const &[r_min, r_max] = get_radii();
    double r2_max = (r_max+2) * (r_max+2); // Must expand a bit to avoid edge effects.
    double r2_min = (r_min-2) * (r_min-2); // Must shrink a bit to avoid edge effects.

    for (int i = 0; i < height; ++i) {
        auto p = _buffer_ring.data() + i * width;
        double dy = (cy - i);

        for (int j = 0; j < width; ++j) {
            double dx = (j - cx);
            double r2 = dx * dx + dy * dy;
            if (r2 < r2_min || r2 > r2_max) {
                *p++ = 0; // Save calculation time.
            } else {
                double angle = atan2 (dy, dx);
                if (angle < 0.0) {
                    angle += 2.0 * M_PI;
                }
                double hue = angle/(2.0 * M_PI);
                *p++ = Color(Type::HSV, {hue, 1.0, 1.0}).toARGB();
            }
        }
    }

    auto const data = reinterpret_cast<unsigned char *>(_buffer_ring.data());
    _source_ring = Cairo::ImageSurface::create(data,
                                               Cairo::Surface::Format::RGB24,
                                               width, height, stride);
}

ColorWheelHSL::TriangleCorners
ColorWheelHSL::update_triangle_source()
{
    bool const source_is_stale = !_triangle_corners.has_value();

    // Reorder so we paint from top down.
    auto ps = get_triangle_corners();
    std::sort(ps.begin(), ps.end(), [](auto const &l, auto const &r){ return l.y < r.y; });
    auto const &[p0, p1, p2] = ps;

    if (_source_triangle && !source_is_stale) return {p0, p1, p2};

    /* The triangle is painted by first finding color points on the
     * edges of the triangle at the same y value via linearly
     * interpolating between corner values, and then interpolating along
     * x between the those edge points. The interpolation is in sRGB
     * space which leads to a complicated mapping between x/y and
     * saturation/value. This was probably done to remove the need to
     * convert between HSV and RGB for each pixel.
     * Black corner: v = 0, s = 1
     * White corner: v = 1, s = 0
     * Color corner; v = 1, s = 1
     */
    constexpr int padding = 3; // Avoid edge artifacts.

    _source_triangle.reset();
    auto const [width, height] = *_cache_size;
    auto const stride = Cairo::ImageSurface::format_stride_for_width(Cairo::Surface::Format::RGB24, width);
    _buffer_triangle.resize(height * stride / 4);

    for (int y = 0; y < height; ++y) {
        if (p0.y <= y + padding && y - padding < p2.y) {
            // Get values on side at position y.
            ColorPoint side0;
            double y_inter = std::clamp(static_cast<double>(y), p0.y, p2.y);
            if (y < p1.y) {
                side0 = lerp(p0, p1, p0.y, p1.y, y_inter);
            } else {
                side0 = lerp(p1, p2, p1.y, p2.y, y_inter);
            }

            ColorPoint side1 = lerp(p0, p2, p0.y, p2.y, y_inter);

            // side0 should be on left
            if (side0.x > side1.x) {
                std::swap (side0, side1);
            }

            int const x_start = std::max(0, static_cast<int>(side0.x));
            int const x_end   = std::min(static_cast<int>(side1.x), width);

            auto p = _buffer_triangle.data() + y * (stride / 4);
            int x = 0;
            for (; x <= x_start; ++x) {
                *p++ = side0.color.toARGB();
            }
            for (; x < x_end; ++x) {
                *p++ = lerp(side0, side1, side0.x, side1.x, x).color.toARGB();
            }
            for (; x < width; ++x) {
                *p++ = side1.color.toARGB();
            }
        }
    }

    // add vertical padding to each side separately

    ColorPoint temp_point = lerp(p0, p1, p0.x, p1.x, (p0.x + p1.x) / 2.0);
    bool pad_upwards = _is_in_triangle(temp_point.x, temp_point.y + 1);
    draw_vertical_padding(p0, p1, padding, pad_upwards, _buffer_triangle.data(), height, stride / 4);

    temp_point = lerp(p0, p2, p0.x, p2.x, (p0.x + p2.x) / 2.0);
    pad_upwards = _is_in_triangle(temp_point.x, temp_point.y + 1);
    draw_vertical_padding(p0, p2, padding, pad_upwards, _buffer_triangle.data(), height, stride / 4);

    temp_point = lerp(p1, p2, p1.x, p2.x, (p1.x + p2.x) / 2.0);
    pad_upwards = _is_in_triangle(temp_point.x, temp_point.y + 1);
    draw_vertical_padding(p1, p2, padding, pad_upwards, _buffer_triangle.data(), height, stride / 4);

    auto const data = reinterpret_cast<unsigned char *>(_buffer_triangle.data());
    _source_triangle = Cairo::ImageSurface::create(data,
                                                   Cairo::Surface::Format::RGB24,
                                                   width, height, stride);

    return {p0, p1, p2};
}

void ColorWheelHSL::on_drawing_area_size(int width, int height, int baseline)
{
    auto const size = Geom::IntPoint{width, height};
    if (size == _cache_size) return;
    _cache_size = size;
    _radii.reset();
    _source_ring.reset();
}

void ColorWheelHSL::on_drawing_area_draw(Cairo::RefPtr<Cairo::Context> const &cr, int, int)
{
    auto const [width, height] = *_cache_size;
    auto const cx = width  / 2.0;
    auto const cy = height / 2.0;

    cr->set_antialias(Cairo::ANTIALIAS_SUBPIXEL);

    // Update caches
    update_ring_source();
    auto const &[p0, p1, p2] = update_triangle_source();
    auto const &[r_min, r_max] = get_radii();

    // Paint with ring surface, clipping to ring.
    cr->save();
    cr->set_source(_source_ring, 0, 0);
    cr->set_line_width (r_max - r_min);
    cr->begin_new_path();
    cr->arc(cx, cy, (r_max + r_min)/2.0, 0, 2.0 * M_PI);
    cr->stroke();
    cr->restore();
    // Paint line on ring
    auto color_on_ring = Color(Type::HSV, {_values[0], 1.0, 1.0});
    double l = luminance(color_on_ring) < 0.5 ? 1.0 : 0.0;
    cr->save();
    cr->set_source_rgb(l, l, l);
    cr->move_to(cx + cos(_values[0] * M_PI * 2.0) * (r_min + 1),
                cy - sin(_values[0] * M_PI * 2.0) * (r_min + 1));
    cr->line_to(cx + cos(_values[0] * M_PI * 2.0) * (r_max - 1),
                cy - sin(_values[0] * M_PI * 2.0) * (r_max - 1));
    cr->stroke();
    cr->restore();

    // Paint with triangle surface, clipping to triangle.
    cr->save();
    cr->set_source(_source_triangle, 0, 0);
    cr->move_to(p0.x, p0.y);
    cr->line_to(p1.x, p1.y);
    cr->line_to(p2.x, p2.y);
    cr->close_path();
    cr->fill();
    cr->restore();

    // Draw marker
    auto const &[mx, my] = get_marker_point();
    double a = luminance(getColor()) < 0.5 ? 1.0 : 0.0;
    cr->set_source_rgb(a, a, a);
    cr->begin_new_path();
    cr->arc(mx, my, marker_radius, 0, 2 * M_PI);
    cr->stroke();

    // Draw focus
    if (drawing_area_has_focus()) {
        // The focus_dash width & alpha(foreground_color) are from GTK3 Adwaita.
        cr->set_dash(focus_dash, 0);
        cr->set_line_width(1.0);

        if (_focus_on_ring) {
            auto const rgba = change_alpha(Widget::get_color(), 0.7);
            Gdk::Cairo::set_source_rgba(cr, rgba);
            cr->begin_new_path();
            cr->rectangle(0, 0, width, height);
        } else {
            cr->set_source_rgb(1 - a, 1 - a, 1 - a);
            cr->begin_new_path();
            cr->arc(mx, my, marker_radius + focus_padding, 0, 2 * M_PI);
        }

        cr->stroke();
    }
}

std::optional<bool> ColorWheelHSL::focus(Gtk::DirectionType const direction)
{
    // Any focus change must update focus indicators (add or remove).
    queue_drawing_area_draw();

    // In forward direction, focus passes from no focus to ring focus to triangle
    // focus to no focus.
    if (!drawing_area_has_focus()) {
        _focus_on_ring = (direction == Gtk::DirectionType::TAB_FORWARD);
        focus_drawing_area();
        return true;
    }

    // Already have focus
    bool keep_focus = true;

    switch (direction) {
        case Gtk::DirectionType::TAB_BACKWARD:
            if (!_focus_on_ring) {
                _focus_on_ring = true;
            } else {
                keep_focus = false;
            }
            break;

        case Gtk::DirectionType::TAB_FORWARD:
            if (_focus_on_ring) {
                _focus_on_ring = false;
            } else {
                keep_focus = false;
            }
    }

    return keep_focus;
}

bool ColorWheelHSL::_set_from_xy(double const x, double const y)
{
    auto const [width, height] = *_cache_size;
    double const cx = width/2.0;
    double const cy = height/2.0;

    double const r = std::min(cx, cy) * (1 - _ring_width);

    // We calculate RGB value under the cursor by rotating the cursor
    // and triangle by the hue value and looking at position in the
    // now right pointing triangle.
    double angle = _values[0]  * 2 * M_PI;
    double sin = std::sin(angle);
    double cos = std::cos(angle);
    double xp =  ((x - cx) * cos - (y - cy) * sin) / r;
    double yp =  ((x - cx) * sin + (y - cy) * cos) / r;

    double xt = lerp(0.0, 1.0, -0.5, 1.0, xp);
    xt = std::clamp(xt, 0.0, 1.0);

    double dy = (1-xt) * std::cos(M_PI / 6.0);
    double yt = lerp(0.0, 1.0, -dy, dy, yp);
    yt = std::clamp(yt, 0.0, 1.0);

    ColorPoint c0(0, 0, Color(Type::RGB, {yt, yt, yt}));      // Grey point along base.
    ColorPoint c1(0, 0, Color(Type::HSV, {_values[0], 1, 1})); // Hue point at apex
    ColorPoint c = lerp(c0, c1, 0, 1, xt);
    c.color.setOpacity(_values.getOpacity()); // Remember opacity
    return setColor(c.color, false); // Don't override previous hue.
}

bool ColorWheelHSL::set_from_xy_delta(double const dx, double const dy)
{
    auto [mx, my] = get_marker_point();
    mx += dx;
    my += dy;
    return _set_from_xy(mx, my);
}

bool ColorWheelHSL::_is_in_ring(double x, double y)
{
    auto const [width, height] = *_cache_size;
    auto const cx = width  / 2.0;
    auto const cy = height / 2.0;

    auto const &[r_min, r_max] = get_radii();
    double r2_max = r_max * r_max;
    double r2_min = r_min * r_min;

    double dx = x - cx;
    double dy = y - cy;
    double r2 = dx * dx + dy * dy;

    return (r2_min < r2 && r2 < r2_max);
}

bool ColorWheelHSL::_is_in_triangle(double x, double y)
{
    auto const &[p0, p1, p2] = get_triangle_corners();
    auto const &[x0, y0] = p0.get_xy();
    auto const &[x1, y1] = p1.get_xy();
    auto const &[x2, y2] = p2.get_xy();

    double det = (x2 - x1) * (y0 - y1) - (y2 - y1) * (x0 - x1);
    double s = ((x - x1) * (y0 - y1) - (y - y1) * (x0 - x1)) / det;
    if (s < 0.0) return false;

    double t = ((x2 - x1) * (y - y1) - (y2 - y1) * (x - x1)) / det;
    return (t >= 0.0 && s + t <= 1.0);
}

void ColorWheelHSL::_update_ring_color(double x, double y)
{
    auto const [width, height] = *_cache_size;
    double cx = width / 2.0;
    double cy = height / 2.0;

    double angle = -atan2(y - cy, x - cx);
    if (angle < 0) {
        angle += 2.0 * M_PI;
    }
    angle /= 2.0 * M_PI;

    if (_values.set(0, angle)) {
        _triangle_corners.reset();
        color_changed();
    }
}

Gtk::EventSequenceState ColorWheelHSL::on_click_pressed(Gtk::GestureClick const &controller,
                                                        int /*n_press*/, double x, double y)
{
    if (_is_in_ring(x, y) ) {
        _adjusting = true;
        _mode = DragMode::HUE;
        focus_drawing_area();
        _focus_on_ring = true;
        _update_ring_color(x, y);
        return Gtk::EventSequenceState::CLAIMED;
    } else if (_is_in_triangle(x, y)) {
        _adjusting = true;
        _mode = DragMode::SATURATION_VALUE;
        focus_drawing_area();
        _focus_on_ring = false;
        _set_from_xy(x, y);
        return Gtk::EventSequenceState::CLAIMED;
    }

    return Gtk::EventSequenceState::NONE;
}

Gtk::EventSequenceState ColorWheelHSL::on_click_released(int /*n_press*/, double /*x*/, double /*y*/)
{
    _mode = DragMode::NONE;
    _adjusting = false;
    return Gtk::EventSequenceState::CLAIMED;
}

void ColorWheelHSL::on_motion(Gtk::EventControllerMotion const &motion, double x, double y)
{
    if (!_adjusting) return;
    auto state = motion.get_current_event_state();
    if (!Controller::has_flag(state, Gdk::ModifierType::BUTTON1_MASK)) {
        // lost button release event
        _mode = DragMode::NONE;
        _adjusting = false;
        return;
    }

    if (_mode == DragMode::HUE) {
        _update_ring_color(x, y);
    } else if (_mode == DragMode::SATURATION_VALUE) {
        _set_from_xy(x, y);
    }
}

bool ColorWheelHSL::on_key_pressed(unsigned keyval, unsigned /*keycode*/, Gdk::ModifierType state)
{
    static constexpr double delta_hue = 2.0 / MAX_HUE;
    auto dx = 0.0, dy = 0.0;

    switch (keyval) {
        case GDK_KEY_Up:
        case GDK_KEY_KP_Up:
            dy = -1.0;
            break;

        case GDK_KEY_Down:
        case GDK_KEY_KP_Down:
            dy = +1.0;
            break;

        case GDK_KEY_Left:
        case GDK_KEY_KP_Left:
            dx = -1.0;
            break;

        case GDK_KEY_Right:
        case GDK_KEY_KP_Right:
            dx = +1.0;
    }

    if (dx == 0.0 && dy == 0.0) return false;

    bool changed = false;
    if (_focus_on_ring) {
        changed = _values.set(0, _values[0] - ((dx != 0 ? dx : dy) * delta_hue));
    } else {
        changed = set_from_xy_delta(dx, dy);
    }

    _values.normalize();

    if (changed) {
        color_changed();
    }

    return changed;
}

ColorWheelHSL::MinMax const &ColorWheelHSL::get_radii()
{
    if (_radii) return *_radii;

    // Force calc others, too.
    _triangle_corners.reset();

    _radii.emplace();
    auto &[r_min, r_max] = *_radii;
    auto const [width, height] = *_cache_size;
    r_max = std::min(width, height) / 2.0 - 2 * (focus_line_width + focus_padding);
    r_min = r_max * (1.0 - _ring_width);
    return *_radii;
}

std::array<ColorPoint, 3> const &ColorWheelHSL::get_triangle_corners()
{
    if (_triangle_corners) return *_triangle_corners;

    auto const [width, height] = *_cache_size;
    double const cx = width  / 2.0;
    double const cy = height / 2.0;

    auto const &[r_min, r_max] = get_radii();
    double angle = _values[0] * 2.0 * M_PI;
    auto const add2 = 2.0 * M_PI / 3.0;
    auto const angle2 = angle  + add2;
    auto const angle4 = angle2 + add2;

    // Force calc this too
    _marker_point.reset();

    _triangle_corners.emplace();
    auto &[p0, p1, p2] = *_triangle_corners;
    auto const x0 = cx + std::cos(angle ) * r_min;
    auto const y0 = cy - std::sin(angle ) * r_min;
    auto const x1 = cx + std::cos(angle2) * r_min;
    auto const y1 = cy - std::sin(angle2) * r_min;
    auto const x2 = cx + std::cos(angle4) * r_min;
    auto const y2 = cy - std::sin(angle4) * r_min;
    p0 = {x0, y0, Color(Type::HSV, {_values[0], 1.0, 1.0})};
    p1 = {x1, y1, Color(Type::HSV, {_values[0], 1.0, 0.0})};
    p2 = {x2, y2, Color(Type::HSV, {_values[0], 0.0, 1.0})};
    return *_triangle_corners;
}

Geom::Point const &ColorWheelHSL::get_marker_point()
{
    if (_marker_point) return *_marker_point;

    auto const &[p0, p1, p2] = get_triangle_corners();
    auto const &[x0, y0] = p0.get_xy();
    auto const &[x1, y1] = p1.get_xy();
    auto const &[x2, y2] = p2.get_xy();

    _marker_point.emplace();
    auto &[mx, my] = *_marker_point;
    auto const v1v2 = _values[1] * _values[2];
    mx = x1 + (x2 - x1) * _values[2] + (x0 - x2) * v1v2;
    my = y1 + (y2 - y1) * _values[2] + (y0 - y2) * v1v2;
    return *_marker_point;
}

ColorWheelHSL::ColorWheelHSL()
    : Glib::ObjectBase{"ColorWheelHSL"}
    , WidgetVfuncsClassInit{}
    // All the calculations are based on HSV, not HSL
    , ColorWheelBase(Type::HSV, {0, 0, 0, 1})
{}

ColorWheelHSL::ColorWheelHSL(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder) :
    ColorWheelBase(cobject, builder, Type::HSV, {0, 0, 0, 1}) {}

/* HSLuv Color Wheel */

ColorWheelHSLuv::ColorWheelHSLuv()
    : ColorWheelBase(Type::HSLUV, {0, 1, 0.5, 1})
{
    _picker_geometry = std::make_unique<PickerGeometry>();
}

ColorWheelHSLuv::ColorWheelHSLuv(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder) :
    ColorWheelBase(cobject, builder, Type::HSLUV, {0, 1, 0.5, 1}) {

    _picker_geometry = std::make_unique<PickerGeometry>();
}

bool ColorWheelHSLuv::setColor(Color const &color,
                             bool /*overrideHue*/, bool const emit)
{
    if (_values.set(color, true)) {
        assert(_values.getSpace()->getType() == Type::HSLUV);
        updateGeometry();
        _scale = OUTER_CIRCLE_RADIUS / _picker_geometry->outer_circle_radius;
        _updatePolygon();
        if (emit)
            color_changed();
        else
            queue_drawing_area_draw();
        return true;
    }
    return false;
}

/**
 * Update the PickerGeometry structure owned by the instance.
 */
void ColorWheelHSLuv::updateGeometry()
{
    // Separate from the extremes to avoid overlapping intersections
    double lightness = std::clamp((_values[2] * 100) + 0.01, 0.1, 99.9);

    // Find the lines bounding the gamut polygon
    auto const lines = Space::HSLuv::get_bounds(lightness);

    // Find the line closest to origin
    Geom::Line const *closest_line = nullptr;
    double closest_distance = -1;

    for (auto const &line : lines) {
        double d = Geom::distance(Geom::Point(0, 0), line);
        if (closest_distance < 0 || d < closest_distance) {
            closest_distance = d;
            closest_line = &line;
        }
    }

    g_assert(closest_line);
    auto const nearest_time = closest_line->nearestTime(Geom::Point(0, 0));
    Geom::Angle start_angle{closest_line->pointAt(nearest_time)};

    constexpr auto num_lines = 6;
    constexpr auto max_intersections = num_lines * (num_lines - 1) / 2;
    std::vector<Intersection> intersections;
    intersections.reserve(max_intersections);

    for (int i = 0; i < num_lines - 1; i++) {
        for (int j = i + 1; j < num_lines; j++) {
            auto xings = lines[i].intersect(lines[j]);
            if (xings.empty()) {
                continue;
            }
            intersections.emplace_back(i, j, xings.front().point(), start_angle);
        }
    }

    std::sort(intersections.begin(), intersections.end(), [](Intersection const &lhs, Intersection const &rhs) {
        return lhs.relative_angle.radians0() >= rhs.relative_angle.radians0();
    });

    // Find the relevant vertices of the polygon, in the counter-clockwise order.
    std::vector<Geom::Point> ordered_vertices;
    ordered_vertices.reserve(intersections.size());
    double circumradius = 0.0;
    unsigned current_index = closest_line - &lines[0];

    for (auto const &intersection : intersections) {
        if (intersection.line1 == current_index) {
            current_index = intersection.line2;
        } else if (intersection.line2 == current_index) {
            current_index = intersection.line1;
        } else {
            continue;
        }
        ordered_vertices.emplace_back(intersection.point);
        circumradius = std::max(circumradius, intersection.point.length());
    }

    _picker_geometry->vertices = std::move(ordered_vertices);
    _picker_geometry->outer_circle_radius = circumradius;
    _picker_geometry->inner_circle_radius = closest_distance;
}

static Geom::IntPoint _getMargin(Gtk::Allocation const &allocation)
{
    int const width = allocation.get_width();
    int const height = allocation.get_height();

    return {std::max(0, (width - height) / 2),
            std::max(0, (height - width) / 2)};
}

inline static Geom::IntPoint _getAllocationDimensions(Gtk::Allocation const &allocation)
{
    return {allocation.get_width(), allocation.get_height()};
}

inline static int _getAllocationSize(Gtk::Allocation const &allocation)
{
    return std::min(allocation.get_width(), allocation.get_height());
}

/// Detect whether we're at the top or bottom vertex of the color space.
bool ColorWheelHSLuv::_vertex() const
{
    return _values[2] < VERTEX_EPSILON || _values[2] > 1.0 - VERTEX_EPSILON;
}

void ColorWheelHSLuv::on_drawing_area_draw(::Cairo::RefPtr<::Cairo::Context> const &cr, int, int)
{
    auto const &allocation = get_drawing_area_allocation();
    auto dimensions = _getAllocationDimensions(allocation);
    auto center = (0.5 * (Geom::Point)dimensions).floor();

    auto size = _getAllocationSize(allocation);
    double const resize = size / static_cast<double>(SIZE);

    auto const margin = _getMargin(allocation);
    auto polygon_vertices_px = to_pixel_coordinate(_picker_geometry->vertices, _scale, resize);
    for (auto &point : polygon_vertices_px) {
        point += margin;
    }

    bool const is_vertex = _vertex();
    cr->set_antialias(Cairo::ANTIALIAS_SUBPIXEL);

    if (size > _square_size && !polygon_vertices_px.empty()) {
        if (_cache_size != dimensions) {
            _updatePolygon();
        }
        if (!is_vertex) {
            // Paint with surface, clipping to polygon
            cr->save();
            cr->set_source(_surface_polygon, 0, 0);
            auto it = polygon_vertices_px.begin();
            cr->move_to((*it)[Geom::X], (*it)[Geom::Y]);
            for (++it; it != polygon_vertices_px.end(); ++it) {
                cr->line_to((*it)[Geom::X], (*it)[Geom::Y]);
            }
            cr->close_path();
            cr->fill();
            cr->restore();
        }
    }

    // Draw foreground

    // Outer circle
    std::vector<double> dashes{OUTER_CIRCLE_DASH_SIZE};
    cr->set_line_width(1);
    // White dashes
    cr->set_source_rgb(1.0, 1.0, 1.0);
    cr->set_dash(dashes, 0.0);
    cr->begin_new_path();
    cr->arc(center[Geom::X], center[Geom::Y], _scale * resize * _picker_geometry->outer_circle_radius, 0, 2 * M_PI);
    cr->stroke();
    // Black dashes
    cr->set_source_rgb(0.0, 0.0, 0.0);
    cr->set_dash(dashes, OUTER_CIRCLE_DASH_SIZE);
    cr->begin_new_path();
    cr->arc(center[Geom::X], center[Geom::Y], _scale * resize * _picker_geometry->outer_circle_radius, 0, 2 * M_PI);
    cr->stroke();
    cr->unset_dash();

    // Contrast
    auto [gray, alpha] = get_contrasting_color(perceptual_lightness(_values[2]));
    cr->set_source_rgba(gray, gray, gray, alpha);

    // Draw inscribed circle
    double const inner_stroke_width = 2.0;
    double inner_radius = is_vertex ? 0.01 : _picker_geometry->inner_circle_radius;
    cr->set_line_width(inner_stroke_width);
    cr->begin_new_path();
    cr->arc(center[Geom::X], center[Geom::Y], _scale * resize * inner_radius, 0, 2 * M_PI);
    cr->stroke();

    // Center
    cr->begin_new_path();
    cr->arc(center[Geom::X], center[Geom::Y], 2, 0, 2 * M_PI);
    cr->fill();

    // Draw marker
    auto luv = Luv::toCoordinates(_values.converted(Type::LUV)->getValues());
    auto mp = to_pixel_coordinate({luv[1], luv[2]}, _scale, resize) + margin;

    cr->set_line_width(inner_stroke_width);
    cr->begin_new_path();
    cr->arc(mp[Geom::X], mp[Geom::Y], marker_radius, 0, 2 * M_PI);
    cr->stroke();

    // Focus
    if (drawing_area_has_focus()) {
        cr->set_dash(focus_dash, 0);
        cr->set_line_width(focus_line_width);
        cr->set_source_rgb(1 - gray, 1 - gray, 1 - gray);
        cr->begin_new_path();
        cr->arc(mp[Geom::X], mp[Geom::Y], marker_radius + focus_padding, 0, 2 * M_PI);
        cr->stroke();
    }
}

bool ColorWheelHSLuv::_set_from_xy(double const x, double const y)
{
    auto const allocation = get_drawing_area_allocation();
    int const width = allocation.get_width();
    int const height = allocation.get_height();

    double const resize = std::min(width, height) / static_cast<double>(SIZE);
    auto const p = from_pixel_coordinate(Geom::Point(x, y) - _getMargin(allocation), _scale, resize);

    if (_values.set(Color(Type::LUV, Luv::fromCoordinates({_values[2] * 100, p[Geom::X], p[Geom::Y]})), true)) {
        color_changed();
        return true;
    }
    return false;
}

void ColorWheelHSLuv::_updatePolygon()
{
    auto const allocation = get_drawing_area_allocation();
    auto allocation_size = _getAllocationDimensions(allocation);
    int const size = std::min(allocation_size[Geom::X], allocation_size[Geom::Y]);

    // Update square size
    _square_size = std::max(1, static_cast<int>(size / 50));
    if (size < _square_size) {
        return;
    }

    _cache_size = allocation_size;

    double const resize = size / static_cast<double>(SIZE);

    auto const margin = _getMargin(allocation);
    auto polygon_vertices_px = to_pixel_coordinate(_picker_geometry->vertices, _scale, resize);

    // Find the bounding rectangle containing all points (adjusted by the margin).
    Geom::Rect bounding_rect;
    for (auto const &point : polygon_vertices_px) {
        bounding_rect.expandTo(point + margin);
    }
    bounding_rect *= Geom::Scale(1.0 / _square_size);

    // Round to integer pixel coords
    auto const bounding_max = bounding_rect.max().ceil();
    auto const bounding_min = bounding_rect.min().floor();

    int const stride = Cairo::ImageSurface::format_stride_for_width(Cairo::Surface::Format::RGB24, _cache_size.x());

    _surface_polygon.reset();
    _buffer_polygon.resize(_cache_size.y() * stride / 4);
    std::vector<guint32> buffer_line(stride / 4);

    auto const square_center = Geom::IntPoint(_square_size / 2, _square_size / 2);
    std::vector<double> color_vals = {_values[2] * 100, 0, 0};

    // Set the color of each pixel/square
    for (int y = bounding_min[Geom::Y]; y < bounding_max[Geom::Y]; y++) {
        for (int x = bounding_min[Geom::X]; x < bounding_max[Geom::X]; x++) {
            auto pos = Geom::IntPoint(x * _square_size, y * _square_size);
            auto point = from_pixel_coordinate(pos + square_center - margin, _scale, resize);
            color_vals[1] = point[Geom::X];
            color_vals[2] = point[Geom::Y];

            auto color = Color(Type::LUV, Luv::fromCoordinates(color_vals));
            guint32 *p = buffer_line.data() + (x * _square_size);
            for (int i = 0; i < _square_size; i++) {
                p[i] = color.toARGB();
            }
        }

        // Copy the line buffer to the surface buffer
        int const scaled_y = y * _square_size;
        for (int i = 0; i < _square_size; i++) {
            guint32 *t = _buffer_polygon.data() + (scaled_y + i) * (stride / 4);
            std::memcpy(t, buffer_line.data(), stride);
        }
    }

    _surface_polygon = ::Cairo::ImageSurface::create(reinterpret_cast<unsigned char *>(_buffer_polygon.data()),
                                                     Cairo::Surface::Format::RGB24, _cache_size.x(), _cache_size.y(), stride);
}

Gtk::EventSequenceState ColorWheelHSLuv::on_click_pressed(Gtk::GestureClick const &,
                                                          int /*n_press*/, double x, double y)
{
    auto const event_pt = Geom::Point(x, y);
    auto const allocation = get_drawing_area_allocation();
    int const size = _getAllocationSize(allocation);
    auto const region = Geom::IntRect::from_xywh(_getMargin(allocation), {size, size});

    if (region.contains(event_pt.round())) {
        _adjusting = true;
        focus_drawing_area();
        _setFromPoint(event_pt);
        return Gtk::EventSequenceState::CLAIMED;
    }

    return Gtk::EventSequenceState::NONE;
}

Gtk::EventSequenceState ColorWheelHSLuv::on_click_released(int /*n_press*/, double /*x*/, double /*y*/)
{
    _adjusting = false;
    return Gtk::EventSequenceState::CLAIMED;
}

void ColorWheelHSLuv::on_motion(Gtk::EventControllerMotion const &/*motion*/,
                                double x, double y)
{
    if (_adjusting) {
        _set_from_xy(x, y);
    }
}

bool ColorWheelHSLuv::on_key_pressed(unsigned keyval, unsigned /*keycode*/, Gdk::ModifierType state)
{
    bool consumed = false;

    // Get current point
    auto luv = *_values.converted(Type::LUV);

    double const marker_move = 1.0 / _scale;

    switch (keyval) {
        case GDK_KEY_Up:
        case GDK_KEY_KP_Up:
            luv.set(2, luv[2] + marker_move);
            consumed = true;
            break;
        case GDK_KEY_Down:
        case GDK_KEY_KP_Down:
            luv.set(2, luv[2] - marker_move);
            consumed = true;
            break;
        case GDK_KEY_Left:
        case GDK_KEY_KP_Left:
            luv.set(1, luv[1] - marker_move);
            consumed = true;
            break;
        case GDK_KEY_Right:
        case GDK_KEY_KP_Right:
            luv.set(1, luv[1] + marker_move);
            consumed = true;
    }

    if (!consumed) return false;

    _adjusting = true;

    if (_values.set(luv, true))
        color_changed();

    return true;
}

/* ColorPoint */
ColorPoint::ColorPoint()
    : x(0)
    , y(0)
    , color(0x0)
{}

ColorPoint::ColorPoint(double x, double y, Color c)
    : x(x)
    , y(y)
    , color(std::move(c))
{}

ColorPoint::ColorPoint(double x, double y, guint c)
    : x(x)
    , y(y)
    , color(c)
{}

static double lerp(double v0, double v1, double t0, double t1, double t)
{
    double const s = (t0 != t1) ? (t - t0) / (t1 - t0) : 0.0;
    return Geom::lerp(s, v0, v1);
}

static ColorPoint lerp(ColorPoint const &v0, ColorPoint const &v1, double t0, double t1,
        double t)
{
    double x = lerp(v0.x, v1.x, t0, t1, t);
    double y = lerp(v0.y, v1.y, t0, t1, t);

    auto r0 = *v0.color.converted(Type::RGB);
    auto r1 = *v1.color.converted(Type::RGB);
    double r = lerp(r0[0], r1[0], t0, t1, t);
    double g = lerp(r0[1], r1[1], t0, t1, t);
    double b = lerp(r0[2], r1[2], t0, t1, t);

    return ColorPoint(x, y, Color(Type::RGB, {r, g, b}));
}

// N.B. We also have Color:get_perceptual_lightness(), but that uses different weightings..!
double luminance(Color const &color)
{
    auto c = *color.converted(Type::RGB);
    return (c[0] * 0.2125 + c[1] * 0.7154 + c[2] * 0.0721);
}

/**
 * Convert a point of the gamut color polygon (Luv) to pixel coordinates.
 *
 * @param point The point in Luv coordinates.
 * @param scale Zoom amount to fit polygon to outer circle.
 * @param resize Zoom amount to fit wheel in widget.
 */
static Geom::Point to_pixel_coordinate(Geom::Point const &point, double scale, double resize)
{
    return Geom::Point(
        point[Geom::X] * scale * resize + (SIZE * resize / 2.0),
        (SIZE * resize / 2.0) - point[Geom::Y] * scale * resize
    );
}

/**
 * Convert a point in pixels on the widget to Luv coordinates.
 *
 * @param point The point in pixel coordinates.
 * @param scale Zoom amount to fit polygon to outer circle.
 * @param resize Zoom amount to fit wheel in widget.
 */
static Geom::Point from_pixel_coordinate(Geom::Point const &point, double scale, double resize)
{
    return Geom::Point(
        (point[Geom::X] - (SIZE * resize / 2.0)) / (scale * resize),
        ((SIZE * resize / 2.0) - point[Geom::Y]) / (scale * resize)
    );
}

/**
 * @overload
 * @param point A vector of points in Luv coordinates.
 * @param scale Zoom amount to fit polygon to outer circle.
 * @param resize Zoom amount to fit wheel in widget.
 */
static std::vector<Geom::Point> to_pixel_coordinate(std::vector<Geom::Point> const &points,
                                                    double scale, double resize)
{
    std::vector<Geom::Point> result;

    for (auto const &p : points) {
        result.emplace_back(to_pixel_coordinate(p, scale, resize));
    }

    return result;
}

/**
  * Paints padding for an edge of the triangle,
  * using the (vertically) closest point.
  *
  * @param p0 A corner of the triangle. Not the same corner as p1
  * @param p1 A corner of the triangle. Not the same corner as p0
  * @param padding The height of the padding
  * @param pad_upwards True if padding is above the line
  * @param buffer Array that the triangle is painted to
  * @param height Height of buffer
  * @param stride Stride of buffer
*/

void draw_vertical_padding(ColorPoint p0, ColorPoint p1, int padding, bool pad_upwards,
        guint32 *buffer, int height, int stride)
{
    // skip if horizontal padding is more accurate, e.g. if the edge is vertical
    double gradient = (p1.y - p0.y) / (p1.x - p0.x);
    if (std::abs(gradient) > 1.0) {
        return;
    }

    double min_y = std::min(p0.y, p1.y);
    double max_y = std::max(p0.y, p1.y);

    double min_x = std::min(p0.x, p1.x);
    double max_x = std::max(p0.x, p1.x);

    // go through every point on the line
    for (int y = min_y; y <= max_y; ++y) {
        double start_x = lerp(p0, p1, p0.y, p1.y, std::clamp(static_cast<double>(y), min_y,
                    max_y)).x;
        double end_x = lerp(p0, p1, p0.y, p1.y, std::clamp(static_cast<double>(y) + 1, min_y,
                    max_y)).x;
        if (start_x > end_x) {
            std::swap(start_x, end_x);
        }

        guint32 *p = buffer + y * stride;
        p += static_cast<int>(start_x);
        for (int x = start_x; x <= end_x; ++x) {
            // get the color at this point on the line
            ColorPoint point = lerp(p0, p1, p0.x, p1.x, std::clamp(static_cast<double>(x),
                        min_x, max_x));
            // paint the padding vertically above or below this point
            for (int offset = 0; offset <= padding; ++offset) {
                if (pad_upwards && (point.y - offset) >= 0) {
                    *(p - (offset * stride)) = point.color.toARGB();
                } else if (!pad_upwards && (point.y + offset) < height) {
                    *(p + (offset * stride)) = point.color.toARGB();
                }
            }
            ++p;
        }
    }
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
// vim:filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8: textwidth=99:
