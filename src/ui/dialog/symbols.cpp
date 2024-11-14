// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Symbols dialog.
 */
/* Authors:
 * Copyright (C) 2012 Tavmjong Bah
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "symbols.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <glibmm/refptr.h>
#include <gtkmm/gridview.h>
#include <gtkmm/object.h>
#include <gtkmm/widget.h>
#include <iostream>
#include <locale>
#include <memory>
#include <regex>
#include <sstream>
#include "libnrtype/font-factory.h"
using namespace std::literals;

#include <2geom/point.h>
#include <cairo.h>
#include <cairomm/refptr.h>
#include <cairomm/surface.h>
#include <glibmm/i18n.h>
#include <glibmm/main.h>
#include <glibmm/markup.h>
#include <glibmm/priorities.h>
#include <glibmm/regex.h>
#include <glibmm/stringutils.h>
#include <gdkmm/pixbuf.h>
#include <gdkmm/rgba.h>
#include <gtkmm/box.h>
#include <gtkmm/builder.h>
#include <gtkmm/cellrenderertext.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/dragsource.h>
#include <gtkmm/enums.h>
#include <gtkmm/iconview.h>
#include <gtkmm/label.h>
#include <gtkmm/menubutton.h>
#include <gtkmm/overlay.h>
#include <gtkmm/popover.h>
#include <gtkmm/scale.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/searchentry2.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treemodelfilter.h>
#include <gtkmm/treemodelsort.h>
#include <gtkmm/treepath.h>
#include <gtkmm/treeiter.h>
#include <pangomm/layout.h>
#include <2geom/point.h>
#include "ui/controller.h"
#include "ui/util.h"

#ifdef HAVE_CONFIG_H
# include "config.h"  // only include where actually required!
#endif

#include "document.h"
#include "document-undo.h"
#include "desktop.h"
#include "preferences.h"
#include "selection.h"
#include "include/gtkmm_version.h"

#include "io/resource.h"
#include "object/sp-defs.h"
#include "object/sp-root.h"
#include "object/sp-symbol.h"
#include "object/sp-use.h"
#include "ui/builder-utils.h"
#include "ui/cache/svg_preview_cache.h"
#include "ui/clipboard.h"
#include "ui/drag-and-drop.h"
#include "ui/icon-loader.h"
#include "ui/pack.h"
#include "util/statics.h"
#include "util/value-utils.h"
#include "xml/href-attribute-helper.h"

#ifdef WITH_LIBVISIO
    #include <libvisio/libvisio.h>
    #include <librevenge-stream/librevenge-stream.h>

    using librevenge::RVNGFileStream;
    using librevenge::RVNGString;
    using librevenge::RVNGStringVector;
    using librevenge::RVNGPropertyList;
    using librevenge::RVNGSVGDrawingGenerator;
#endif


namespace Inkscape::UI::Dialog {
namespace {

constexpr int SIZES = 51;
int SYMBOL_ICON_SIZES[SIZES];

struct SymbolSet
{
    std::unique_ptr<SPDocument> document;
    std::vector<SPSymbol *> symbols;
    Glib::ustring title;
};

struct SymbolSetView
{
    SPDocument *document;
    std::vector<SPSymbol *> symbols;
    Glib::ustring title;
};

struct SymbolSets : Util::EnableSingleton<SymbolSets, Util::Depends<FontFactory>>
{
    // key: symbol set full file name
    // value: symbol set
    std::map<std::string, SymbolSet> map;
};

static Cairo::RefPtr<Cairo::ImageSurface> g_dummy;

struct SymbolSetsColumns : public Gtk::TreeModel::ColumnRecord {
    Gtk::TreeModelColumn<Glib::ustring> set_id;
    Gtk::TreeModelColumn<Glib::ustring> translated_title;
    Gtk::TreeModelColumn<std::string>   set_filename;
    Gtk::TreeModelColumn<SPDocument*>   set_document;

    SymbolSetsColumns() {
        add(set_id);
        add(translated_title);
        add(set_filename);
        add(set_document);
    }
};
SymbolSetsColumns const g_set_columns;

const Glib::ustring CURRENT_DOC_ID = "{?cur-doc?}";
const Glib::ustring ALL_SETS_ID = "{?all-sets?}";
const char *CURRENT_DOC = N_("Current document");
const char *ALL_SETS = N_("All symbol sets");

} // namespace

struct SymbolItem : public Glib::Object {
    std::string unique_key;
    Glib::ustring symbol_id;
    Glib::ustring symbol_title;
    Glib::ustring symbol_label;
    Glib::ustring symbol_search_title;
    Cairo::RefPtr<Cairo::Surface> symbol_image;
    Geom::Point doc_dimensions;
    SPDocument* symbol_document;

    static Glib::RefPtr<SymbolItem> create(
        std::string unique_key,
        Glib::ustring symbol_id,
        Glib::ustring symbol_title,
        Glib::ustring symbol_label,
        Glib::ustring symbol_search_title,
        Geom::Point doc_dimensions,
        SPDocument* symbol_document
    ) {
        auto item = Glib::make_refptr_for_instance<SymbolItem>(new SymbolItem());
        item->unique_key = unique_key;
        item->symbol_id = symbol_id;
        item->symbol_title = symbol_title;
        item->symbol_label = symbol_label;
        item->symbol_search_title = symbol_search_title;
        // symbol_image is left empty
        item->doc_dimensions = doc_dimensions;
        item->symbol_document = symbol_document;
        return item;
    }
};

static SPDocument *load_symbol_set(std::string const &filename);
static void scan_all_symbol_sets();

SymbolsDialog::SymbolsDialog(const char* prefsPath)
    : DialogBase(prefsPath, "Symbols"),
    _builder(create_builder("dialog-symbols.glade")),
    _zoom(            get_widget<Gtk::Scale>      (_builder, "zoom")),
    _symbols_popup(   get_widget<Gtk::MenuButton> (_builder, "symbol-set-popup")),
    _set_search(      get_widget<Gtk::SearchEntry2>(_builder, "set-search")),
    _search(          get_widget<Gtk::SearchEntry2>(_builder, "search")),
    _symbol_sets_view(get_widget<Gtk::IconView>   (_builder, "symbol-sets")),
    _cur_set_name(   get_widget<Gtk::Label>       (_builder, "cur-set")),
    _image_cache(1000), // arbitrary limit for how many rendered symbols to keep around
    _gridview(get_widget<Gtk::GridView>(_builder, "icon-view"))
{
    auto prefs = Inkscape::Preferences::get();
    Glib::ustring path = prefsPath;
    path += '/';

    _symbol_sets = Gtk::ListStore::create(g_set_columns);
    _sets._store = _symbol_sets;
    _sets._filtered = Gtk::TreeModelFilter::create(_symbol_sets);
    _sets._filtered->set_visible_func([=, this](const Gtk::TreeModel::const_iterator& it){
        if (_set_search.get_text().length() == 0) return true;

        Glib::ustring id = (*it)[g_set_columns.set_id];
        if (id == CURRENT_DOC_ID || id == ALL_SETS_ID) return true;

        auto text = _set_search.get_text().lowercase();
        Glib::ustring title = (*it)[g_set_columns.translated_title];
        return title.lowercase().find(text) != Glib::ustring::npos;
    });
    _sets._sorted = Gtk::TreeModelSort::create(_sets._filtered);
    _sets._sorted->set_sort_func(g_set_columns.translated_title, [this](const Gtk::TreeModel::const_iterator& a, 
        const Gtk::TreeModel::const_iterator& b) -> int
    {
        Glib::ustring ida = (*a)[g_set_columns.set_id];
        Glib::ustring idb = (*b)[g_set_columns.set_id];
        // current doc and all docs up front
        if (ida == idb) return 0;
        if (ida == CURRENT_DOC_ID) return -1;
        if (idb == CURRENT_DOC_ID) return 1;
        if (ida == ALL_SETS_ID) return -1;
        if (idb == ALL_SETS_ID) return 1;
        Glib::ustring ttl_a = (*a)[g_set_columns.translated_title];
        Glib::ustring ttl_b = (*b)[g_set_columns.translated_title];
        return ttl_a.compare(ttl_b);
    });
    _symbol_sets_view.set_model(_sets._sorted);
    _symbol_sets_view.set_text_column(g_set_columns.translated_title.index());
    _symbol_sets_view.pack_start(_renderer2);

    auto row = _symbol_sets->append();
    (*row)[g_set_columns.set_id] = CURRENT_DOC_ID;
    (*row)[g_set_columns.translated_title] = _(CURRENT_DOC);
    row = _symbol_sets->append();
    (*row)[g_set_columns.set_id] = ALL_SETS_ID;
    (*row)[g_set_columns.translated_title] = _(ALL_SETS);

    _set_search.signal_search_changed().connect([this](){
        auto scoped(_update.block());
        _sets.refilter();
    });

    auto select_set = [=, this](const Gtk::TreeModel::Path& set_path) {
        if (!set_path.empty()) {
            // drive selection
            _symbol_sets_view.select_path(set_path);
        }
        else if (auto set = get_current_set()) {
            // populate icon view
            rebuild_set(*set);
            rebuild(false);
            _cur_set_name.set_text((**set)[g_set_columns.translated_title]);
            update_tool_buttons();
            Glib::ustring id = (**set)[g_set_columns.set_id];
            prefs->setString(path + "current-set", id);
            return true;
        }
        return false;
    };

    _selection_changed = _symbol_sets_view.signal_selection_changed().connect([=, this](){
        if (select_set({})) {
            get_widget<Gtk::Popover>(_builder, "set-popover").set_visible(false);
        }
    });

    const double factor = std::pow(2.0, 1.0 / 12.0);
    for (int i = 0; i < SIZES; ++i) {
        SYMBOL_ICON_SIZES[i] = std::round(std::pow(factor, i) * 16);
    }

    preview_document = symbolsPreviewDoc(); /* Template to render symbols in */
    key = SPItem::display_key_new(1);
    renderDrawing.setRoot(preview_document->getRoot()->invoke_show(renderDrawing, key, SP_ITEM_SHOW_DISPLAY));

    auto& main = get_widget<Gtk::Box>(_builder, "main-box");
    UI::pack_start(*this, main, UI::PackOptions::expand_widget);

    tools = &get_widget<Gtk::Box>(_builder, "tools");

    _symbol_store = Gio::ListStore<SymbolItem>::create();
    _filter = Gtk::BoolFilter::create({});
    _filtered_model = Gtk::FilterListModel::create(_symbol_store, _filter);
    _selection_model = Gtk::SingleSelection::create(_filtered_model);

    _factory = IconViewItemFactory::create([this](auto& ptr) -> IconViewItemFactory::ItemData {
        auto symbol = std::dynamic_pointer_cast<SymbolItem>(ptr);
        if (!symbol) return {};

        auto tex = get_image(symbol->unique_key, symbol->symbol_document, symbol->symbol_id);
        return { .label_markup = symbol->symbol_label, .image = tex, .tooltip = symbol->symbol_title };
    });
    _factory->set_track_bindings(true);

    _gridview.set_min_columns(1);
    // max columns impacts number of prerendered items requested by gridview (= maxcol * 32 + 1),
    // so it needs to be artificially kept low to prevent gridview from rendering all items up front
    _gridview.set_max_columns(5);
    // gtk_list_base_set_anchor_max_widgets(0); - private method, no way to customize number of prerendered items
    _gridview.set_model(_selection_model);
    _gridview.set_factory(_factory->get_factory());
    // handle item activation (double-click)
    _gridview.signal_activate().connect([this](auto pos){
        // TODO: insert symbol into document
    });

    _search.signal_search_changed().connect([this](){
        int delay = _search.get_text().length() == 0 ? 0 : 300;
        _idle_search = Glib::signal_timeout().connect([this](){
            auto scoped(_update.block());
            refilter();
            set_info();
            return false; // disconnect
        }, delay);
    });

    auto show_names = &get_widget<Gtk::CheckButton>(_builder, "show-names");
    auto names = prefs->getBool(path + "show-names", true);
    show_names->set_active(names);
    if (names) {
        _factory->set_include_label(names);
    }
    show_names->signal_toggled().connect([=, this](){
        bool show = show_names->get_active();
        _factory->set_include_label(show);
        rebuild(false);
        prefs->setBool(path + "show-names", show);
    });

    // find symbol list widget under the mouse cursor (x, y) as reported by d&d prepare call
    auto find_item = [this](double x, double y) {
        // iterate from last to first to avoid too large a bounding box trap reported by get_bounds
        for (auto child = _gridview.get_last_child(); child; child = child->get_prev_sibling()) {
            if (!child->get_child_visible()) continue;

            int bx, by, w, h;
            child->get_bounds(bx, by, w, h);
            if (x >= bx && x < bx+w && y >= by && y < by+h) {
                if (auto item = _factory->find_item(*child)) {
                    return std::dynamic_pointer_cast<SymbolItem>(item);
                }
            }
        }
        return std::shared_ptr<SymbolItem>();
    };

    auto const source = Gtk::DragSource::create();
    auto drag_prepare = [this, find_item, &source = *source](double x, double y) -> Glib::RefPtr<Gdk::ContentProvider> {
        auto dragged = find_item(x, y);
        if (!dragged) {
            return nullptr;
        }

        auto const dims = getSymbolDimensions(dragged);
        sendToClipboard(*dragged, Geom::Rect(-0.5 * dims, 0.5 * dims), false);

        return Gdk::ContentProvider::create(Util::GlibValue::create<DnDSymbol>(
            DnDSymbol{dragged->symbol_id, dragged->unique_key, dragged->symbol_document}
        ));
    };
    auto drag_begin = [this, &source = *source](Glib::RefPtr<Gdk::Drag> const &drag) {
        auto c = source.get_content();
        if (!c) return;

        auto symbol = Util::GlibValue::from_content_provider<DnDSymbol>(*c);

        auto tex = get_image(symbol->unique_key, symbol->document, symbol->id);
        // TODO: scale for high dpi display (somehow)
        int x = 0, y = 0;
        if (tex) {
            x = tex->get_intrinsic_width() / 2;
            y = tex->get_intrinsic_height() / 2;
        }
        source.set_icon(tex, x, y);
    };
    source->signal_prepare().connect(std::move(drag_prepare), false); // before
    source->signal_drag_begin().connect(std::move(drag_begin));
    _gridview.add_controller(source);

    scroller = &get_widget<Gtk::ScrolledWindow>(_builder, "scroller");

    // here we fix scoller to allow pass the scroll to parent scroll when reach upper or lower limit
    // this must be added to al scrolleing window in dialogs. We dont do auto because dialogs can be recreated
    // in the dialog code so think is safer call inside
    fix_inner_scroll(scroller);

    overlay = &get_widget<Gtk::Overlay>(_builder, "overlay");

    /*************************Overlays******************************/
    // No results
    overlay_icon = sp_get_icon_image("searching", Gtk::IconSize::LARGE);
    overlay_icon->set_pixel_size(40);
    overlay_icon->set_halign(Gtk::Align::CENTER);
    overlay_icon->set_valign(Gtk::Align::START);
    overlay_icon->set_margin_top(90);
    overlay_icon->set_visible(false);
  
    overlay_title = Gtk::make_managed<Gtk::Label>();
    overlay_title->set_halign(Gtk::Align::CENTER );
    overlay_title->set_valign(Gtk::Align::START );
    overlay_title->set_justify(Gtk::Justification::CENTER);
    overlay_title->set_margin_top(135);
    overlay_title->set_visible(false);
  
    overlay_desc = Gtk::make_managed<Gtk::Label>();
    overlay_desc->set_halign(Gtk::Align::CENTER);
    overlay_desc->set_valign(Gtk::Align::START);
    overlay_desc->set_margin_top(160);
    overlay_desc->set_justify(Gtk::Justification::CENTER);
    overlay_desc->set_visible(false);

    overlay->add_overlay(*overlay_icon);
    overlay->add_overlay(*overlay_title);
    overlay->add_overlay(*overlay_desc);

    previous_height = 0;
    previous_width = 0;

    /******************** Tools *******************************/

    add_symbol = &get_widget<Gtk::Button>(_builder, "add-symbol");
    add_symbol->signal_clicked().connect([this](){ convert_object_to_symbol(); });

    remove_symbol = &get_widget<Gtk::Button>(_builder, "remove-symbol");
    remove_symbol->signal_clicked().connect([this](){ revertSymbol(); });

    _copy_symbol = &get_widget<Gtk::Button>(_builder, "copy-symbol");
    _copy_symbol->signal_clicked().connect([this](){ copy_symbol(); });

    // Pack size (controls display area)
    pack_size = prefs->getIntLimited(path + "tile-size", 12, 0, SIZES);

    auto scale = &get_widget<Gtk::Scale>(_builder, "symbol-size");
    scale->set_value(pack_size);
    scale->signal_value_changed().connect([=, this](){
        pack_size = scale->get_value();
        assert(pack_size >= 0 && pack_size < SIZES);
        _image_cache.clear();
        rebuild(true);
        prefs->setInt(path + "tile-size", pack_size);
    });

    scale_factor = prefs->getIntLimited(path + "scale-factor", 0, -10, +10);

    _zoom.set_value(scale_factor);
    _zoom.signal_value_changed().connect([=, this](){
        scale_factor = _zoom.get_value();
        rebuild(true);
        prefs->setInt(path + "scale-factor", scale_factor);
    });

    // Toggle scale to fit on/off
    fit_symbol = &get_widget<Gtk::CheckButton>(_builder, "zoom-to-fit");
    auto fit = prefs->getBool(path + "zoom-to-fit", true);
    fit_symbol->set_active(fit);
    fit_symbol->signal_toggled().connect([=, this](){
        rebuild(true);
        prefs->setBool(path + "zoom-to-fit", fit_symbol->get_active());
    });

    scan_all_symbol_sets();

    for (auto const &it : SymbolSets::get().map) {
        auto row = _symbol_sets->append();
        auto& set = it.second;
        (*row)[g_set_columns.set_id] = it.first;
        (*row)[g_set_columns.translated_title] = g_dpgettext2(nullptr, "Symbol", set.title.c_str());
        (*row)[g_set_columns.set_document] = set.document.get();
        (*row)[g_set_columns.set_filename] = it.first;
    }

    // last selected set
    auto current = prefs->getString(path + "current-set", CURRENT_DOC_ID);

    // by default select current doc (first on the list) in case nothing else gets selected
    select_set(Gtk::TreeModel::Path("0"));

    sensitive = true;

    // restore set selection; check if it is still available first
    _sets._sorted->foreach_path([&](const Gtk::TreeModel::Path& path){
        auto it = _sets.path_to_child_iter(path);
        Glib::ustring id = (*it)[g_set_columns.set_id];
        if (current == id) {
            select_set(path);
            return true;
        }
        return false;
    });
}

