// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_CANVAS_ITEM_CTRL_H
#define SEEN_CANVAS_ITEM_CTRL_H

/**
 * A class to represent a control node.
 */

/*
 * Authors:
 *   Tavmjong Bah
 *   Sanidhya Singh
 *
 * Copyright (C) 2020 Tavmjong Bah
 *
 * Rewrite of SPCtrl
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <memory>
#include <2geom/point.h>

#include "canvas-item.h"
#include "canvas-item-enums.h"
#include "ctrl-handle-styling.h"

#include "enums.h" // SP_ANCHOR_X
#include "display/initlock.h"

namespace Inkscape {

// Handle sizes relative to the preferred size
enum class HandleSize { XTINY = -4, TINY = -2, SMALL = -1, NORMAL = 0, LARGE = 1 };

class CanvasItemCtrl : public CanvasItem
{
public:
    CanvasItemCtrl(CanvasItemGroup *group);
    CanvasItemCtrl(CanvasItemGroup *group, CanvasItemCtrlType type);
    CanvasItemCtrl(CanvasItemGroup *group, CanvasItemCtrlType type, Geom::Point const &p);

    // Geometry
    void set_position(Geom::Point const &position);

    double closest_distance_to(Geom::Point const &p) const;

    // Selection
    bool contains(Geom::Point const &p, double tolerance = 0) override;

    // Properties
    void set_size(HandleSize rel_size);
    void set_fill(uint32_t rgba) override;
    void set_stroke(uint32_t rgba) override;
    void set_shape(CanvasItemCtrlShape shape);
    void set_size_via_index(int size_index);
    void set_size_default(); // Use preference and type to set size.
    void set_anchor(SPAnchorType anchor);
    void set_angle(double angle);
    void set_type(CanvasItemCtrlType type);
    void set_selected(bool selected = true);
    void set_click(bool click = true);
    void set_hover(bool hover = true);
    void set_normal(bool selected = false);

    // do not call directly; only used for invisible handle
    void _set_size(int size);
protected:
    ~CanvasItemCtrl() override = default;

    void _update(bool propagate) override;
    void _render(CanvasItemBuffer &buf) const override;
    void _invalidate_ctrl_handles() override;

    void build_cache(int device_scale) const;
    float get_width() const;
    float get_total_width() const;

private:
    // Geometry
    Geom::Point _position;
    // Display
    InitLock _built;
    mutable std::shared_ptr<Cairo::ImageSurface const> _cache;
    // Properties
    Handles::TypeState _handle;
    CanvasItemCtrlShape _shape = CANVAS_ITEM_CTRL_SHAPE_SQUARE;
    uint32_t _fill = 0x000000ff;
    uint32_t _stroke = 0xffffffff;
    bool _shape_set = false;
    bool _fill_set = false;
    bool _stroke_set = false;
    bool _size_set = false;
    double _angle = 0; // Used for triangles, could be used for arrows.
    SPAnchorType _anchor = SP_ANCHOR_CENTER;
    int _width  = 5;
    HandleSize _rel_size = HandleSize::NORMAL;
    Geom::Point _pos;

    // get effective stroke width
    float get_stroke_width() const;
    // get size of the pixmap needed to render this control item
    int get_pixmap_width(int device_scale) const;
    // for debugging only - save handles to "handle.png"
    void _dump();
};

} // namespace Inkscape

#endif // SEEN_CANVAS_ITEM_CTRL_H

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
