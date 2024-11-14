// SPDX-License-Identifier: GPL-2.0-or-later
/* Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2022 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "template-list.h"

#include <cairomm/surface.h>
#include <giomm/liststore.h>
#include <glibmm/markup.h>
#include <glibmm/refptr.h>
#include <gtkmm/expression.h>
#include <gtkmm/gridview.h>
#include <map>
#include <glib/gi18n.h>
#include <gdkmm/pixbuf.h>
#include <gtkmm/builder.h>
#include <gtkmm/iconview.h>
#include <gtkmm/liststore.h>
#include <gtkmm/numericsorter.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/sortlistmodel.h>
#include <gtkmm/singleselection.h>
#include <memory>

#include "document.h"
#include "extension/db.h"
#include "extension/template.h"
#include "inkscape-application.h"
#include "io/resource.h"
#include "ui/builder-utils.h"
#include "ui/iconview-item-factory.h"
#include "ui/util.h"
#include "ui/svg-renderer.h"

using namespace Inkscape::IO::Resource;
using Inkscape::Extension::TemplatePreset;

namespace Inkscape::UI::Widget {

struct TemplateList::TemplateItem : public Glib::Object {
    Glib::ustring name;
    Glib::ustring label;
    Glib::ustring tooltip;
    Glib::RefPtr<Gdk::Texture> icon;
    Glib::ustring key;
    int priority;

    static Glib::RefPtr<TemplateItem> create(const Glib::ustring& name, const Glib::ustring& label, const Glib::ustring& tooltip, 
        Glib::RefPtr<Gdk::Texture> icon, Glib::ustring key, int priority) {

        auto item = Glib::make_refptr_for_instance<TemplateItem>(new TemplateItem());
        item->name = name;
        item->label = label;
        item->tooltip = tooltip;
        item->icon = icon;
        item->key = key;
        item->priority = priority;
        return item;
    }
private:
    TemplateItem() = default;
};


TemplateList::TemplateList(BaseObjectType *cobject, const Glib::RefPtr<Gtk::Builder> &refGlade)
    : Gtk::Notebook(cobject)
{
}

/**
 * Initialise this template list with categories and icons
 */
void TemplateList::init(Inkscape::Extension::TemplateShow mode)
{
    std::map<std::string, Glib::RefPtr<Gio::ListStore<TemplateItem>>> stores;

    Inkscape::Extension::DB::TemplateList extensions;
    Inkscape::Extension::db.get_template_list(extensions);

    for (auto tmod : extensions) {
        for (auto preset : tmod->get_presets(mode)) {
            auto const &cat = preset->get_category();

            if (auto it = stores.lower_bound(cat);
                it == stores.end() || it->first != cat)
            {
                try {
                    it = stores.emplace_hint(it, cat, generate_category(cat));
                    it->second->remove_all();
                } catch (UIBuilderError const& error) {
                    g_error("Error building templates %s\n", error.what());
                    return;
                }
            }

            auto& name = preset->get_name();
            auto& desc = preset->get_description();
            auto& label = preset->get_label();
            auto tooltip = _(desc.empty() ? name.c_str() : desc.c_str());
            auto trans_label = label.empty() ? "" : _(label.c_str());
            auto icon = to_texture(icon_to_pixbuf(preset->get_icon_path(), get_scale_factor()));

            stores[cat]->append(TemplateItem::create(
                Glib::Markup::escape_text(name),
                Glib::Markup::escape_text(trans_label),
                Glib::Markup::escape_text(tooltip),
                icon, preset->get_key(), preset->get_sort_priority()
            ));
        }
    }

    reset_selection();
}

/**
 * Turn the requested template icon name into a pixbuf
 */
Cairo::RefPtr<Cairo::ImageSurface> TemplateList::icon_to_pixbuf(std::string const &path, int scale)
{
    // TODO: cache to filesystem. This function is a major bottleneck for startup time (ca. 1 second)!
    // The current memory-based caching only catches the case where multiple templates share the same icon.
    static std::map<std::string, Cairo::RefPtr<Cairo::ImageSurface>> cache;
    if (path.empty()) {
        return {};
    }
    if (cache.contains(path)) {
        return cache[path];
    }
    Inkscape::svg_renderer renderer(path.c_str());
    auto result = renderer.render_surface(scale);
    cache[path] = result;
    return result;
}

