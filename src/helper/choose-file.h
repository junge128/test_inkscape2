// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SEEN_CHOOSE_FILE_H
#define SEEN_CHOOSE_FILE_H

#include <string>
#include <utility>
#include <vector>
#include <glibmm/refptr.h>

namespace Glib {
class ustring;
} // namespace Glib

namespace Gio {
template <typename> class ListStore;
} // namespace Gio

namespace Gtk {
class FileDialog;
class FileFilter;
class Window;
} // namespace Gtk

namespace Inkscape {

/// Create a Gtk::FileDialog with the given title and label for its default/accept button.
[[nodiscard]] Glib::RefPtr<Gtk::FileDialog> create_file_dialog(Glib::ustring const &title,
                                                               Glib::ustring const &accept_label);
/// Set available filters to a given list, & default to its 1st filter (if any).
void set_filters(Gtk::FileDialog &file_dialog,
                 Glib::RefPtr<Gio::ListStore<Gtk::FileFilter>> const &filters);
/// Set the available filters & the default filter, to the single filter passed.
void set_filter(Gtk::FileDialog &file_dialog, Glib::RefPtr<Gtk::FileFilter> const &filter);

/// Synchronously run a Gtk::FileDialog to select a file for saving data.
/// @param mime_type MIME type to use as a file filter in the dialog.
/// @param file_name Name of the initial file to show in the dialogʼs text entry.
/// @param current_folder Path of initial folder to show, updated to parent of selected file if any
/// @returns name of selected file as local path, or empty if no file selected or not representable
[[nodiscard]]
std::string choose_file_save(Glib::ustring const &title, Gtk::Window *parent,
                             Glib::ustring const &mime_type, Glib::ustring const &file_name,
                             std::string& current_folder);

/// Synchronously run a Gtk::FileDialog to open a single file for reading data.
/// @param mime_type MIME types to offer as file filters in the dialog.
/// @param current_folder Path of initial folder to show, updated to parent of selected file if any
/// @returns name of selected file as local path, or empty if no file selected or not representable
[[nodiscard]]
std::string choose_file_open(Glib::ustring const &title, Gtk::Window *parent,
                             std::vector<Glib::ustring> const &mime_types,
                             std::string& current_folder);
/// Synchronously run a Gtk::FileDialog to open a single file for reading data.
/// @param filters Vector of pairs of <name, pattern> to create file filters to offer in the dialog
/// @param current_folder Path of initial folder to show, updated to parent of selected file if any
/// @returns name of selected file as local path, or empty if no file selected or not representable
[[nodiscard]]
std::string choose_file_open(Glib::ustring const &title, Gtk::Window *parent,
                             std::vector<std::pair<Glib::ustring, Glib::ustring>> const &filters,
                             std::string& current_folder);

} // namespace Inkscape

#endif // SEEN_CHOOSE_FILE_H

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