SymbolsDialog::~SymbolsDialog()
{
    preview_document->getRoot()->invoke_hide(key);
}

bool SymbolsDialog::is_item_visible(const Glib::RefPtr<Glib::ObjectBase>& item) const {
    auto symbol_ptr = std::dynamic_pointer_cast<SymbolItem>(item);
    if (!symbol_ptr) return false;

    const auto& symbol = *symbol_ptr;

    // filter by name
    auto str = _search.get_text().lowercase();
    if (str.empty()) return true;

    Glib::ustring text = symbol.symbol_search_title;
    return text.lowercase().find(str) != Glib::ustring::npos;
}

void SymbolsDialog::refilter() {
    // When a new expression is set in the BoolFilter, it emits signal_changed(),
    // and the FilterListModel re-evaluates the filter.
    auto expression = Gtk::ClosureExpression<bool>::create([this](auto& item){ return is_item_visible(item); });
    // filter results
    _filter->set_expression(expression);
}

void SymbolsDialog::rebuild(bool clear_image_cache) {
    // empty cache, so item will get re-rendered at new size
    if (clear_image_cache) {
        _image_cache.clear();
    }
    // remove all
    auto none = Gtk::ClosureExpression<bool>::create([this](auto& item){ return false; });
    _filter->set_expression(none);
    // restore
    refilter();
}

