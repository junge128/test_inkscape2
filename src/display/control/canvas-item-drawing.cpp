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

#include "canvas-item-drawing.h"

#include "desktop.h"

#include "display/drawing.h"
#include "display/drawing-context.h"
#include "display/drawing-item.h"
#include "display/drawing-group.h"

#include "helper/geom.h"
#include "ui/widget/canvas.h"
#include "ui/widget/events/canvas-event.h"
#include "ui/modifiers.h"

namespace Inkscape {

/**
 * Create the drawing. One per window!
 */
CanvasItemDrawing::CanvasItemDrawing(CanvasItemGroup *group)
    : CanvasItem(group)
    , _drawing{std::make_unique<Drawing>(this)}
{
    _name = "CanvasItemDrawing";
    _pickable = true;

    auto root = new DrawingGroup(*_drawing);
    root->setPickChildren(true);
    _drawing->setRoot(root);
}

/**
 * Returns true if point p (in canvas units) is inside some object in drawing.
 */
bool CanvasItemDrawing::contains(Geom::Point const &p, double tolerance)
{
    if (tolerance != 0) {
        std::cerr << "CanvasItemDrawing::contains: Non-zero tolerance not implemented!" << std::endl;
    }

    _picked_item = _drawing->pick(p, _drawing->cursorTolerance(), _sticky * DrawingItem::PICK_STICKY | _pick_outline * DrawingItem::PICK_OUTLINE);

    if (_picked_item) {
        // This will trigger a signal that is handled by our event handler. Seems a bit of a
        // round-about way of doing things but it matches what other pickable canvas-item classes do.
        return true;
    }

    return false;
}

/**
 * Update and redraw drawing.
 */
void CanvasItemDrawing::_update(bool)
{
    // Undo y-axis flip. This should not be here!!!!
    auto new_drawing_affine = affine();
    if (auto desktop = get_canvas()->get_desktop()) {
        new_drawing_affine = desktop->doc2dt() * new_drawing_affine;
    }

    bool affine_changed = _drawing_affine != new_drawing_affine;
    if (affine_changed) {
        _drawing_affine = new_drawing_affine;
    }

    _drawing->update(Geom::IntRect::infinite(), _drawing_affine, DrawingItem::STATE_ALL, affine_changed * DrawingItem::STATE_ALL);

    _bounds = expandedBy(_drawing->root()->drawbox(), 1); // Avoid aliasing artifacts

    if (_cursor) {
        /* Mess with enter/leave notifiers */
        auto new_drawing_item = _drawing->pick(_c, _delta, _sticky * DrawingItem::PICK_STICKY | _pick_outline * DrawingItem::PICK_OUTLINE);
        if (_active_item != new_drawing_item) {
            // Fixme: These crossing events have no modifier state set.

            if (_active_item) {
                auto event = LeaveEvent();
                _drawing_event_signal.emit(event, _active_item);
            }

            _active_item = new_drawing_item;

            if (_active_item) {
                auto event = EnterEvent();
                event.pos = _c;
                _drawing_event_signal.emit(event, _active_item);
            }
        }
    }
}

/**
 * Render drawing to screen via Cairo.
 */
void CanvasItemDrawing::_render(Inkscape::CanvasItemBuffer &buf) const
{
    auto dc = Inkscape::DrawingContext(buf.cr->cobj(), buf.rect.min());
    _drawing->render(dc, buf.rect, buf.outline_pass * DrawingItem::RENDER_OUTLINE);
}

/**
 * Handle events directed at the drawing. We first attempt to handle them here.
 */
bool CanvasItemDrawing::handle_event(CanvasEvent const &event)
{
    bool retval = false;
    
    inspect_event(event,
        [&] (EnterEvent const &event) {
            if (!_cursor) {
                if (_active_item) {
                    // Fixme: This warning seems to fire a lot.
                    std::cerr << "CanvasItemDrawing::event_handler: cursor entered drawing with an active item!" << std::endl;
                }
                _cursor = true;

                /* TODO ... event -> arena transform? */
                _c = event.pos;

                _active_item = _drawing->pick(_c, _drawing->cursorTolerance(), _sticky * DrawingItem::PICK_STICKY | _pick_outline * DrawingItem::PICK_OUTLINE);
                retval = _drawing_event_signal.emit(event, _active_item);
            }
        },

        [&] (LeaveEvent const &event) {
            if (_cursor) {
                retval = _drawing_event_signal.emit(event, _active_item);
                _active_item = nullptr;
                _cursor = false;
            }
        },

        [&] (MotionEvent const &event) {
            /* TODO ... event -> arena transform? */
            _c = event.pos;

            auto new_drawing_item = _drawing->pick(_c, _drawing->cursorTolerance(), _sticky * DrawingItem::PICK_STICKY | _pick_outline * DrawingItem::PICK_OUTLINE);
            if (_active_item != new_drawing_item) {

                /* fixme: What is wrong? */
                if (_active_item) {
                    auto event2 = LeaveEvent();
                    event2.modifiers = event.modifiers;
                    retval = _drawing_event_signal.emit(event2, _active_item);
                }

                _active_item = new_drawing_item;

                if (_active_item) {
                    auto event2 = EnterEvent();
                    event2.modifiers = event.modifiers;
                    event2.pos = event.pos;
                    retval = _drawing_event_signal.emit(event2, _active_item);
                }
            }
            retval = retval || _drawing_event_signal.emit(event, _active_item);
        },

        [&] (ScrollEvent const &event) {
            if (Modifiers::Modifier::get(Modifiers::Type::CANVAS_ZOOM)->active(event.modifiers)) {
                /* Zoom is emitted by the canvas as well, ignore here */
                retval = false;
                return;
            }
            retval = _drawing_event_signal.emit(event, _active_item);
        },

        [&] (CanvasEvent const &event) {
            // Just send event.
            retval = _drawing_event_signal.emit(event, _active_item);
        }
    );

    return retval;
}

} // namespace Inkscape

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
