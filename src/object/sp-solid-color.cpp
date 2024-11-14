// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @solid color class.
 */
/* Authors:
 *   Tavmjong Bah <tavjong@free.fr>
 *
 * Copyright (C) 2014 Tavmjong Bah
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#include <cairo.h>

#include "sp-solid-color.h"

#include "attributes.h"
#include "style.h"
#include "display/drawing-paintserver.h"

/*
 * Solid Color
 */
SPSolidColor::SPSolidColor() : SPPaintServer() {
}

SPSolidColor::~SPSolidColor() = default;

void SPSolidColor::build(SPDocument* doc, Inkscape::XML::Node* repr) {
    SPPaintServer::build(doc, repr);

    this->readAttr(SPAttr::STYLE);
    this->readAttr(SPAttr::SOLID_COLOR);
    this->readAttr(SPAttr::SOLID_OPACITY);
}

/**
 * Virtual build: set solidcolor attributes from its associated XML node.
 */

void SPSolidColor::set(SPAttr key, const gchar* value) {

    if (SP_ATTRIBUTE_IS_CSS(key)) {
        style->clear(key);
        this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG);
    } else {
        SPPaintServer::set(key, value);
    }
}

/**
 * Virtual set: set attribute to value.
 */

Inkscape::XML::Node* SPSolidColor::write(Inkscape::XML::Document* xml_doc, Inkscape::XML::Node* repr, guint flags) {
    if ((flags & SP_OBJECT_WRITE_BUILD) && !repr) {
        repr = xml_doc->createElement("svg:solidColor");
    }

    SPObject::write(xml_doc, repr, flags);

    return repr;
}

std::unique_ptr<Inkscape::DrawingPaintServer> SPSolidColor::create_drawing_paintserver()
{
    auto color = style->solid_color.getColor();
    color.addOpacity(style->solid_opacity);
    return std::make_unique<Inkscape::DrawingSolidColor>(color);
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
