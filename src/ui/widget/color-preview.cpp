// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Author:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Ralf Stephan <ralf@ark.in-berlin.de>
 *   Michael Kowalski
 *
 * Copyright (C) 2001-2005 Authors
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "ui/widget/color-preview.h"

#include <cairo.h>
#include <cairomm/context.h>
#include <cairomm/matrix.h>
#include <cairomm/pattern.h>
#include <gdkmm/rgba.h>
#include <gtkmm/enums.h>
#include <gtkmm/window.h>
#include <sigc++/functors/mem_fun.h>
#include <gtkmm/drawingarea.h>
#include "display/cairo-utils.h"
#include "colors/color.h"
#include "colors/spaces/enum.h"
#include <2geom/rect.h>
#include "ui/util.h"

namespace Inkscape::UI::Widget {

ColorPreview::ColorPreview(std::uint32_t const rgba)
    : _rgba{rgba}
{
    set_name("ColorPreview");
    set_draw_func(sigc::mem_fun(*this, &ColorPreview::draw_func));
    setStyle(_style);
}

void ColorPreview::setRgba32(std::uint32_t const rgba) {
    if (_rgba == rgba && !_pattern) return;

    _rgba = rgba;
    _pattern = {};
    queue_draw();
}

void ColorPreview::setPattern(Cairo::RefPtr<Cairo::Pattern> pattern) {
    if (_pattern == pattern) return;

    _pattern = pattern;
    _rgba = 0;
    queue_draw();
}

Geom::Rect round_rect(const Cairo::RefPtr<Cairo::Context>& ctx, Geom::Rect rect, double radius) {
    auto x = rect.left();
    auto y = rect.top();
    auto width = rect.width();
    auto height = rect.height();
    ctx->arc(x + width - radius, y + radius, radius, -M_PI_2, 0);
    ctx->arc(x + width - radius, y + height - radius, radius, 0, M_PI_2);
    ctx->arc(x + radius, y + height - radius, radius, M_PI_2, M_PI);
    ctx->arc(x + radius, y + radius, radius, M_PI, 3 * M_PI_2);
    ctx->close_path();
    return rect.shrunkBy(1);
}

Cairo::RefPtr<Cairo::Pattern> create_checkerboard_pattern(double tx, double ty) {
    auto pattern = Cairo::RefPtr<Cairo::Pattern>(new Cairo::Pattern(ink_cairo_pattern_create_checkerboard(), true));
    pattern->set_matrix(Cairo::translation_matrix(tx, ty));
    return pattern;
}

void ColorPreview::draw_func(Cairo::RefPtr<Cairo::Context> const &cr,
                             int const widget_width, int const widget_height)
{
    double width = widget_width;
    double height = widget_height;
    auto x = 0.0;
    auto y = 0.0;
    double radius = _style == Simple ? 0.0 : 2.0;
    double degrees = M_PI / 180.0;
    auto rect = Geom::Rect(x, y, x + width, y + height);

    std::uint32_t outline_color = 0x00000000;
    std::uint32_t border_color = 0xffffff00;

    auto style = get_style_context();
    Gdk::RGBA bgnd;
    bool found = style->lookup_color("theme_bg_color", bgnd);
    // use theme background color as a proxy for dark theme; this method is fast, which is important here
    auto dark_theme = found && get_luminance(bgnd) <= 0.5;
    auto state = get_state_flags();
    auto disabled = (state & Gtk::StateFlags::INSENSITIVE) == Gtk::StateFlags::INSENSITIVE;
    auto backdrop = (state & Gtk::StateFlags::BACKDROP) == Gtk::StateFlags::BACKDROP;
    if (dark_theme) {
        std::swap(outline_color, border_color);
    }

    if (_style == Outlined) {
        // outside outline
        rect = round_rect(cr, rect, radius--);
        // opacity of outside outline is reduced
        int alpha = disabled || backdrop ? 0x2f : 0x5f;
        ink_cairo_set_source_rgba32(cr->cobj(), outline_color | alpha);
        cr->fill();

        // inside border
        rect = round_rect(cr, rect, radius--);
        ink_cairo_set_source_rgba32(cr->cobj(), border_color | 0xff);
        cr->fill();
    }

    if (_pattern) {
        // draw pattern-based preview
        round_rect(cr, rect, radius);

        // checkers first
        auto checkers = create_checkerboard_pattern(-x, -y);
        cr->set_source(checkers);
        cr->fill_preserve();

        cr->set_source(_pattern);
        cr->fill();
    }
    else {
        // color itself
        auto rgba = _rgba;
        auto alpha = rgba & 0xff;
        // if preview is disabled, render colors with reduced saturation and intensity
        if (disabled) {
            auto color = Colors::Color(_rgba);
            auto hsl = *color.converted(Colors::Space::Type::HSLUV);
            // reduce saturation and lightness/darkness (on dark/light theme)
            double lf = 0.35; // lightness factor - 35% of lightness
            double sf = 0.30; // saturation factor - 30% of saturation
            // for both light and dark themes the idea it to compress full range of color lightness (0..1)
            // to a narrower range to convey subdued look of disabled widget (that's the lf * l part);
            // then we move the lightness floor to 0.70 for light theme and 0.20 for dark theme:
            auto saturation = hsl.get(1) * sf;
            auto lightness = lf * hsl.get(2) + (dark_theme ? 0.20 : 0.70); // new lightness in 0..1 range
            hsl.set(1, saturation);
            hsl.set(2, lightness);
            auto rgb = *hsl.converted(Colors::Space::Type::RGB);
            rgba = rgb.toRGBA();
        }

        width = rect.width() / 2;
        height = rect.height();
        x = rect.min().x();
        y = rect.min().y();

        // solid on the right
        auto solid = rgba | 0xff;
        cairo_new_sub_path(cr->cobj());
        cairo_line_to(cr->cobj(), x + width, y);
        cairo_line_to(cr->cobj(), x + width, y + height);
        cairo_arc(cr->cobj(), x + radius, y + height - radius, radius, 90 * degrees, 180 * degrees);
        cairo_arc(cr->cobj(), x + radius, y + radius, radius, 180 * degrees, 270 * degrees);
        cairo_close_path(cr->cobj());
        ink_cairo_set_source_rgba32(cr->cobj(), solid);
        cr->fill();

        // semi-transparent on the left
        x += width;
        cairo_new_sub_path(cr->cobj());
        cairo_arc(cr->cobj(), x + width - radius, y + radius, radius, -90 * degrees, 0 * degrees);
        cairo_arc(cr->cobj(), x + width - radius, y + height - radius, radius, 0 * degrees, 90 * degrees);
        cairo_line_to(cr->cobj(), x, y + height);
        cairo_line_to(cr->cobj(), x, y);
        cairo_close_path(cr->cobj());
        if (alpha < 0xff) {
            auto checkers = create_checkerboard_pattern(-x, -y);
            cr->set_source(checkers);
            cr->fill_preserve();
        }
        ink_cairo_set_source_rgba32(cr->cobj(), rgba);
        cr->fill();
    }

    if (_indicator != None) {
        constexpr double side = 7.5;
        constexpr double line = 1.5; // 1.5 pixles b/c it's a diagonal line, so 1px is too thin
        const auto right = rect.right();
        const auto bottom = rect.bottom();
        if (_indicator == Swatch) {
            // draw swatch color indicator - a black corner
            cr->move_to(right, bottom - side);
            cr->line_to(right, bottom - side + line);
            cr->line_to(right - side + line, bottom);
            cr->line_to(right - side, bottom);
            cr->set_source_rgb(1, 1, 1); // white separator
            cr->fill();
            cr->move_to(right, bottom - side + line);
            cr->line_to(right, bottom);
            cr->line_to(right - side + line, bottom);
            cr->set_source_rgb(0, 0, 0); // black
            cr->fill();
        }
        else if (_indicator == SpotColor) {
            // draw spot color indicator - a black dot
            cr->move_to(right, bottom);
            cr->line_to(right, bottom - side);
            cr->line_to(right - side, bottom);
            cr->set_source_rgb(1, 1, 1); // white background
            cr->fill();
            constexpr double r = 2;
            cr->arc(right - r, bottom - r, r, 0, 2*M_PI);
            cr->set_source_rgb(0, 0, 0);
            cr->fill();
        }
        else {
            assert(false);
        }
    }
}

void ColorPreview::setStyle(Style style) {
    _style = style;
    if (style == Simple) {
        add_css_class("simple");
    }
    else {
        remove_css_class("simple");
    }
    queue_draw();
}

void ColorPreview::setIndicator(Indicator indicator) {
    if (_indicator != indicator) {
        _indicator = indicator;
        queue_draw();
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
