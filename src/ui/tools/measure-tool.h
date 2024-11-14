// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Our fine measuring tool
 *
 * Authors:
 *   Felipe Correa da Silva Sanches <juca@members.fsf.org>
 *   Jabiertxo Arraiza <jabier.arraiza@marker.es>
 * Copyright (C) 2011 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_TOOLS_MEASURE_TOOL_H
#define INKSCAPE_UI_TOOLS_MEASURE_TOOL_H

#include <cstddef>
#include <optional>
#include <2geom/point.h>

#include "display/control/canvas-temporary-item.h"
#include "display/control/canvas-item-enums.h"
#include "display/control/canvas-item-ptr.h"
#include <sigc++/scoped_connection.h>
#include "ui/tools/tool-base.h"

class SPKnot;
namespace Inkscape { class CanvasItemCurve; }

namespace Inkscape::UI::Tools {

class MeasureTool : public ToolBase
{
public:
    MeasureTool(SPDesktop *desktop);
    ~MeasureTool() override;

    bool root_handler(CanvasEvent const &event) override;
    void showCanvasItems(bool to_guides = false, bool to_item = false, bool to_phantom = false, Inkscape::XML::Node *measure_repr = nullptr);
    void reverseKnots();
    void toGuides();
    void toPhantom();
    void toMarkDimension();
    void toItem();
    void reset();
    void setMarkers();
    void setMarker(bool isStart);
    Geom::Point readMeasurePoint(bool is_start) const;
    void writeMeasurePoint(Geom::Point point, bool is_start) const;

    void showInfoBox(Geom::Point cursor, bool into_groups);
    void showItemInfoText(Geom::Point pos, Glib::ustring const &measure_str, double fontsize);
    void setGuide(Geom::Point origin, double angle, const char *label);
    void setPoint(Geom::Point origin, Inkscape::XML::Node *measure_repr);
    void setLine(Geom::Point start_point,Geom::Point end_point, bool markers, guint32 color,
                 Inkscape::XML::Node *measure_repr = nullptr);
    void setMeasureCanvasText(bool is_angle, double precision, double amount, double fontsize,
                              Glib::ustring unit_name, Geom::Point position, guint32 background,
                              bool to_left, bool to_item, bool to_phantom,
                              Inkscape::XML::Node *measure_repr);
    void setMeasureCanvasItem(Geom::Point position, bool to_item, bool to_phantom,
                              Inkscape::XML::Node *measure_repr);
    void setMeasureCanvasControlLine(Geom::Point start, Geom::Point end, bool to_item, bool to_phantom,
                                     Inkscape::CanvasItemColor color, Inkscape::XML::Node *measure_repr);
    void setLabelText(Glib::ustring const &value, Geom::Point pos, double fontsize, Geom::Coord angle,
                      guint32 background,
                      Inkscape::XML::Node *measure_repr = nullptr);

    void knotStartMovedHandler(SPKnot */*knot*/, Geom::Point const &ppointer, guint state);
    void knotEndMovedHandler(SPKnot */*knot*/, Geom::Point const &ppointer, guint state);
    void knotClickHandler(SPKnot *knot, guint state);
    void knotUngrabbedHandler(SPKnot */*knot*/,  unsigned int /*state*/);
    void setMeasureItem(Geom::PathVector pathv, bool is_curve, bool markers, guint32 color, Inkscape::XML::Node *measure_repr);
    void createAngleDisplayCurve(Geom::Point const &center, Geom::Point const &end, Geom::Point const &anchor,
                                 double angle, bool to_phantom,
                                 Inkscape::XML::Node *measure_repr = nullptr);

private:
    std::optional<Geom::Point> explicit_base;
    std::optional<Geom::Point> last_end;
    SPKnot *knot_start = nullptr;
    SPKnot *knot_end   = nullptr;
    int dimension_offset = 20;
    Geom::Point start_p;
    Geom::Point end_p;
    Geom::Point last_pos;

    std::vector<CanvasItemPtr<CanvasItem>> measure_tmp_items;
    std::vector<CanvasItemPtr<CanvasItem>> measure_phantom_items;
    std::vector<CanvasItemPtr<CanvasItem>> measure_item;

    double item_width;
    double item_height;
    double item_x;
    double item_y;
    double item_length;
    SPItem *over;
    sigc::scoped_connection _knot_start_moved_connection;
    sigc::scoped_connection _knot_start_ungrabbed_connection;
    sigc::scoped_connection _knot_start_click_connection;
    sigc::scoped_connection _knot_end_moved_connection;
    sigc::scoped_connection _knot_end_click_connection;
    sigc::scoped_connection _knot_end_ungrabbed_connection;
};

} // namespace Inkscape::UI::Tools

#endif // INKSCAPE_UI_TOOLS_MEASURE_TOOL_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