void collect_symbols(SPObject* object, std::vector<SPSymbol*>& symbols) {
    if (!object) return;

    if (auto symbol = cast<SPSymbol>(object)) {
        symbols.push_back(symbol);
    }

    if (is<SPUse>(object)) return;

    for (auto& child : object->children) {
        collect_symbols(&child, symbols);
    }
}

void SymbolsDialog::load_all_symbols() {
    _sets._store->foreach_iter([this](const Gtk::TreeModel::iterator& it){
        if (!(*it)[g_set_columns.set_document]) {
            std::string path = (*it)[g_set_columns.set_filename];
            if (!path.empty()) {
                auto doc = load_symbol_set(path);
                (*it)[g_set_columns.set_document] = doc;
            }
        }
        return false;
    });
}

std::map<std::string, SymbolSetView> get_all_symbols(Glib::RefPtr<Gtk::ListStore> &store)
{
    std::map<std::string, SymbolSetView> map;

    store->foreach_iter([&] (Gtk::TreeModel::iterator const &it) {
        if (SPDocument *doc = (*it)[g_set_columns.set_document]) {
            SymbolSetView vect;
            collect_symbols(doc->getRoot(), vect.symbols);
            vect.title = (*it)[g_set_columns.translated_title];
            vect.document = doc;
            Glib::ustring id = (*it)[g_set_columns.set_id];
            map[id.raw()] = vect;
        }
        return false;
    });

    return map;
}

