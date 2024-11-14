// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 * A class to hold:
 *   - Top toolbars
 *     - Command Toolbar (in horizontal mode)
 *     - Tool Toolbars (one at a time)
 *     - Snap Toolbar (in simple or advanced modes)
 *   - DesktopHBox
 *     - ToolboxCanvasPaned
 *       - Tool Toolbar (Tool selection)
 *       - Dialog Container
 *     - Snap Toolbar (in permanent mode)
 *     - Command Toolbar (in vertical mode)
 *   - Swatches
 *   - StatusBar.
 */
/* Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   MenTaLguY <mental@rydia.net>
 *   bulia byak <buliabyak@users.sf.net>
 *   Ralf Stephan <ralf@ark.in-berlin.de>
 *   John Bintz <jcoswell@coswellproductions.org>
 *   Johan Engelen <j.b.c.engelen@ewi.utwente.nl>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *
 * Copyright (C) 2007 Johan Engelen
 * Copyright (C) 2006 John Bintz
 * Copyright (C) 2004 MenTaLguY
 * Copyright (C) 1999-2002 Lauris Kaplinski
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_SP_DESKTOP_WIDGET_H
#define SEEN_SP_DESKTOP_WIDGET_H

#include <memory>
#include <2geom/point.h>
#include <glibmm/refptr.h>
#include <gtkmm/box.h>

#include <sigc++/scoped_connection.h>
#include "message.h"
#include "preferences.h"

namespace Glib {
class ustring;
} // namespace Glib

namespace Gio {
class ActionMap;
} // namespace Gio

namespace Gtk {
class Grid;
class Paned;
class Toolbar;
class Widget;
} // namespace Gtk

class InkscapeWindow;
class SPDocument;
class SPDesktop;
class SPObject;

namespace Inkscape::UI {

namespace Dialog {
class DialogContainer;
class DialogMultipaned;
class SwatchesPanel;
} // namespace Dialog

namespace Toolbar {
class Toolbars;
class CommandToolbar;
class SnapToolbar;
} // namespace Toolbars

namespace Widget {
class Button;
class Canvas;
class CanvasGrid;
class SpinButton;
class StatusBar;
} // namespace Widget

} // namespace Inkscape::UI

/// A GtkBox on an SPDesktop.
class SPDesktopWidget : public Gtk::Box
{
    using parent_type = Gtk::Box;

public:
    SPDesktopWidget(InkscapeWindow *inkscape_window, SPDocument *document);
    ~SPDesktopWidget() override;

    Inkscape::UI::Widget::CanvasGrid *get_canvas_grid()  { return _canvas_grid; }  // Temp, I hope!
    Inkscape::UI::Widget::Canvas     *get_canvas()       { return _canvas; }
    SPDesktop                        *get_desktop()      { return _desktop.get(); }
    InkscapeWindow             const *get_window() const { return _window; }
    InkscapeWindow                   *get_window()       { return _window; }
    double                            get_dt2r()   const { return _dt2r; }

    void set_window(InkscapeWindow * const window) { _window = window; }

    Gio::ActionMap *get_action_map();

    void on_realize() override;
    void on_unrealize() override;

private:
    sigc::scoped_connection modified_connection;

    std::unique_ptr<SPDesktop> _desktop;
    InkscapeWindow *_window = nullptr;

    Gtk::Paned *_tbbox = nullptr;
    Gtk::Box *_hbox = nullptr;
    std::unique_ptr<Inkscape::UI::Dialog::DialogContainer> _container;
    Inkscape::UI::Dialog::DialogMultipaned *_columns = nullptr;
    Gtk::Grid* _top_toolbars = nullptr;

    Inkscape::UI::Widget::StatusBar *_statusbar = nullptr;
    Inkscape::UI::Dialog::SwatchesPanel *_panels;

    /** A grid to display the canvas, rulers, and scrollbars. */
    Inkscape::UI::Widget::CanvasGrid *_canvas_grid = nullptr;

    double _dt2r;
    Inkscape::UI::Widget::Canvas *_canvas = nullptr;

public:
    void setMessage(Inkscape::MessageType type, char const *message);
    void viewSetPosition (Geom::Point p);
    void letRotateGrabFocus();
    void letZoomGrabFocus();
    Geom::IntPoint getWindowSize() const;
    void setWindowSize(Geom::IntPoint const &size);
    void setWindowTransient(Gtk::Window &window, int transient_policy);
    void presentWindow();
    void showInfoDialog(Glib::ustring const &message);
    bool warnDialog (Glib::ustring const &text);
    Gtk::Widget *get_toolbar_by_name(const Glib::ustring &name);
    void setToolboxFocusTo(char const *);
    void setToolboxAdjustmentValue(char const *id, double value);
    bool isToolboxButtonActive(char const *id) const;
    void setCoordinateStatus(Geom::Point p);
    void updateTitle(char const *uri);
    void onFocus(bool has_focus);
    Inkscape::UI::Dialog::DialogContainer *getDialogContainer();
    void showNotice(Glib::ustring const &msg, int timeout = 0);

    void updateNamedview();
    void update_guides_lock();

    // Canvas Grid Widget
    void update_zoom();
    void update_rotation();
    void repack_snaptoolbar();

    void layoutWidgets();
    void toggle_scrollbars();
    void toggle_command_palette();
    void toggle_rulers();
    void sticky_zoom_toggled();
    void sticky_zoom_updated();

private:
    Gtk::Widget *tool_toolbox;
    std::unique_ptr<Inkscape::UI::Toolbar::Toolbars> tool_toolbars;
    std::unique_ptr<Inkscape::UI::Toolbar::CommandToolbar> command_toolbar;
    std::unique_ptr<Inkscape::UI::Toolbar::SnapToolbar> snap_toolbar;
    Inkscape::PrefObserver _tb_snap_pos;
    Inkscape::PrefObserver _tb_icon_sizes1;
    Inkscape::PrefObserver _tb_icon_sizes2;
    Inkscape::PrefObserver _tb_visible_buttons;
    Inkscape::PrefObserver _ds_sticky_zoom;

    void namedviewModified(SPObject *obj, unsigned flags);
    void apply_ctrlbar_settings();
    void remove_from_top_toolbar_or_hbox(Gtk::Widget &widget);
};

#endif /* !SEEN_SP_DESKTOP_WIDGET_H */

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
