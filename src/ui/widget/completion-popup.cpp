// SPDX-License-Identifier: GPL-2.0-or-later

#include <cassert>
#include <gtkmm/entrycompletion.h>
#include <gtkmm/eventcontrollerkey.h>
#include <gtkmm/eventcontrollerfocus.h>
#include <gtkmm/menubutton.h>
#include <gtkmm/entry.h>
#include <gtkmm/liststore.h>

#include "completion-popup.h"
#include "ui/builder-utils.h"
#include <locale>
#include <codecvt>

namespace Inkscape::UI::Widget {

enum Columns {
    ColID = 0,
    ColName,
    ColIcon,
    ColSearch
};

CompletionPopup::CompletionPopup() :
    _builder(create_builder("completion-box.glade")),
    _search(get_widget<Gtk::Entry>(_builder, "search")),
    _button(get_widget<Gtk::MenuButton>(_builder, "menu-btn")),
    _popover_menu{Gtk::PositionType::BOTTOM},
    _completion(get_object<Gtk::EntryCompletion>(_builder, "completion"))
{
    auto const key = Gtk::EventControllerKey::create();
    key->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    key->signal_key_pressed().connect(sigc::mem_fun(*this, &CompletionPopup::onPopoverKeyPressed), true);
    _popover_menu.add_controller(key);
    _button.set_popover(_popover_menu);

    _list = std::dynamic_pointer_cast<Gtk::ListStore>(_builder->get_object("list"));
    assert(_list);

    append(get_widget<Gtk::Box>(_builder, "main-box"));

    _completion->set_match_func([=](const Glib::ustring& text, const Gtk::TreeModel::const_iterator& it){
        Glib::ustring str;
        it->get_value(ColSearch, str);
        if (str.empty()) {
            return false;
        }
        return str.normalize().lowercase().find(text.normalize().lowercase()) != Glib::ustring::npos;
    });

    _completion->signal_match_selected().connect([this](const Gtk::TreeModel::iterator& it){
        int id;
        it->get_value(ColID, id);
        _match_selected.emit(id);
        clear();
        return true;
    }, false);

    auto focus = Gtk::EventControllerFocus::create();
    focus->property_contains_focus().signal_changed().connect([this, &focus = *focus] {
        if (focus.contains_focus()) {
            _on_focus.emit();
        }
    });
    _search.add_controller(focus);

    _button.property_active().signal_changed().connect([this] {
        if (!_button.get_active()) {
            return;
        }
        _button_press.emit();
        clear();
        _menu_search.clear();
        _popover_menu.activate({});
    });
}

CompletionPopup::~CompletionPopup() = default;

bool CompletionPopup::onPopoverKeyPressed(unsigned keyval, unsigned /*keycode*/, Gdk::ModifierType /*state*/) {
    if (!_button.get_active()) {
        return false;
    }
    switch (keyval) {
        case GDK_KEY_Left:
        case GDK_KEY_KP_Left:
        case GDK_KEY_Up:
        case GDK_KEY_KP_Up:
        case GDK_KEY_Right:
        case GDK_KEY_KP_Right:
        case GDK_KEY_Down:
        case GDK_KEY_KP_Down:
            _menu_search.clear();
            _popover_menu.activate({});
            return false;
            break;
        case GDK_KEY_BackSpace:
            if (int len = _menu_search.size()) {
                _popover_menu.unset_items_focus_hover(nullptr);
                _menu_search = _menu_search.erase(len - 1);
                _popover_menu.activate(_menu_search);
                return true;
            }
            break;
        default:
            break;
    }
    int const ucode = gdk_keyval_to_unicode(gdk_keyval_to_lower(keyval));
    if (!std::isalpha(ucode) && keyval != GDK_KEY_minus) {
        return false;
    }
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv1;
    std::string charutf = conv1.to_bytes(ucode);
    _menu_search += charutf;
    _popover_menu.activate(_menu_search);
    return true;
}

void CompletionPopup::clear_completion_list() {
    _list->clear();
}

void CompletionPopup::add_to_completion_list(int id, Glib::ustring name, Glib::ustring icon_name, Glib::ustring search_text) {
    auto row = *_list->append();
    row.set_value(ColID, id);
    row.set_value(ColName, name);
    row.set_value(ColIcon, icon_name);
    row.set_value(ColSearch, search_text.empty() ? name : search_text);
}

PopoverMenu& CompletionPopup::get_menu() {
    return _popover_menu;
}

Gtk::Entry& CompletionPopup::get_entry() {
    return _search;
}

sigc::signal<void (int)>& CompletionPopup::on_match_selected() {
    return _match_selected;
}

sigc::signal<void ()>& CompletionPopup::on_button_press() {
    return _button_press;
}

sigc::signal<bool ()>& CompletionPopup::on_focus() {
    return _on_focus;
}

/// Clear search box without triggering completion popup menu
void CompletionPopup::clear() {
    _search.set_text({});
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
// vim:filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99:
