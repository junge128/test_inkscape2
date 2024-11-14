// SPDX-License-Identifier: GPL-2.0-or-later

#include "save-image.h"
#include <glib/gi18n.h>
#include <string>
#include "display/cairo-utils.h"
#include "helper/choose-file.h"
#include "object/sp-image.h"

namespace Inkscape {

bool save_image(const std::string& fname, const Inkscape::Pixbuf* pixbuf) {
    if (fname.empty() || !pixbuf) return false;

    Inkscape::Pixbuf image(*pixbuf);
    auto pix = image.getPixbufRaw(true);
    GError* error = nullptr;
    gdk_pixbuf_save(pix, fname.c_str(), "png", &error, nullptr);
    if (error) {
        g_warning("Image saving error: %s", error->message);
        g_error_free(error);
        return false;
    }
    else {
        return true;
    }
}

bool extract_image(Gtk::Window* parent, SPImage* image) {
    if (!image || !image->pixbuf || !parent) return false;

    std::string current_dir;
    auto fname = choose_file_save(_("Extract Image"), parent, "image/png", "image.png", current_dir);
    if (fname.empty()) return false;

    // save image
    return save_image(fname, image->pixbuf.get());
}

} // namespace Inkscape
