// SPDX-License-Identifier: GPL-2.0-or-later
/* Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2022 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef WIDGET_TEMPLATE_LIST_H
#define WIDGET_TEMPLATE_LIST_H

#include <giomm/liststore.h>
#include <gtkmm/gridview.h>
#include <memory>
#include <string>
#include <glibmm/refptr.h>
#include <gtkmm/notebook.h>
#include <sigc++/signal.h>
#include <vector>

#include "extension/template.h"
#include "ui/iconview-item-factory.h"

namespace Gdk {
class Pixbuf;
} // namespace Gdk

namespace Gtk {
class Builder;
class IconView;
class ListStore;
} // namespace Gtk

class SPDocument;

namespace Inkscape::UI::Widget {

class TemplateList final : public Gtk::Notebook
{
public:
    TemplateList() = default;
    TemplateList(BaseObjectType *cobject, const Glib::RefPtr<Gtk::Builder> &refGlade);

    void init(Extension::TemplateShow mode);
    void reset_selection();
    bool has_selected_preset();
    std::shared_ptr<Extension::TemplatePreset> get_selected_preset();
    SPDocument *new_document();

    sigc::connection connectItemSelected(const sigc::slot<void ()> &slot) { return _item_selected_signal.connect(slot); }
    sigc::connection connectItemActivated(const sigc::slot<void ()> &slot) { return _item_activated_signal.connect(slot); }

private:
    struct TemplateItem;
    Glib::RefPtr<Gio::ListStore<TemplateItem>> generate_category(std::string const &label);
    Cairo::RefPtr<Cairo::ImageSurface> icon_to_pixbuf(std::string const &name, int scale);
    Gtk::GridView *get_iconview(Gtk::Widget *widget);

    sigc::signal<void ()> _item_selected_signal;
    sigc::signal<void ()> _item_activated_signal;
    std::vector<std::unique_ptr<IconViewItemFactory>> _factory;
};

} // namespace Inkscape::UI::Widget

#endif // WIDGET_TEMPLATE_LIST_H

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
