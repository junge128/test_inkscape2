// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Font browser and selector
 *
 * Copyright (C) 2022-2023 Michael Kowalski
 */

#ifndef INKSCAPE_UI_WIDGET_FONT_LIST_H
#define INKSCAPE_UI_WIDGET_FONT_LIST_H

#include <glibmm/ustring.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treepath.h>
#include <gtkmm/widget.h>
#include <memory>
#include <sigc++/connection.h>
#include <vector>
#include <gtkmm/builder.h>
#include <gtkmm/grid.h>
#include <gtkmm/box.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/iconview.h>
#include <gtkmm/treeview.h>
#include <gtkmm/listbox.h>
#include <gtkmm/liststore.h>
#include <gtkmm/scale.h>
#include <sigc++/scoped_connection.h>
#include "ui/widget/font-variations.h"
#include "ui/operation-blocker.h"
#include "util/font-discovery.h"
#include "util/font-tags.h"
#include "font-selector-interface.h"

namespace Inkscape::UI::Widget {

class FontList : public Gtk::Box, public FontSelectorInterface {
public:
    static std::unique_ptr<FontSelectorInterface> create_font_list(Glib::ustring pref_path);

    FontList(Glib::ustring preferences_path);

    // get font selected in this FontList, if any
    Glib::ustring get_fontspec() const override;
    double get_fontsize() const override;

    // show requested font in a FontList
    void set_current_font(const Glib::ustring& family, const Glib::ustring& face) override;
    // 
    void set_current_size(double size) override;

    sigc::signal<void ()>& signal_changed() override { return _signal_changed; }
    sigc::signal<void ()>& signal_apply() override { return _signal_apply; }

    Gtk::Widget* box() override { return this; }

    ~FontList() override = default;

    // no op, not used
    void set_model() override {};
    void unset_model() override {};

private:
    void sort_fonts(Inkscape::FontOrder order);
    void filter();
    struct Show {
        bool monospaced;
        bool oblique;
        bool others;
    };
    void populate_font_store(Glib::ustring text, const Show& params);
    void add_font(const Glib::ustring& fontspec, bool select);
    bool select_font(const Glib::ustring& fontspec);
    void update_font_count();
    void add_categories();
    void toggle_category();
    void update_categories(const std::string& tag, bool select);
    void update_filterbar();
    Gtk::Box* create_pill_box(const Glib::ustring& display_name, const Glib::ustring& tag, bool tags);
    void sync_font_tag(const FontTag* ftag, bool selected);
    void scroll_to_row(Gtk::TreePath path);
    Gtk::TreeModel::iterator get_selected_font() const;

    sigc::signal<void ()> _signal_changed;
    sigc::signal<void ()> _signal_apply;
    Glib::RefPtr<Gtk::Builder> _builder;
    Gtk::Grid& _main_grid;
    Gtk::TreeView& _font_list;
    Gtk::TreeViewColumn _text_column;
    Gtk::IconView& _font_grid;
    sigc::scoped_connection _selection_changed;
    Glib::RefPtr<Gtk::ListStore> _font_list_store;
    Gtk::Box& _tag_box;
    Gtk::Box& _info_box;
    Gtk::Box& _progress_box;
    std::vector<FontInfo> _fonts;
    Inkscape::FontOrder _order = Inkscape::FontOrder::by_name;
    Glib::ustring _filter;
    Gtk::ComboBoxText& _font_size;
    Gtk::Scale& _font_size_scale;
    std::unique_ptr<Gtk::CellRendererText> _cell_renderer;
    std::unique_ptr<Gtk::CellRenderer> _cell_icon_renderer;
    std::unique_ptr<Gtk::CellRendererText> _grid_renderer;
    Glib::ustring _current_fspec;
    double _current_fsize = 0.0;
    OperationBlocker _update;
    int _extra_fonts = 0;
    Gtk::ListBox& _tag_list;
    Inkscape::FontTags& _font_tags;
    FontVariations _font_variations;
    sigc::scoped_connection _scroll;
    Glib::ustring _prefs;
    bool _view_mode_list = true;
    sigc::scoped_connection _font_stream;
    std::size_t _initializing = 0;
    sigc::scoped_connection _font_collections_update;
    sigc::scoped_connection _font_collections_selection;
};

} // namespaces

#endif // INKSCAPE_UI_WIDGET_FONT_LIST_H
