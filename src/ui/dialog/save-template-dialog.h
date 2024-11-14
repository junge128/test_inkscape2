// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2017 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef INKSCAPE_SEEN_UI_DIALOG_SAVE_TEMPLATE_H
#define INKSCAPE_SEEN_UI_DIALOG_SAVE_TEMPLATE_H

#include <glibmm/refptr.h>

namespace Gtk {
class Builder;
class CheckButton;
class Dialog;
class Entry;
class Window;
}

namespace Inkscape::UI::Dialog {

class SaveTemplate
{

public:

    static void save_document_as_template(Gtk::Window &parentWindow);

protected:

    void on_name_changed();

private:

    Glib::RefPtr<Gtk::Builder> builder;

    Gtk::Dialog &dialog;

    Gtk::Entry &name;
    Gtk::Entry &author;
    Gtk::Entry &description;
    Gtk::Entry &keywords;

    Gtk::CheckButton &set_default_template;

    SaveTemplate(Gtk::Window &parent);
    void save_template(Gtk::Window &parent);

};
} // namespace Inkscape:UI::Dialog
#endif // SEEN_TOOLBAR_SNAP_H

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