/**
 * Generate a new category with the given label and return it's list store.
 */
Glib::RefPtr<Gio::ListStore<TemplateList::TemplateItem>> TemplateList::generate_category(std::string const &label)
{
    auto builder = create_builder("widget-new-from-template.ui");
    auto container = &get_widget<Gtk::ScrolledWindow>(builder, "container");
    auto icons     = &get_widget<Gtk::GridView>      (builder, "iconview");

    auto store = Gio::ListStore<TemplateItem>::create();
    auto sorter = Gtk::NumericSorter<int>::create(Gtk::ClosureExpression<int>::create([this](auto& item){
        auto ptr = std::dynamic_pointer_cast<TemplateList::TemplateItem>(item);
        return ptr ? ptr->priority : 0;
    }));
    auto model = Gtk::SortListModel::create(store, sorter);
    auto selection_model = Gtk::SingleSelection::create(model);
    selection_model->set_can_unselect();
    auto factory = IconViewItemFactory::create([](auto& ptr) -> IconViewItemFactory::ItemData {
        auto tmpl = std::dynamic_pointer_cast<TemplateItem>(ptr);
        if (!tmpl) return {};

        auto label = tmpl->name + "\n<small><span alpha=\"60%\" line_height=\"1.75\">" + tmpl->label + "</span></small>";
        return { .label_markup = label, .image = tmpl->icon, .tooltip = tmpl->tooltip };
    });
    icons->set_factory(factory->get_factory());
    icons->set_model(selection_model);

    // This packing keeps the Gtk widget alive, beyond the builder's lifetime
    append_page(*container, g_dpgettext2(nullptr, "TemplateCategory", label.c_str()));

    selection_model->signal_selection_changed().connect([this](auto pos, auto count){
        _item_selected_signal.emit();
    });
    icons->signal_activate().connect([this](auto pos){
        _item_activated_signal.emit();
    });

    _factory.emplace_back(std::move(factory));
    return store;
}

/**
 * Returns true if the template list has a visible, selected preset.
 */
bool TemplateList::has_selected_preset()
{
    return (bool)get_selected_preset();
}

/**
 * Returns the selected template preset, if one is not selected returns nullptr.
 */
std::shared_ptr<TemplatePreset> TemplateList::get_selected_preset()
{
    if (auto iconview = get_iconview(get_nth_page(get_current_page()))) {
        auto sel = std::dynamic_pointer_cast<Gtk::SingleSelection>(iconview->get_model());
        auto ptr = sel->get_selected_item();
        if (auto item = std::dynamic_pointer_cast<TemplateList::TemplateItem>(ptr)) {
            return Extension::Template::get_any_preset(item->key);
        }
    }
    return nullptr;
}

/**
 * Create a new document based on the selected item and return.
 */
SPDocument *TemplateList::new_document()
{
    auto app = InkscapeApplication::instance();
    if (auto preset = get_selected_preset()) {
        if (auto doc = preset->new_from_template()) {
            // TODO: Add memory to remember this preset for next time.
            return app->document_add(std::move(doc));
        } else {
            // Cancel pressed in options box.
            return nullptr;
        }
    }
    // Fallback to the default template (already added)!
    return app->document_new();
}

/**
 * Reset the selection, forcing the use of the default template.
 */
void TemplateList::reset_selection()
{
    // TODO: Add memory here for the new document default (see new_document).
    for (auto const widget : UI::get_children(*this)) {
        if (auto iconview = get_iconview(widget)) {
            auto sel = std::dynamic_pointer_cast<Gtk::SingleSelection>(iconview->get_model());
            sel->unselect_all();
        }
    }
}

/**
 * Returns the internal iconview for the given widget.
 */
Gtk::GridView *TemplateList::get_iconview(Gtk::Widget *widget)
{
    if (!widget) return nullptr;

    for (auto const child : UI::get_children(*widget)) {
        if (auto iconview = get_iconview(child)) {
            return iconview;
        }
    }

    return dynamic_cast<Gtk::GridView *>(widget);
}

} // namespace Inkscape::UI::Widget

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
