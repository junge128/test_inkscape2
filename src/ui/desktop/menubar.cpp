// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Desktop main menu bar code.
 */
/*
 * Authors:
 *   Tavmjong Bah       <tavmjong@free.fr>
 *   Alex Valavanis     <valavanisalex@gmail.com>
 *   Patrick Storz      <eduard.braun2@gmx.de>
 *   Krzysztof Kosiński <tweenk.pl@gmail.com>
 *   Kris De Gussem     <Kris.DeGussem@gmail.com>
 *   Sushant A.A.       <sushant.co19@gmail.com>
 *
 * Copyright (C) 2018 Authors
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 * Read the file 'COPYING' for more information.
 */

#include "menubar.h"

#include <algorithm>
#include <iostream>
#include <regex>
#include <string>
#include <unordered_map>
#include <glibmm/i18n.h>
#include <glibmm/quark.h>
#include <glibmm/regex.h>
#include <glibmm/ustring.h>
#include <giomm/menu.h>
#include <giomm/menuattributeiter.h>
#include <giomm/menuitem.h>
#include <giomm/menulinkiter.h>
#include <giomm/menumodel.h>
#include <gtkmm/builder.h>
#include <gtkmm/recentmanager.h>

#include "actions/actions-effect-data.h"
#include "actions/actions-effect.h"
#include "inkscape-application.h" // Open recent
#include "preferences.h"          // Use icons or not
#include "io/fix-broken-links.h"
#include "io/resource.h"          // UI File location