void SymbolsDialog::rebuild_set(Gtk::TreeModel::iterator current) {
    if (!sensitive || !current) {
        return;
    }

    auto pending = _update.block();

    _symbol_store->remove_all();

    auto it = current;

    std::map<std::string, SymbolSetView> symbols;

    SPDocument* document = (*it)[g_set_columns.set_document];
    Glib::ustring set_id = (*it)[g_set_columns.set_id];

    if (!document) {
        if (set_id == CURRENT_DOC_ID) {
            document = getDocument();
        }
        else if (set_id == ALL_SETS_ID) {
            // load symbol sets, if not yet open
            load_all_symbols();
            // get symbols from all symbol sets (apart from current document)
            symbols = get_all_symbols(_sets._store);
        }
        else {
            std::string path = (*it)[g_set_columns.set_filename];
            // load symbol set
            document = load_symbol_set(path);
            (*it)[g_set_columns.set_document] = document;
        }
    }

    if (document) {
        auto& vect = symbols[set_id.raw()];
        collect_symbols(document->getRoot(), vect.symbols);
        vect.document = set_id == CURRENT_DOC_ID ? nullptr : document;
        vect.title = (*it)[g_set_columns.translated_title];
    }

    size_t n = 0;
    for (auto&& it : symbols) {
        auto& set = it.second;
        for (auto symbol : set.symbols) {
            addSymbol(symbol, set.title, set.document);
        }
        n += set.symbols.size();
    }

#if false
    // layout speed test:
    Gtk::Allocation alloc;
    alloc.set_width(200);
    alloc.set_height(500);
    alloc.set_x(0);
    alloc.set_y(0);
    auto old_time =  std::chrono::high_resolution_clock::now();
    icon_view->size_allocate(alloc);
    auto current_time =  std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - old_time);
    g_warning("size time: %d ms", static_cast<int>(elapsed.count()));
#endif

    set_info();
}

void SymbolsDialog::showOverlay() {
    auto search = _search.get_text().length() > 0;
    auto visible = visible_symbols();
    auto current = get_current_set_id() == CURRENT_DOC_ID;

    auto small = [](const char* str){
        return Glib::ustring::compose("<small>%1</small>", Glib::Markup::escape_text(str));
    };
    auto large = [](const char* str){
        return Glib::ustring::compose("<span size='large'>%1</span>", Glib::Markup::escape_text(str));
    };

    if (!visible && search) {
        overlay_title->set_markup(large(_("No symbols found.")));
        overlay_desc->set_markup(small(_("Try a different search term,\nor switch to a different symbol set.")));
    } else if (!visible && current) {
        overlay_title->set_markup(large(_("No symbols found.")));
        overlay_desc->set_markup(small(_("No symbols in current document.\nChoose a different symbol set\nor add a new symbol.")));
    }

  /*
  if (current == ALLDOCS && !_l.size())
  {
    overlay_icon->set_visible(false);
    if (!all_docs_processed) {
        overlay_icon->set_visible(true);
        overlay_title->set_markup(
            Glib::ustring("<span size=\"large\">") + _("Search in all symbol sets...") + "</span>");
        overlay_desc->set_markup(
            Glib::ustring("<span size=\"small\">") + _("The first search can be slow.") + "</span>");
    } else if (!icons_found && !search_str.empty()) {
        overlay_title->set_markup(
            Glib::ustring("<span size=\"large\">") + _("No symbols found.") + "</span>");
        overlay_desc->set_markup(
            Glib::ustring("<span size=\"small\">") + _("Try a different search term.") + "</span>");
    } else {
        overlay_icon->set_visible(true);
        overlay_title->set_markup(Glib::ustring("<span size=\"large\">") +
                                  Glib::ustring(_("Search in all symbol sets...")) + Glib::ustring("</span>"));
        overlay_desc->set_markup(Glib::ustring("<span size=\"small\">") + Glib::ustring("</span>"));
    }
  } else if (!number_symbols && (current != CURRENTDOC || !search_str.empty())) {
      overlay_title->set_markup(Glib::ustring("<span size=\"large\">") + Glib::ustring(_("No symbols found.")) +
                                Glib::ustring("</span>"));
      overlay_desc->set_markup(Glib::ustring("<span size=\"small\">") +
                               Glib::ustring(_("Try a different search term,\nor switch to a different symbol set.")) +
                               Glib::ustring("</span>"));
  } else if (!number_symbols && current == CURRENTDOC) {
      overlay_title->set_markup(Glib::ustring("<span size=\"large\">") + Glib::ustring(_("No symbols found.")) +
                                Glib::ustring("</span>"));
      overlay_desc->set_markup(
          Glib::ustring("<span size=\"small\">") +
          Glib::ustring(_("No symbols in current document.\nChoose a different symbol set\nor add a new symbol.")) +
          Glib::ustring("</span>"));
  } else if (!icons_found && !search_str.empty()) {
      overlay_title->set_markup(Glib::ustring("<span size=\"large\">") + Glib::ustring(_("No symbols found.")) +
                                Glib::ustring("</span>"));
      overlay_desc->set_markup(Glib::ustring("<span size=\"small\">") +
                               Glib::ustring(_("Try a different search term,\nor switch to a different symbol set.")) +
                               Glib::ustring("</span>"));
  }
  */
  gint width = scroller->get_allocated_width();
  gint height = scroller->get_allocated_height();
  if (previous_height != height || previous_width != width) {
      previous_height = height;
      previous_width = width;
  }
  overlay_icon->set_visible(true);
  overlay_title->set_visible(true);
  overlay_desc->set_visible(true);
}

