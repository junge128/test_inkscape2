// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_UI_TOOLS_SELECT_TOOl_H
#define INKSCAPE_UI_TOOLS_SELECT_TOOl_H

/*
 * Select tool
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * Copyright (C) 1999-2002 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "display/control/canvas-item-enums.h"
#include "display/control/canvas-item.h"
#include "rubberband.h"
#include "ui/tools/tool-base.h"

namespace Inkscape {
struct ScrollEvent;
class SelTrans;
class SelectionDescriber;
class Selection;
} // namespace Inkscape

namespace Inkscape::UI::Tools {

class SelectTool : public ToolBase
{
public:
    SelectTool(SPDesktop *desktop);
    ~SelectTool() override;

    bool moved = false;
    unsigned button_press_state = 0;

    std::vector<SPItem *> cycling_items;
    std::vector<SPItem *> cycling_items_cmp;
    SPItem *cycling_cur_item = nullptr;
    bool cycling_wrap = true;

    SPItem *item = nullptr;
    CanvasItem *grabbed = nullptr;
    SelTrans *_seltrans = nullptr;
    SelectionDescriber *_describer = nullptr;
    char *no_selection_msg = nullptr;

    void set(Preferences::Entry const &val) override;
    bool root_handler(CanvasEvent const &event) override;
    bool item_handler(SPItem *item, CanvasEvent const &event) override;

    void updateDescriber(Selection *sel);

private:
    bool sp_select_context_abort();
    void sp_select_context_cycle_through_items(Selection *selection, ScrollEvent const &scroll_event);
    void sp_select_context_reset_opacities();
    static std::pair<Rubberband::Mode, CanvasItemCtrlType> get_default_rubberband_state();

    bool _alt_on = false;
    bool _force_dragging = false;

    std::string _default_cursor;
};

} // namespace Inkscape::UI::Tools

#endif // INKSCAPE_UI_TOOLS_SELECT_TOOl_H

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