// =================== Main Menu ================
void
build_menu()
{
    std::string filename = Inkscape::IO::Resource::get_filename(Inkscape::IO::Resource::UIS, "menus.ui");
    auto refBuilder = Gtk::Builder::create();

    try {
        refBuilder->add_from_file(filename);
    } catch (Glib::Error const &err) {
        std::cerr << "build_menu: failed to load Main menu from: "
                    << filename <<": "
                    << err.what() << std::endl;
        return;
    }

    const auto object = refBuilder->get_object("menus");
    const auto gmenu = std::dynamic_pointer_cast<Gio::Menu>(object);

    if (!gmenu) {
        std::cerr << "build_menu: failed to build Main menu!" << std::endl;
        return;
    }

    static auto app = InkscapeApplication::instance();
    enable_effect_actions(app, false);
    auto &label_to_tooltip_map = app->get_menu_label_to_tooltip_map();
    label_to_tooltip_map.clear();

    { // Filters and Extensions
        auto effects_object = refBuilder->get_object("effect-menu-effects");
        auto filters_object = refBuilder->get_object("filter-menu-filters");
        auto effects_menu = std::dynamic_pointer_cast<Gio::Menu>(effects_object);
        auto filters_menu = std::dynamic_pointer_cast<Gio::Menu>(filters_object);

        if (!filters_menu) {
            std::cerr << "build_menu(): Couldn't find Filters menu entry!" << std::endl;
        }
        if (!effects_menu) {
            std::cerr << "build_menu(): Couldn't find Extensions menu entry!" << std::endl;
        }

        std::map<std::string, Glib::RefPtr<Gio::Menu>> submenus;

        for (auto &&entry : app->get_action_effect_data().give_all_data()) {
            auto const &submenu_name_list = entry.submenu;

            // Effect data is used for both filters menu and extensions menu... we need to
            // add to correct menu.
            std::string path; // Only used as index to map of submenus.
            auto top_menu = filters_menu;
            if (!entry.is_filter) {
                top_menu = effects_menu;
                path = "Effects";
            } else {
                path = "Filters";
            }

            if (!top_menu) { // It's possible that the menu doesn't exist (Kid's Inkscape?)
                std::cerr << "build_menu(): menu doesn't exist!" << std::endl; // Warn for now.
                continue;
            }

            auto current_menu = top_menu;
            for (auto const &submenu_name : submenu_name_list) {
                path.reserve(path.size() + submenu_name.size() + 1);
                path.append(submenu_name).append(1, '-');

                auto it = submenus.lower_bound(path);

                if (it == submenus.end() || it->first != path) {
                    auto submenu = Gio::Menu::create();
                    current_menu->append_submenu(submenu_name, submenu);
                    it = submenus.emplace_hint(it, path, std::move(submenu));
                }

                current_menu = it->second;
            }
            current_menu->append(entry.effect_name, "app." + entry.effect_id);
        }
    }

    // Recent file
    auto recent_manager = Gtk::RecentManager::get_default();
    auto recent_object = refBuilder->get_object("recent-files");
    auto recent_gmenu = std::dynamic_pointer_cast<Gio::Menu>(recent_object);
    auto recent_menu_quark = Glib::Quark("recent-manager");
    if (recent_gmenu) {
        recent_gmenu->set_data(recent_menu_quark, recent_manager.get()); // mark submenu, so we can find it
    }

    auto rebuild_recent = [](Glib::RefPtr<Gio::Menu> const &submenu) {
        if (!submenu) {
            g_warning("No recent submenu in menus.ui found.");
            return;
        }

        submenu->remove_all();

        int max_files = Inkscape::Preferences::get()->getInt("/options/maxrecentdocuments/value");
        if (max_files <= 0) {
            return;
        }

        auto recent_manager = Gtk::RecentManager::get_default();
        auto recent_files = recent_manager->get_items(); // all recent files not necessarily inkscape only (std::vector)

        // Remove non-inkscape files.
        std::erase_if(recent_files, [](auto const &recent_file) -> bool {
            // Note: Do not check if the file exists, to avoid long delays. See https://gitlab.com/inkscape/inkscape/-/issues/2348.
            bool valid_file =
                recent_file->has_application(g_get_prgname())         ||
                recent_file->has_application("org.inkscape.Inkscape") ||
                recent_file->has_application("inkscape")
#ifdef _WIN32
             || recent_file->has_application("inkscape.exe")
#endif
            ;
            return !valid_file;
        });

        // Truncate to user specified max_files.
        if (recent_files.size() > max_files) {
            recent_files.resize(max_files);
        }

        // Create a map of path to shortened path, and prefill.
        std::map<Glib::ustring, Glib::ustring> shortened_path_map;
        for (auto recent_file : recent_files) {
            shortened_path_map[recent_file->get_uri_display()] = recent_file->get_display_name();
        }

        // Sort by display name (which includes date in file name for files saved during crash.
        auto sort_comparator = [](auto const a, auto const b) -> bool { return a->get_display_name() < b->get_display_name(); };
        std::sort (recent_files.begin(), recent_files.end(), sort_comparator);
        for (auto recent_file : recent_files) {
            shortened_path_map[recent_file->get_uri_display()] = recent_file->get_display_name();
        }

        // Look for duplicate short names. These are the only ones that matter here.
        auto equal_comparator = [](auto const a, auto const b) -> bool { return a->get_display_name() == b->get_display_name(); };
        auto it = recent_files.begin();
        while (it != (recent_files.end() - 1)) {
            it = std::adjacent_find(it, recent_files.end(), equal_comparator);
            if (it != recent_files.end()) {

                // Found duplicate display name!
                std::vector<Glib::ustring> display_uris;
                display_uris.emplace_back(( * it   )->get_uri_display());
                display_uris.emplace_back(( *(it+1))->get_uri_display());

                std::vector<std::vector<std::string>> path_parts;
                path_parts.emplace_back(Inkscape::splitPath((* it   )->get_uri_display()));
                path_parts.emplace_back(Inkscape::splitPath((*(it+1))->get_uri_display()));

                // Find first directory difference from root down.
                auto max_size = std::min(path_parts[0].size(), path_parts[1].size());
                int i = 0;
                for (; i < max_size; ++i) {
                    if (path_parts[0][i] != path_parts[1][i]) {
                        break;
                    }
                }

                // Override map of path to shortened path.
                for (int j = 0; j < 2; j++) {

                    auto display_uri = display_uris[j]; // We always use display_uri as map index.
                    // Size is always one first element such as '/' or 'C:\\' and the last element is the filename
                    auto size = path_parts[j].size();

                    if (size <= 3) {
                        // If file is in root directory or child of root directory, just use display uri.
                        shortened_path_map[display_uri] = display_uri;
                    } else if (i == size - 1) {
                        // If difference is at last path part (file name), use that.
                        shortened_path_map[display_uri] = path_parts[j].back();
                    } else if (i == size - 2) {

                        // If difference is last directory level (file name), use that + file name.
                        shortened_path_map[display_uri] =
                            Glib::ustring::compose ("..%1%2%3%4",
                                                    G_DIR_SEPARATOR_S,
                                                    path_parts[j][size-2],
                                                    G_DIR_SEPARATOR_S,
                                                    path_parts[j][size-1]);
                    } else if (i == 1) {
                        // parts[j][i] is actually a root folder
                        shortened_path_map[display_uri] =
                            Glib::ustring::compose ("%1%2%3..%4%5",
                                                    path_parts[j][0],
                                                    path_parts[j][i],
                                                    G_DIR_SEPARATOR_S,
                                                    G_DIR_SEPARATOR_S,
                                                    path_parts[j][size-1]);
                    } else {
                        shortened_path_map[display_uri] =
                            Glib::ustring::compose ("..%1%2%3..%4%5",
                                                    G_DIR_SEPARATOR_S,
                                                    path_parts[j][i],
                                                    G_DIR_SEPARATOR_S,
                                                    G_DIR_SEPARATOR_S,
                                                    path_parts[j][size-1]);
                    }
                }
            } else {
                // At end!
                break;
            }

            // Test next entry.
            ++it;
        }

        // Sort by "last modified" time, which puts the most recently opened files first.
        std::sort(std::begin(recent_files), std::end(recent_files), [](auto const &a, auto const &b) -> bool {
            // a should precede b if a->get_modified() is later than b->get_modified()
            return a->get_modified().compare(b->get_modified()) > 0;
        });

        unsigned inserted_entries = 0;
        for (auto const &recent_file : recent_files) {
            // check if given was generated by inkscape

            // Escape underscores to prevent them from being interpreted as accelerator mnemonics
            std::regex underscore{"_"};
            std::string const dunder{"__"};
            std::string raw_name = shortened_path_map[recent_file->get_uri_display()]; // recent_file->get_short_name();
            Glib::ustring escaped = std::regex_replace(raw_name, underscore, dunder);
            auto item { Gio::MenuItem::create(std::move(escaped), Glib::ustring()) };
            auto target { Glib::Variant<Glib::ustring>::create(recent_file->get_uri_display()) };
            // note: setting action and target separately rather than using convenience menu method append
            // since some filename characters can result in invalid "direct action" string
            item->set_action_and_target(Glib::ustring("app.file-open-window"), target);
            submenu->append_item(item);

            inserted_entries++;
        }

        if (!inserted_entries) { // Create a placeholder with a non-existent action
            auto nothing2c = Gio::MenuItem::create(_("No items found"), "app.nop");
            submenu->append_item(nothing2c);
        }
    };

    rebuild_recent(recent_gmenu);

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    auto useicons = static_cast<UseIcons>(prefs->getInt("/theme/menuIcons", 0));

    // Remove all or some icons. Also create label to tooltip map.
    auto gmenu_copy = Gio::Menu::create();
    // menu gets recreated; keep track of new recent items submenu
    rebuild_menu(gmenu, gmenu_copy, useicons, recent_menu_quark, recent_gmenu);
    app->gtk_app()->set_menubar(gmenu_copy);

    // rebuild recent items submenu when the list changes
    recent_manager->signal_changed().connect([=](){ rebuild_recent(recent_gmenu); });
}