void SymbolsDialog::hideOverlay() {
    // overlay_opacity->set_visible(false);
    overlay_icon->set_visible(false);
    overlay_title->set_visible(false);
    overlay_desc->set_visible(false);
}

// Convert selection to <symbol>
void SymbolsDialog::convert_object_to_symbol() {
    getDesktop()->getSelection()->toSymbol();
}

void SymbolsDialog::revertSymbol() {
    if (auto document = getDocument()) {
        if (auto current = SymbolsDialog::get_selected_symbol()) {
            if (auto symbol = cast<SPSymbol>(document->getObjectById(current->symbol_id))) {
                symbol->unSymbol();
                Inkscape::DocumentUndo::done(document, _("Group from symbol"), "");
            }
        }
    }
}

void SymbolsDialog::selectionChanged(Inkscape::Selection *selection) {
    // what are we trying to do here? this code doesn't seem to accomplish anything in v1.2
/*
  auto selected = getSelected();
  Glib::ustring symbol_id = getSymbolId(selected);
  Glib::ustring doc_title = get_active_base_text(getSymbolDocTitle(selected));
  if (!doc_title.empty()) {
    SPDocument* symbol_document = symbol_sets[doc_title].second;
    if (!symbol_document) {
      //we are in global search so get the original symbol document by title
      symbol_document = selectedSymbols();
    }
    if (symbol_document) {
      SPObject* symbol = symbol_document->getObjectById(symbol_id);
      if(symbol && !selection->includes(symbol)) {
          icon_view->unselect_all();
      }
    }
  }
  */
}

void SymbolsDialog::refresh_on_idle(int delay) {
    // if symbols from current document are presented...
    if (get_current_set_id() == CURRENT_DOC_ID) {
        // refresh them on idle; delay here helps to coalesce consecutive requests into one
        _idle_refresh = Glib::signal_timeout().connect([this](){
            rebuild_set(*get_current_set());
            return false; // disconnect
        }, delay, Glib::PRIORITY_DEFAULT_IDLE);
    }
}

void SymbolsDialog::documentReplaced()
{
    _defs_modified.disconnect();
    _doc_resource_changed.disconnect();

    if (auto document = getDocument()) {
        _defs_modified = document->getDefs()->connectModified([this](SPObject* ob, guint flags) {
            refresh_on_idle();
        });
        _doc_resource_changed = document->connectResourcesChanged("symbol", [this](){
            refresh_on_idle();
        });
    }

    // if symbol set is from current document, need to rebuild
    refresh_on_idle(0);
    update_tool_buttons();
}

void SymbolsDialog::update_tool_buttons() {
    if (get_current_set_id() == CURRENT_DOC_ID) {
        add_symbol->set_sensitive();
        remove_symbol->set_sensitive();
    }
    else {
        add_symbol->set_sensitive(false);
        remove_symbol->set_sensitive(false);
    }
}

Glib::ustring SymbolsDialog::get_current_set_id() const {
    auto cur = get_current_set();
    if (cur.has_value()) {
        return (*cur.value())[g_set_columns.set_id];
    }
    return {};
}

std::optional<Gtk::TreeModel::iterator> SymbolsDialog::get_current_set() const {
    auto selected = _symbol_sets_view.get_selected_items();
    if (selected.empty()) {
        return std::nullopt;
    }
    return _sets.path_to_child_iter(selected.front());
}

std::shared_ptr<SymbolItem> SymbolsDialog::get_selected_symbol() const {
    auto item = _selection_model->get_selected_item();
    auto symbol = std::dynamic_pointer_cast<SymbolItem>(item);
    return symbol;
}

/** Return the dimensions of the symbol at the given path, in document units. */
Geom::Point SymbolsDialog::getSymbolDimensions(const std::shared_ptr<SymbolItem>& symbol) const
{
    if (!symbol) {
        return Geom::Point();
    }
    return symbol->doc_dimensions;
}

/** Store the symbol in the clipboard for further manipulation/insertion into document.
 *
 * @param symbol_path The path to the symbol in the tree model.
 * @param bbox The bounding box to set on the clipboard document's clipnode.
 */
void SymbolsDialog::sendToClipboard(const SymbolItem& symbol, Geom::Rect const &bbox, bool set_clipboard) {
    auto symbol_id = symbol.symbol_id;// getSymbolId(symbol_iter);
    if (symbol_id.empty()) return;

    auto symbol_document = symbol.symbol_document;
    const char* doc_name = nullptr;
    // auto symbol_document = get_symbol_document(symbol_iter);
    if (symbol_document) {
        doc_name = symbol_document->getDocumentName();
    }
    else {
        //we are in global search so get the original symbol document by title
        symbol_document = getDocument();
    }
    if (!symbol_document) {
        g_message("Cannot copy onto a clipboard symbol without document");
        return;
    }
    if (SPObject* symbol = symbol_document->getObjectById(symbol_id)) {
        // Find style for use in <use>
        // First look for default style stored in <symbol>
        gchar const* style = symbol->getAttribute("inkscape:symbol-style");
        if (!style) {
            // If no default style in <symbol>, look in documents.
            if (symbol_document == getDocument()) {
                style = styleFromUse(symbol_id.c_str(), symbol_document);
            } else {
                style = symbol_document->getReprRoot()->attribute("style");
            }
        }
        ClipboardManager::get()->copySymbol(symbol->getRepr(), style, symbol_document, doc_name, bbox, set_clipboard);
    }
}

