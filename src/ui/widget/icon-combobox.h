// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SEEN_INKSCAPE_UI_WIDGET_ICONCOMBOBOX
#define SEEN_INKSCAPE_UI_WIDGET_ICONCOMBOBOX

#include <cairomm/refptr.h>
#include <cairomm/surface.h>
#include <giomm/liststore.h>
#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <gtkmm/boolfilter.h>
#include <gtkmm/dropdown.h>
#include <gtkmm/filterlistmodel.h>
#include <gtkmm/signallistitemfactory.h>
#include <gtkmm/singleselection.h>
#include <sigc++/signal.h>

namespace Inkscape::UI::Widget {

class IconComboBox : public Gtk::DropDown
{
public:
    IconComboBox(bool use_icons = true);
    ~IconComboBox() override;

    void add_row(Glib::ustring const &icon_name, Glib::ustring const &label, int id);
    void add_row(Cairo::RefPtr<Cairo::Surface> image, const Glib::ustring& label, int id);
    void set_active_by_id(int id);
    void set_row_visible(int id, bool visible = true, bool refilter_items = true);
    int get_active_row_id() const;
    // return signal to selection change event; it reports ID of current item
    sigc::signal<void (int)>& signal_changed();

    static int get_image_size() { return 16; }

    void refilter();
private:
    struct ListItem;
    bool is_item_visible(const Glib::RefPtr<Glib::ObjectBase>& item) const;
    std::pair<std::shared_ptr<IconComboBox::ListItem>, int> find_by_id(int id, bool visible_only);
    std::shared_ptr<IconComboBox::ListItem> current_item();

    Glib::RefPtr<Gtk::SignalListItemFactory> _factory;
    Glib::RefPtr<Gtk::FilterListModel> _filtered_model;
    Glib::RefPtr<Gtk::SingleSelection> _selection_model;
    Glib::RefPtr<Gtk::BoolFilter> _filter;
    Glib::RefPtr<Gio::ListStore<IconComboBox::ListItem>> _store;
    sigc::signal<void (int)> _signal_current_changed = sigc::signal<void (int)>();
};

} // namespace Inkscape::UI::Widget

#endif // SEEN_INKSCAPE_UI_WIDGET_ICONCOMBOBOX

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim:filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99:
