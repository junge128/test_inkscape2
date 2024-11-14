// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Global color palette information.
 */
/* Authors: PBS <pbs3141@gmail.com>
 * Copyright (C) 2022 PBS
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "global-palettes.h"

#include <iomanip>
#include <giomm/file.h>
#include <giomm/fileinputstream.h>
#include <giomm/inputstream.h>
#include <glib/gi18n.h>
#include <glibmm/convert.h>
#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <gtkmm/window.h>

// Using Glib::regex because
//  - std::regex is too slow in debug mode.
//  - boost::regex requires a library not present in the CI image.
#include <glibmm/i18n.h>
#include <glibmm/miscutils.h>
#include <glibmm/regex.h>
#include <iostream>
#include <lcms2.h>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "colors/manager.h"
#include "colors/spaces/enum.h"

#include "helper/choose-file.h"
#include "colors/spaces/lab.h"
#include "io/resource.h"
#include "io/sys.h"
#include "util/delete-with.h"
#include "util-string/ustring-format.h"
using Inkscape::Util::delete_with;
using namespace Inkscape::Colors;

namespace {

Glib::ustring get_extension(Glib::ustring const &name) {
    auto extpos = name.rfind('.');
    if (extpos != Glib::ustring::npos) {
        auto ext = name.substr(extpos).casefold();
        return ext;
    }
    return {};
}

std::vector<uint8_t> read_data(const Glib::RefPtr<Gio::InputStream>& s, size_t len) {
    std::vector<uint8_t> buf(len, 0);
    s->read(buf.data(), len);
    return buf;
}

std::string read_string(const Glib::RefPtr<Gio::InputStream>& s, size_t len) {
    std::vector<char> buf(len, 0);
    s->read(buf.data(), len);
    return std::string(buf.data(), len);
}

template<typename T>
T read_value(const Glib::RefPtr<Gio::InputStream>& s) {
    uint8_t buf[sizeof(T)];
    s->read(buf, sizeof(T));
    T val = 0;
    for (int i = 0; i < sizeof(T); ++i) {
        val <<= 8;
        val |= buf[i];
    }
    return val;
}

float read_float(const Glib::RefPtr<Gio::InputStream>& s) {
    auto val = read_value<uint32_t>(s);
    return *reinterpret_cast<float*>(&val);
}

Glib::ustring read_pstring(const Glib::RefPtr<Gio::InputStream>& s, bool short_string = false) {
    size_t len = short_string ? read_value<uint16_t>(s) : read_value<uint32_t>(s);
    if (!len) return {};

    std::vector<uint16_t> buf(len, 0);
    s->read(buf.data(), 2 * len);
    for (int i = 0; i < len; ++i) {
        auto c = buf[i];
        c = (c & 0xff) << 8 | c >> 8; // std::byteswap()
        buf[i] = c;
    }
    // null terminated string?
    if (buf[len - 1] == 0) --len;

    auto string = g_utf16_to_utf8(buf.data(), len, nullptr, nullptr, nullptr);
    if (!string) return {};

    Glib::ustring ret(string);
    g_free(string);
    return ret;
}

void skip(const Glib::RefPtr<Gio::InputStream>& s, size_t bytes) {
    s->skip(bytes);
}

using namespace Inkscape::UI::Dialog;

namespace ColorBook {

// Color space codes in ACB color book palettes
enum Colorspace : uint16_t
{
    RgbColorspace = 0,
    CmykColorspace = 2,
    LabColorspace = 7,
    GrayscaleColorspace = 8
};

} // namespace ColorBook

// Load Adobe ACB color book
void load_acb_palette(PaletteFileData& palette, std::string const &fname) {
    auto file = Gio::File::create_for_path(fname);
    auto stream = file->read();
    auto magic = read_string(stream, 4);
    if (magic != "8BCB") throw std::runtime_error(_("ACB file header not recognized."));

    auto version = read_value<uint16_t>(stream);
    if (version != 1) {
        g_warning("Unknown ACB palette version in %s", fname.c_str());
    }

    /* id */ read_value<uint16_t>(stream);

    auto ttl = read_pstring(stream);
    auto prefix = read_pstring(stream);
    auto suffix = read_pstring(stream);
    auto desc = read_pstring(stream);
    auto extract = [](const Glib::ustring& str) {
        auto pos = str.find('=');
        if (pos != Glib::ustring::npos) {
            return str.substr(pos + 1);
        }
        return Glib::ustring();
    };
    prefix = extract(prefix);
    suffix = extract(suffix);
    ttl = extract(ttl);

    auto color_count = read_value<uint16_t>(stream);
    palette.columns = read_value<uint16_t>(stream);
    palette.page_offset = read_value<uint16_t>(stream);
    auto cs = read_value<uint16_t>(stream);

    auto ext = get_extension(ttl);
    if (ext == ".acb") {
        // extension in palette title -> junk name; use file name instead
        palette.name = Glib::path_get_basename(fname);
        ext = get_extension(palette.name);
        if (ext == ".acb") {
            palette.name = palette.name.substr(0, palette.name.size() - ext.size());
        }
    }
    else {
        auto r = ttl.find("^R");
        if (r != Glib::ustring::npos) ttl.replace(r, 2, "®");
        palette.name = ttl;
    }

    palette.colors.reserve(color_count);

    auto &cm = Manager::get();
    std::shared_ptr<Space::AnySpace> space;

    switch (cs) {
        case ColorBook::RgbColorspace:
            space = cm.find(Space::Type::RGB);
            break;
        case ColorBook::CmykColorspace:
            space = cm.find(Space::Type::CMYK);
            break;
        case ColorBook::LabColorspace:
            space = cm.find(Space::Type::LAB);
            break;
        case ColorBook::GrayscaleColorspace:
            space = cm.find(Space::Type::Gray);
            break;
        default:
            throw std::runtime_error(_("ACB file color space not supported."));
    }

    for (int index = 0; index < color_count; ++index) {

        auto name = read_pstring(stream);
        if (name.substr(0, 3) == "$$$") name = extract(name);
        auto code = read_string(stream, 6);
        std::ostringstream ost;
        ost.precision(3);

        std::vector<double> data;
        for (auto channel : read_data(stream, space->getComponentCount())) {
            data.emplace_back(channel / 255.0);
        }
        auto color = Color(space, data);

        if (name.empty()) {
            palette.colors.emplace_back(PaletteFileData::SpacerItem());
        }
        else {
            color.setName(prefix + name + suffix);
            palette.colors.emplace_back(std::move(color));
        }
    }
}