void SymbolsDialog::copy_symbol() // handler for symbol double-click
{
    if (_update.pending()) return;

    if (auto selected = get_selected_symbol()) {
        auto const dims = getSymbolDimensions(selected);
        sendToClipboard(*selected, Geom::Rect(-0.5 * dims, 0.5 * dims), true);
    }
}

#ifdef WITH_LIBVISIO

// Extend libvisio's native RVNGSVGDrawingGenerator with support for extracting stencil names (to be used as ID/title)
class REVENGE_API RVNGSVGDrawingGenerator_WithTitle : public RVNGSVGDrawingGenerator {
  public:
    RVNGSVGDrawingGenerator_WithTitle(RVNGStringVector &output, RVNGStringVector &titles, const RVNGString &nmSpace)
      : RVNGSVGDrawingGenerator(output, nmSpace)
      , _titles(titles)
    {}

    void startPage(const RVNGPropertyList &propList) override
    {
      RVNGSVGDrawingGenerator::startPage(propList);
      if (propList["draw:name"]) {
          _titles.append(propList["draw:name"]->getStr());
      } else {
          _titles.append("");
      }
    }

  private:
    RVNGStringVector &_titles;
};

// Read Visio stencil files
static std::unique_ptr<SPDocument> read_vss(std::string filename, std::string name)
{
  char *fullname;
  #ifdef _WIN32
    // RVNGFileStream uses fopen() internally which unfortunately only uses ANSI encoding on Windows
    // therefore attempt to convert uri to the system codepage
    // even if this is not possible the alternate short (8.3) file name will be used if available
    fullname = g_win32_locale_filename_from_utf8(filename.c_str());
  #else
    fullname = g_strdup(filename.c_str());
  #endif

  RVNGFileStream input(fullname);
  g_free(fullname);

  if (!libvisio::VisioDocument::isSupported(&input)) {
    return nullptr;
  }
  RVNGStringVector output;
  RVNGStringVector titles;
  RVNGSVGDrawingGenerator_WithTitle generator(output, titles, "svg");

  if (!libvisio::VisioDocument::parseStencils(&input, &generator)) {
    return nullptr;
  }
  if (output.empty()) {
    return nullptr;
  }

  // prepare a valid title for the symbol file
  Glib::ustring title = Glib::Markup::escape_text(name);
  // prepare a valid id prefix for symbols libvisio doesn't give us a name for
  Glib::RefPtr<Glib::Regex> regex1 = Glib::Regex::create("[^a-zA-Z0-9_-]");
  Glib::ustring id = regex1->replace(name.c_str(), 0, "_", Glib::Regex::MatchFlags::PARTIAL);

  Glib::ustring tmpSVGOutput;
  tmpSVGOutput += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n";
  tmpSVGOutput += "<svg\n";
  tmpSVGOutput += "  xmlns=\"http://www.w3.org/2000/svg\"\n";
  tmpSVGOutput += "  xmlns:svg=\"http://www.w3.org/2000/svg\"\n";
  tmpSVGOutput += "  xmlns:xlink=\"http://www.w3.org/1999/xlink\"\n";
  tmpSVGOutput += "  version=\"1.1\"\n";
  tmpSVGOutput += "  style=\"fill:none;stroke:#000000;stroke-width:2\">\n";
  tmpSVGOutput += "  <title>";
  tmpSVGOutput += title;
  tmpSVGOutput += "</title>\n";
  tmpSVGOutput += "  <defs>\n";

  // Each "symbol" is in its own SVG file, we wrap with <symbol> and merge into one file.
  for (unsigned i=0; i<output.size(); ++i) {
    std::stringstream ss;
    if (titles.size() == output.size() && titles[i] != "") {
      // TODO: Do we need to check for duplicated titles?
      ss << regex1->replace(titles[i].cstr(), 0, "_", Glib::Regex::MatchFlags::PARTIAL);
    } else {
      ss << id << "_" << i;
    }

    tmpSVGOutput += "<symbol id=\"" + ss.str() + "\">\n";

    if (titles.size() == output.size() && titles[i] != "") {
      tmpSVGOutput += "<title>" + Glib::ustring(RVNGString::escapeXML(titles[i].cstr()).cstr()) + "</title>\n";
    }

    std::istringstream iss( output[i].cstr() );
    std::string line;
    while( std::getline( iss, line ) ) {
      if( line.find( "svg:svg" ) == std::string::npos ) {
        tmpSVGOutput += line + "\n";
      }
    }

    tmpSVGOutput += "</symbol>\n";
  }

  tmpSVGOutput += "  </defs>\n";
  tmpSVGOutput += "</svg>\n";
  return SPDocument::createNewDocFromMem(tmpSVGOutput.raw(), false);
}
#endif

/* Hunts preference directories for symbol files */
void scan_all_symbol_sets()
{
    using namespace Inkscape::IO::Resource;
    std::regex matchtitle(".*?<title.*?>(.*?)<(/| /)");

    auto &symbol_sets = SymbolSets::get().map;

    for (auto const &filename : get_filenames(SYMBOLS, {".svg", ".vss", "vssx", "vsdx"})) {
        if (symbol_sets.contains(filename)) continue;

        if (Glib::str_has_suffix(filename, ".vss") || Glib::str_has_suffix(filename, ".vssx") || Glib::str_has_suffix(filename, ".vsdx")) {
            std::size_t found = filename.find_last_of("/\\");
            auto title = found != Glib::ustring::npos ? filename.substr(found + 1) : filename;
            title = title.erase(title.rfind('.'));
            if (title.empty()) {
                title = _("Unnamed Symbols");
            }
            symbol_sets[filename].title = title;
        } else {
            std::ifstream infile(filename);
            std::string line;
            while (std::getline(infile, line)) {
                std::string title_res = std::regex_replace(line, matchtitle,"$1",std::regex_constants::format_no_copy);
                if (!title_res.empty()) {
                    title_res = g_dpgettext2(nullptr, "Symbol", title_res.c_str());
                    symbol_sets[filename].title = title_res;
                    break;
                }
                auto position_exit = line.find("<defs");
                if (position_exit != std::string::npos) {
                    std::size_t found = filename.find_last_of("/\\");
                    auto title = found != std::string::npos ? filename.substr(found + 1) : filename;
                    title = title.erase(title.rfind('.'));
                    if (title.empty()) {
                        title = _("Unnamed Symbols");
                    }
                    symbol_sets[filename].title = title;
                    break;
                }
            }
        }
    }
}

