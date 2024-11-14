// SPDX-License-Identifier: GPL-2.0-or-later

#include <cairomm/context.h>
#include <cairomm/refptr.h>
#include <gdkmm/rgba.h>
#include <gtkmm/stylecontext.h>

#include <2geom/rect.h>

namespace Inkscape::Util {

// create a rectangular path with rounded corners
Geom::Rect rounded_rectangle(const Cairo::RefPtr<Cairo::Context>& ctx, const Geom::Rect& rect, double radius);

// draw a shaded border around given area
void draw_border(const Cairo::RefPtr<Cairo::Context>& ctx, Geom::Rect rect, double radius, const Gdk::RGBA& color, int device_scale, bool circular);

// draw a border that stands out in a bright and dark theme
void draw_standard_border(const Cairo::RefPtr<Cairo::Context>& ctx, Geom::Rect rect, bool dark_theme, double radius, int device_scale, bool circular = false);

// find theme background color; it may not be defined on some themes
std::optional<Gdk::RGBA> lookup_background_color(Glib::RefPtr<Gtk::StyleContext>& style);

// find theme foreground color; it may not be defined on some themes
std::optional<Gdk::RGBA> lookup_foreground_color(Glib::RefPtr<Gtk::StyleContext>& style);

} // namespace Inkscape::Util