void load_ase_swatches(PaletteFileData& palette, std::string const &fname) {
    auto file = Gio::File::create_for_path(fname);
    auto stream = file->read();
    auto magic = read_string(stream, 4);
    if (magic != "ASEF") throw std::runtime_error(_("ASE file header not recognized."));

    auto version_major = read_value<uint16_t>(stream);
    auto version_minor = read_value<uint16_t>(stream);

    if (version_major > 1) {
        g_warning("Unknown swatches version %d.%d in %s", (int)version_major, (int)version_minor, fname.c_str());
    }

    auto block_count = read_value<uint32_t>(stream);
    auto &cm = Manager::get();

    static std::map<std::string, Space::Type> name_map = {
        {"RGB ", Space::Type::RGB},
        {"LAB ", Space::Type::LAB},
        {"CMYK", Space::Type::CMYK},
        {"GRAY", Space::Type::Gray}
    };

    for (uint32_t block = 0; block < block_count; ++block) {
        auto block_type = read_value<uint16_t>(stream);
        auto block_length = read_value<uint32_t>(stream);
        std::ostringstream ost;

        if (block_type == 0xc001) { // group start
            auto name = read_pstring(stream, true);
            palette.colors.emplace_back(PaletteFileData::GroupStart{.name = name});
        }
        else if (block_type == 0x0001) { // color entry
            auto color_name = read_pstring(stream, true);
            auto space_name = read_string(stream, 4);
            
            auto it = name_map.find(space_name);
            if (it == name_map.end()) {
                std::ostringstream ost;
                ost << _("ASE color mode not recognized:") << " '" << space_name << "'.";
                throw std::runtime_error(ost.str());
            }
            auto space = cm.find(it->second);
            std::vector<double> data;
            for (unsigned i = 0; i < space->getComponentCount(); i++) {
                data.emplace_back(read_float(stream));
            }
            auto color = Color(space, data);

            read_value<uint16_t>(stream); // type uint16, ignored for now
            // auto mode = to_mode(type); Is this used? 0, Global, 1, Spot, else Normal

            color.setName(color_name);
            palette.colors.emplace_back(color);
        }
        else if (block_type == 0xc002) { // group end
        }
        else {
            skip(stream, block_length);
        }
    }

    // palette name - file name without extension
    palette.name = Glib::path_get_basename(fname);
    auto ext = get_extension(palette.name);
    if (ext == ".ase") palette.name = palette.name.substr(0, palette.name.size() - ext.size());
}