// Load SVG or VSS document and create SPDocument
SPDocument *load_symbol_set(std::string const &filename)
{
    auto &symbol_sets = SymbolSets::get().map;

    if (auto doc = symbol_sets[filename].document.get()) {
        return doc;
    }

    std::unique_ptr<SPDocument> symbol_doc;

    using namespace Inkscape::IO::Resource;
    if (Glib::str_has_suffix(filename, ".vss") || Glib::str_has_suffix(filename, ".vssx") || Glib::str_has_suffix(filename, ".vsdx")) {
#ifdef WITH_LIBVISIO
        symbol_doc = read_vss(filename, symbol_sets[filename].title);
#endif
    } else if (Glib::str_has_suffix(filename, ".svg")) {
        symbol_doc = SPDocument::createNewDoc(filename.c_str(), false);
    }

    if (!symbol_doc) {
        return nullptr;
    }

    return (symbol_sets[filename].document = std::move(symbol_doc)).get();
}

void SymbolsDialog::useInDoc (SPObject *r, std::vector<SPUse*> &l)
{
  if (is<SPUse>(r) ) {
    l.push_back(cast<SPUse>(r));
  }

  for (auto& child: r->children) {
    useInDoc( &child, l );
  }
}

std::vector<SPUse*> SymbolsDialog::useInDoc( SPDocument* useDocument) {
  std::vector<SPUse*> l;
  useInDoc (useDocument->getRoot(), l);
  return l;
}

// Returns style from first <use> element found that references id.
// This is a last ditch effort to find a style.
gchar const* SymbolsDialog::styleFromUse( gchar const* id, SPDocument* document) {

  gchar const* style = nullptr;
  std::vector<SPUse*> l = useInDoc( document );
  for( auto use:l ) {
    if ( use ) {
      gchar const *href = Inkscape::getHrefAttribute(*use->getRepr()).second;
      if( href ) {
        Glib::ustring href2(href);
        Glib::ustring id2(id);
        id2 = "#" + id2;
        if( !href2.compare(id2) ) {
          style = use->getRepr()->attribute("style");
          break;
        }
      }
    }
  }
  return style;
}

size_t SymbolsDialog::total_symbols() const {
    return _symbol_store->get_n_items();
    // return _symbols._store->children().size();
}

size_t SymbolsDialog::visible_symbols() const {
    return _selection_model->get_n_items();
    // return _filtered_model->get_n_items();
    // return _symbols._filtered->children().size();
}

void SymbolsDialog::set_info() {
    auto total = total_symbols();
    auto visible = visible_symbols();
    if (!total) {
        set_info("");
    }
    else if (total == visible) {
        set_info(Glib::ustring::compose("%1: %2", _("Symbols"), total).c_str());
    }
    else if (visible == 0) {
        set_info(Glib::ustring::compose("%1: %2 / %3", _("Symbols"), _("none"), total).c_str());
    }
    else {
        set_info(Glib::ustring::compose("%1: %2 / %3", _("Symbols"), visible, total).c_str());
    }

    if (total == 0 || visible == 0) {
        showOverlay();
    }
    else {
        hideOverlay();
    }
}

void SymbolsDialog::set_info(const Glib::ustring& text) {
    auto info = "<small>" + Glib::Markup::escape_text(text) + "</small>";
    get_widget<Gtk::Label>(_builder, "info").set_markup(info);
}

Cairo::RefPtr<Cairo::Surface> add_background(Cairo::RefPtr<Cairo::Surface> image,
                                             uint32_t rgb,
                                             double margin,
                                             double radius,
                                             unsigned size,
                                             int device_scale,
                                             std::optional<uint32_t> border = {})
{
    int total_size = size + 2 * margin;

    auto surface = Cairo::ImageSurface::create(Cairo::Surface::Format::ARGB32, total_size * device_scale, total_size * device_scale);
    cairo_surface_set_device_scale(surface->cobj(), device_scale, device_scale);
    auto ctx = Cairo::Context::create(surface);

    auto x = 0;
    auto y = 0;
    if (border.has_value()) {
        x += 0.5 * device_scale;
        y += 0.5 * device_scale;
        total_size -= device_scale;
    }
    ctx->arc(x + total_size - radius, y + radius, radius, -M_PI_2, 0);
    ctx->arc(x + total_size - radius, y + total_size - radius, radius, 0, M_PI_2);
    ctx->arc(x + radius, y + total_size - radius, radius, M_PI_2, M_PI);
    ctx->arc(x + radius, y + radius, radius, M_PI, 3 * M_PI_2);
    ctx->close_path();

    ctx->set_source_rgb(SP_RGBA32_R_F(rgb), SP_RGBA32_G_F(rgb), SP_RGBA32_B_F(rgb));
    if (border.has_value()) {
        ctx->fill_preserve();

        auto b = *border;
        ctx->set_source_rgb(SP_RGBA32_R_F(b), SP_RGBA32_G_F(b), SP_RGBA32_B_F(b));
        ctx->set_line_width(1.0);
        ctx->stroke();
    }
    else {
        ctx->fill();
    }

    if (image) {
        ctx->set_source(image, margin, margin);
        ctx->paint();
    }

    return surface;
}

void SymbolsDialog::addSymbol(SPSymbol* symbol, Glib::ustring doc_title, SPDocument* document)
{
    auto id = symbol->getRepr()->attribute("id");
    auto title = symbol->title(); // From title element
    Glib::ustring short_title = title ? g_dpgettext2(nullptr, "Symbol", title) : id;
    g_free(title);
    auto symbol_title = Glib::ustring::compose("%1 (%2)", short_title, doc_title);

    Geom::Point dimensions{64, 64}; // Default to 64x64 px if size not available.
    if (auto rect = symbol->documentVisualBounds()) {
        dimensions = rect->dimensions();
    }
    auto set = symbol->document ? symbol->document->getDocumentFilename() : "null";
    if (!set) set = "noname";
    std::ostringstream key;
    key << set << '\n' << id;

    _symbol_store->append(SymbolItem::create(
        key.str(),
        id,
        // symbol title and document name - used in a tooltip
        Glib::Markup::escape_text(symbol_title),
        // symbol title shown below image
        "<small>" + Glib::Markup::escape_text(short_title) + "</small>",
        // symbol title verbatim, used for searching/filtering
        short_title,
        dimensions,
        document
    ));
}