/*
 * Disable all or some menu icons.
 *
 * This is quite nasty:
 *
 * We must disable icons in the Gio::Menu as there is no way to pass
 * the needed information to the children of Gtk::PopoverMenu and no
 * way to set visibility via CSS.
 *
 * MenuItems are immutable and not copyable so you have to recreate
 * the menu tree. The format for accessing MenuItem data is not the
 * same as what you need to create a new MenuItem.
 *
 * NOTE: Input is a Gio::MenuModel, Output is a Gio::Menu!!
 */
void rebuild_menu(Glib::RefPtr<Gio::MenuModel> const &menu, Glib::RefPtr<Gio::Menu> const &menu_copy,
                  UseIcons const useIcons, Glib::Quark const &quark, Glib::RefPtr<Gio::Menu>& recent_files)
{
    static auto app = InkscapeApplication::instance();
    auto& extra_data = app->get_action_extra_data();
    auto& label_to_tooltip_map = app->get_menu_label_to_tooltip_map();

    for (int i = 0; i < menu->get_n_items(); ++i) {
        Glib::ustring label;
        Glib::ustring action;
        Glib::ustring target;
        Glib::VariantBase icon;
        Glib::ustring use_icon;

        std::unordered_map<Glib::ustring, Glib::VariantBase> attributes;

        auto attribute_iter = menu->iterate_item_attributes(i);
        while (attribute_iter->next()) {
            // Attributes we need to create MenuItem or set icon.
            if          (attribute_iter->get_name() == "label") {
                // Convert label while preserving unicode translations
                label = Glib::VariantBase::cast_dynamic<Glib::Variant<std::string> >(attribute_iter->get_value()).get();
            } else if   (attribute_iter->get_name() == "action") {
                action  = attribute_iter->get_value().print();
                action.erase(0, 1);
                action.erase(action.size()-1, 1);
            } else if   (attribute_iter->get_name() == "target") {
                target  = attribute_iter->get_value().print();
            } else if   (attribute_iter->get_name() == "icon") {
                icon     = attribute_iter->get_value();
            } else if (attribute_iter->get_name() == "use-icon") {
                use_icon =  attribute_iter->get_value().print();
            } else {
                // All the remaining attributes.
                attributes[attribute_iter->get_name()] = attribute_iter->get_value();
            }
        }
        Glib::ustring detailed_action = action;
        if (target.size() > 0) {
            detailed_action += "(" + target + ")";
        }

        auto tooltip = extra_data.get_tooltip_for_action(detailed_action);
        label_to_tooltip_map[label] = std::move(tooltip);

        // std::cout << "  " << std::setw(30) << detailed_action
        //           << "  label: " << std::setw(30) << label.c_str()
        //           << "  use_icon (.ui): " << std::setw(6) << use_icon
        //           << "  icon: " << (icon ? "yes" : "no ")
        //           << "  useIcons: " << (int)useIcons
        //           << "  use_icon.size(): " << use_icon.size()
        //           << "  tooltip: " << tooltip.c_str()
        //           << std::endl;

#ifdef __APPLE__
        // Workaround for https://gitlab.gnome.org/GNOME/gtk/-/issues/5667
        // Convert document actions to window actions
        if (strncmp(detailed_action.c_str(), "doc.", 4) == 0) {
            detailed_action = "win." + detailed_action.raw().substr(4);
        }
#endif

        auto menu_item = Gio::MenuItem::create(label, detailed_action);
        if (icon &&
            (useIcons == UseIcons::always ||
             (useIcons == UseIcons::as_requested && use_icon.size() > 0))) {
            menu_item->set_attribute_value("icon", icon);
        }

        // Add remaining attributes
        for (auto const& [key, value] : attributes) {
            menu_item->set_attribute_value(key, value);
        }

        // Add submenus
        auto link_iter = menu->iterate_item_links(i);
        while (link_iter->next()) {
            auto submenu = Gio::Menu::create();
            if (link_iter->get_name() == "submenu") {
                menu_item->set_submenu(submenu);
                if (link_iter->get_value()->get_data(quark)) {
                    recent_files = submenu;
                }
            } else if (link_iter->get_name() == "section") {
                menu_item->set_section(submenu);
            } else {
                std::cerr << "rebuild_menu: Unknown link type: " << link_iter->get_name().raw() << std::endl;
            }
            rebuild_menu (link_iter->get_value(), submenu, useIcons, quark, recent_files);
        }

        menu_copy->append_item(menu_item);
    }
}

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
