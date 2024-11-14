// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * A class to render the SVG drawing.
 */
/*
 * Author:
 *   Tavmjong Bah
 *
 * Copyright (C) 2020 Tavmjong Bah
 *
 * Rewrite of _SPCanvasArena.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_CANVAS_ITEM_DRAWING_H
#define SEEN_CANVAS_ITEM_DRAWING_H

#include <memory>
#include <sigc++/signal.h>

#include "canvas-item.h"

namespace Inkscape {

class Drawing;
class DrawingItem;
class Updatecontext;

class CanvasItemDrawing final : public CanvasItem
{
public:
    CanvasItemDrawing(CanvasItemGroup *group);

    // Selection
    bool contains(Geom::Point const &p, double tolerance = 0) override;

    // Display
    Inkscape::Drawing *get_drawing() { return _drawing.get(); }

    // Drawing items
    void set_active(Inkscape::DrawingItem *active) { _active_item = active; }
    Inkscape::DrawingItem *get_active() { return _active_item; }

    // Events
    bool handle_event(CanvasEvent const &event) override;
    void set_sticky(bool sticky) { _sticky = sticky; }
    void set_pick_outline(bool pick_outline) { _pick_outline = pick_outline; }

    // Signals
    sigc::connection connect_drawing_event(sigc::slot<bool(CanvasEvent const &, Inkscape::DrawingItem *)> slot) {
        return _drawing_event_signal.connect(slot);
    }

protected:
    ~CanvasItemDrawing() override = default;

    void _update(bool propagate) override;
    void _render(Inkscape::CanvasItemBuffer &buf) const override;

    // Selection
    Geom::Point _c;
    double _delta = Geom::infinity();
    Inkscape::DrawingItem *_active_item = nullptr;
    Inkscape::DrawingItem *_picked_item = nullptr;

    // Display
    std::unique_ptr<Inkscape::Drawing> _drawing;
    Geom::Affine _drawing_affine;

    // Events
    bool _cursor = false;
    bool _sticky = false; // Pick anything, even if hidden.
    bool _pick_outline = false;

    // Signals
    sigc::signal<bool(CanvasEvent const &, Inkscape::DrawingItem *)> _drawing_event_signal;
};

} // namespace Inkscape

#endif // SEEN_CANVAS_ITEM_DRAWING_H

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
