// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  This file came from libwpg as a source, their utility wpg2svg
 *  specifically.  It has been modified to work as an Inkscape extension.
 *  The Inkscape extension code is covered by this copyright, but the
 *  rest is covered by the one below.
 *
 * Authors:
 *   Ted Gould <ted@gould.cx>
 *   Abhishek Sharma
 *
 * Copyright (C) 2006 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 *
 */

/* libwpg
 * Copyright (C) 2006 Ariya Hidayat (ariya@kde.org)
 * Copyright (C) 2005 Fridrich Strba (fridrich.strba@bluewin.ch)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * For further information visit http://libwpg.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

#include "wpg-input.h"
#include "extension/system.h"
#include "extension/input.h"
#include "document.h"
#include "object/sp-root.h"
#include "util/units.h"

#include <libwpg/libwpg.h>
#include <librevenge-stream/librevenge-stream.h>

using librevenge::RVNGString;
using librevenge::RVNGFileStream;
using librevenge::RVNGInputStream;

using namespace libwpg;

namespace Inkscape::Extension::Internal {

static std::span<char const> as_span(RVNGString const &str)
{
    return {str.cstr(), str.size()};
}

std::unique_ptr<SPDocument> WpgInput::open(Inkscape::Extension::Input *, char const *uri, bool)
{
    std::unique_ptr<RVNGInputStream> input;

    #ifdef _WIN32
        // RVNGFileStream uses fopen() internally which unfortunately only uses ANSI encoding on Windows
        // therefore attempt to convert uri to the system codepage
        // even if this is not possible the alternate short (8.3) file name will be used if available
        auto converted_uri = g_win32_locale_filename_from_utf8(uri);
        input = std::make_unique<RVNGFileStream>(converted_uri);
        g_free(converted_uri);
    #else
        input = std::make_unique<RVNGFileStream>(uri);
    #endif

    if (input->isStructured()) {
        if (auto olestream = input->getSubStreamByName("PerfectOffice_MAIN")) {
            input.reset(olestream);
        }
    }

    if (!WPGraphics::isSupported(input.get())) {
        //! \todo Dialog here
        // fprintf(stderr, "ERROR: Unsupported file format (unsupported version) or file is encrypted!\n");
        // printf("I'm giving up not supported\n");
        return nullptr;
    }

    librevenge::RVNGStringVector vec;
    librevenge::RVNGSVGDrawingGenerator generator(vec, "");

    if (!libwpg::WPGraphics::parse(input.get(), &generator) || vec.empty() || vec[0].empty()) {
        return nullptr;
    }

    RVNGString output("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n");
    output.append(vec[0]);

    auto doc = SPDocument::createNewDocFromMem(as_span(output), true);
    
    // Set viewBox if it doesn't exist
    if (doc && !doc->getRoot()->viewBox_set) {
        // Scales the document to account for 72dpi scaling in librevenge(<=0.0.4)
        doc->setWidth(Inkscape::Util::Quantity(doc->getWidth().quantity, "pt"), false);
        doc->setHeight(Inkscape::Util::Quantity(doc->getHeight().quantity, "pt"), false);
        doc->setViewBox(Geom::Rect::from_xywh(0, 0, doc->getWidth().value("pt"), doc->getHeight().value("pt")));
    }
    
    return doc;
}

#include "clear-n_.h"

void WpgInput::init() {
    // clang-format off
    Inkscape::Extension::build_from_mem(
        "<inkscape-extension xmlns=\"" INKSCAPE_EXTENSION_URI "\">\n"
            "<name>" N_("WPG Input") "</name>\n"
            "<id>org.inkscape.input.wpg</id>\n"
            "<input>\n"
                "<extension>.wpg</extension>\n"
                "<mimetype>image/x-wpg</mimetype>\n"
                "<filetypename>" N_("WordPerfect Graphics (*.wpg)") "</filetypename>\n"
                "<filetypetooltip>" N_("Vector graphics format used by Corel WordPerfect") "</filetypetooltip>\n"
            "</input>\n"
        "</inkscape-extension>", std::make_unique<WpgInput>());
    // clang-format on
}

} // namespace Inkscape::Extension::Internal

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
