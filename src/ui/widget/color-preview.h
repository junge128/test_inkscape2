// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Ralf Stephan <ralf@ark.in-berlin.de>
 *   Michael Kowalski
 *
 * Copyright (C) 2001-2005 Authors
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_COLOR_PREVIEW_H
#define SEEN_COLOR_PREVIEW_H

#include <cairomm/pattern.h>
#include <cstdint>
#include <cairomm/refptr.h>
#include <gtkmm/drawingarea.h>

namespace Cairo {
class Context;
} // namespace Cairo

namespace Inkscape::UI::Widget {

/**
 * A color preview widget, used within a picker button and style indicator.
 * It can show RGBA color or Cairo pattern.
 *
 * RGBA colors are split in half to show solid color and transparency, if any.
 * RGBA colors are also manipulated to reduce intensity if color preview is disabled.
 *
 * Patterns are shown "as is" on top of the checkerboard.
 * There's no separate "disabled" look for patterns.
 *
 * Outlined style can be used to surround color patch with a contrasting border.
 * Border is dark-theme-aware.
 *
 * Indicators can be used to distinguish ad-hoc colors from swatches and spot colors.
 */
class ColorPreview final : public Gtk::DrawingArea {
public:
    ColorPreview(std::uint32_t rgba);
    // set preview color RGBA with opacity (alpha)
    void setRgba32(std::uint32_t rgba);
    // set arbitrary pattern-based preview
    void setPattern(Cairo::RefPtr<Cairo::Pattern> pattern);
    // simple color patch vs outlined color patch
    enum Style { Simple, Outlined };
    void setStyle(Style style);
    // add indicator on top of the preview: swatch or spot color
    enum Indicator { None, Swatch, SpotColor };
    void setIndicator(Indicator indicator);
private:
    std::uint32_t _rgba; // requested RGBA color, used if there is no pattern given
    Cairo::RefPtr<Cairo::Pattern> _pattern; // pattern to show, if provided
    Style _style = Simple;
    Indicator _indicator = None;
    void draw_func(Cairo::RefPtr<Cairo::Context> const &cr, int width, int height);
};

} // namespace Inkscape::UI::Widget

#endif // SEEN_COLOR_PREVIEW_H

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