Cairo::RefPtr<Cairo::Surface> SymbolsDialog::draw_symbol(SPSymbol* symbol) {
    Cairo::RefPtr<Cairo::Surface> surface;
    Cairo::RefPtr<Cairo::Surface> image;
    int device_scale = get_scale_factor();

    if (symbol) {
        image = drawSymbol(symbol);
    }
    else {
        unsigned psize = SYMBOL_ICON_SIZES[pack_size] * device_scale;
        image = Cairo::ImageSurface::create(Cairo::Surface::Format::ARGB32, psize, psize);
        cairo_surface_set_device_scale(image->cobj(), device_scale, device_scale);
    }

    // white background for typically black symbols, so they don't disappear in a dark theme
    if (image) {
        uint32_t background = 0xffffff00;
        double margin = 3.0;
        double radius = 3.0;
        surface = add_background(image, background, margin, radius, SYMBOL_ICON_SIZES[pack_size], device_scale);
    }

    return surface;
}

/*
 * Returns image of symbol.
 *
 * Symbols normally are not visible. They must be referenced by a
 * <use> element.  A temporary document is created with a dummy
 * <symbol> element and a <use> element that references the symbol
 * element. Each real symbol is swapped in for the dummy symbol and
 * the temporary document is rendered.
 */
Cairo::RefPtr<Cairo::Surface> SymbolsDialog::drawSymbol(SPSymbol *symbol)
{
    if (!symbol) return Cairo::RefPtr<Cairo::Surface>();

    // Create a copy repr of the symbol with id="the_symbol"
    Inkscape::XML::Node *repr = symbol->getRepr()->duplicate(preview_document->getReprDoc());
    repr->setAttribute("id", "the_symbol");
  
    // First look for default style stored in <symbol>
    gchar const* style = repr->attribute("inkscape:symbol-style");
    if (!style) {
        // If no default style in <symbol>, look in documents.
        if (symbol->document == getDocument()) {
            gchar const *id = symbol->getRepr()->attribute("id");
            style = styleFromUse( id, symbol->document );
        } else {
            style = symbol->document->getReprRoot()->attribute("style");
        }
    }
  
    // This is for display in Symbols dialog only
    if (style) repr->setAttribute( "style", style );

    SPDocument::install_reference_document scoped(preview_document.get(), symbol->document);
    preview_document->getDefs()->getRepr()->appendChild(repr);
    Inkscape::GC::release(repr);
  
    // Uncomment this to get the preview_document documents saved (useful for debugging)
    // FILE *fp = fopen (g_strconcat(id, ".svg", NULL), "w");
    // sp_repr_save_stream(preview_document->getReprDoc(), fp);
    // fclose (fp);
  
    // Make sure preview_document is up-to-date.
    preview_document->ensureUpToDate();
  
    // Make sure we have symbol in preview_document
    SPObject *object_temp = preview_document->getObjectById( "the_use" );
  
    auto item = cast<SPItem>(object_temp);
    g_assert(item != nullptr);
    unsigned psize = SYMBOL_ICON_SIZES[pack_size];
  
    cairo_surface_t* surface = 0;
    // We could use cache here, but it doesn't really work with the structure
    // of this user interface and we've already cached the pixbuf in the gtklist
  
    // Find object's bbox in document.
    // Note symbols can have own viewport... ignore for now.
    Geom::OptRect dbox = item->documentVisualBounds();
  
    if (dbox) {
        /* Scale symbols to fit */
        double scale = 1.0;
        double width  = dbox->width();
        double height = dbox->height();
  
        if (width  == 0.0) width  = 1.0;
        if (height == 0.0) height = 1.0;
  
        if (fit_symbol->get_active()) {
            scale = psize / ceil(std::max(width, height));
        } else {
            scale = pow(2.0, scale_factor / 4.0) * psize / 32.0;
        }
  
        int device_scale = get_scale_factor();
        surface = render_surface(renderDrawing, scale, *dbox, Geom::IntPoint(psize, psize), device_scale, nullptr, true);
        if (surface) {
            cairo_surface_set_device_scale(surface, device_scale, device_scale);
        }
    }
  
    preview_document->getObjectByRepr(repr)->deleteObject(false);

    return surface ? Cairo::RefPtr<Cairo::Surface>(new Cairo::Surface(surface, true))
                   : Cairo::RefPtr<Cairo::Surface>();
}

/*
 * Return empty doc to render symbols in.
 * Symbols are by default not rendered so a <use> element is
 * provided.
 */
std::unique_ptr<SPDocument> SymbolsDialog::symbolsPreviewDoc()
{
    // BUG: <symbol> must be inside <defs>
    constexpr auto buffer = R"A(
<svg xmlns="http://www.w3.org/2000/svg"
     xmlns:sodipodi="http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd"
     xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape"
     xmlns:xlink="http://www.w3.org/1999/xlink">
  <use id="the_use" xlink:href="#the_symbol"/>
</svg>
)A"sv;
    return SPDocument::createNewDocFromMem(buffer, false);
}

Cairo::RefPtr<Cairo::Surface> SymbolsDialog::render_icon(SPDocument* document, const std::string& symbol_id, Geom::Point icon_size, int device_scale) {
    // render
    if (!document) document = getDocument();
    SPSymbol* symbol = document ? cast<SPSymbol>(document->getObjectById(symbol_id)) : nullptr;
    auto surface = draw_symbol(symbol);
    if (!surface) {
        surface = g_dummy;
    }
    return surface;
}

Glib::RefPtr<Gdk::Texture> SymbolsDialog::get_image(const std::string& key, SPDocument* document, const std::string& id) {
    if (auto image = _image_cache.get(key)) {
        // cache hit
        return *image;
    }
    else {
        // render
        auto psize = SYMBOL_ICON_SIZES[pack_size];
        auto icon_size = Geom::Point(psize, psize);
        auto surface = render_icon(document, id, icon_size, get_scale_factor());
        auto tex = to_texture(surface);
        _image_cache.insert(key, tex);
        return tex;
    }
}

} // namespace Inkscape::UI::Dialog

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-basic-offset:2
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=2:tabstop=8:softtabstop=2:fileencoding=utf-8:textwidth=99 :