// Load GIMP color palette
void load_gimp_palette(PaletteFileData& palette, std::string const &path)
{
    palette.name = Glib::path_get_basename(path);
    palette.columns = 1;

    auto f = delete_with<std::fclose>(Inkscape::IO::fopen_utf8name(path.c_str(), "r"));
    if (!f) throw std::runtime_error(_("Failed to open file"));

    char buf[1024];
    if (!std::fgets(buf, sizeof(buf), f.get())) throw std::runtime_error(_("File is empty"));
    if (std::strncmp("GIMP Palette", buf, 12) != 0) throw std::runtime_error(_("First line is wrong"));

    static auto const regex_rgb   = Glib::Regex::create("\\s*(\\d+)\\s+(\\d+)\\s+(\\d+)\\s*(?:\\s(.*\\S)\\s*)?$", Glib::Regex::CompileFlags::OPTIMIZE | Glib::Regex::CompileFlags::ANCHORED);
    static auto const regex_name  = Glib::Regex::create("\\s*Name:\\s*(.*\\S)", Glib::Regex::CompileFlags::OPTIMIZE | Glib::Regex::CompileFlags::ANCHORED);
    static auto const regex_cols  = Glib::Regex::create("\\s*Columns:\\s*(.*\\S)", Glib::Regex::CompileFlags::OPTIMIZE | Glib::Regex::CompileFlags::ANCHORED);
    static auto const regex_blank = Glib::Regex::create("\\s*(?:$|#)", Glib::Regex::CompileFlags::OPTIMIZE | Glib::Regex::CompileFlags::ANCHORED);

    auto &cm = Manager::get();
    auto space = cm.find(Space::Type::RGB);

    while (std::fgets(buf, sizeof(buf), f.get())) {
        Glib::MatchInfo match;
        if (regex_rgb->match(buf, match)) {
            // 8-bit RGB color, followed by an optional name.

            std::vector<double> data;
            for (unsigned i = 0; i < space->getComponentCount(); i++) {
                data.emplace_back(std::stoi(match.fetch(i+1)) / 255.0);
            }
            auto color = Color(space, data);
            color.setName(match.fetch(4));

            if (!color.getName().empty()) {
                // Translate the name if present.
                color.setName(g_dpgettext2(nullptr, "Palette", color.getName().c_str()));
            }

            palette.colors.emplace_back(std::move(color));
        } else if (regex_name->match(buf, match)) {
            // Header entry for name.
            palette.name = match.fetch(1);
        } else if (regex_cols->match(buf, match)) {
            // Header entry for columns.
            palette.columns = std::clamp(std::stoi(match.fetch(1)), 1, 1000);
        } else if (regex_blank->match(buf, match)) {
            // Comment or blank line.
        } else {
            // Unrecognised.
            throw std::runtime_error(C_("Palette", "Invalid line ") + std::string(buf));
        }
    }
}

} // namespace

namespace Inkscape::UI::Dialog {

PaletteResult load_palette(std::string const &path)
{
    auto const utf8path = Glib::filename_to_utf8(path);

    auto compose_error = [&] (char const *what) {
        return Glib::ustring::compose(_("Error loading palette %1: %2"), utf8path, what);
    };

    try {
        PaletteFileData p;
        p.id = utf8path;

        auto const ext = get_extension(utf8path);
        if (ext == ".acb") {
            load_acb_palette(p, path);
        } else if (ext == ".ase") {
            load_ase_swatches(p, path);
        } else {
            load_gimp_palette(p, path);
        }

        return {std::move(p), {}};

    } catch (std::runtime_error const &e) {
        return {{}, compose_error(e.what())};
    } catch (std::logic_error const &e) {
        return {{}, compose_error(e.what())};
    } catch (Glib::Error const &e) {
        return {{}, compose_error(e.what())};
    } catch (...) {
        return {{}, Glib::ustring::compose(_("Unknown error loading palette %1"), utf8path)};
    }
}

GlobalPalettes::GlobalPalettes()
{
    // Load the palettes.
    for (auto const &path : Inkscape::IO::Resource::get_filenames(Inkscape::IO::Resource::PALETTES, {".gpl", ".acb", ".ase"})) {
        auto res = load_palette(path);
        if (res.palette) {
            _palettes.emplace_back(std::move(*res.palette));
        } else {
            g_warning("%s", res.error_message.c_str());
        }
    }

    // Sort by name.
    std::sort(_palettes.begin(), _palettes.end(), [] (auto& a, auto& b) {
        return a.name.compare(b.name) < 0;
    });

    // First priority for lookup: by id.
    for (auto& pal : _palettes) {
        _access.emplace(pal.id.raw(), &pal);
    }

    // Second priority for lookup: by name.
    for (auto& pal : _palettes) {
        if (!pal.name.empty()) {
            _access.emplace(pal.name.raw(), &pal);
        }
    }
}

const PaletteFileData* GlobalPalettes::find_palette(const Glib::ustring& id) const {
    auto p = _access.find(id.raw());
    return p != _access.end() ? p->second : nullptr;
}

std::string choose_palette_file(Gtk::Window* window) {
    static std::string current_folder;
    static std::vector<std::pair<Glib::ustring, Glib::ustring>> const filters{
        {_("Gimp Color Palette"), "*.gpl"},
        {_("Adobe Color Book"), "*.acb"},
        {_("Adobe Swatch Exchange"), "*.ase"}
    };
    return choose_file_open(_("Load color palette"), window, filters, current_folder);
}

GlobalPalettes const &GlobalPalettes::get()
{
    static GlobalPalettes instance;
    return instance;
}

} // namespace Inkscape::UI::Dialog

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
