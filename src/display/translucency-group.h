// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Render some items as translucent in a document rendering stack.
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2021-2024 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_DISPLAY_TRANSLUCENCY
#define SEEN_DISPLAY_TRANSLUCENCY

#include <vector>

class SPItem;

namespace Inkscape::Display {

class TranslucencyGroup {
public:
    TranslucencyGroup(unsigned int dkey);

    SPItem *getSolidItem() const { return _solid_item; }
    void setSolidItem(SPItem *item);
private:
    unsigned int _dkey;
    SPItem *_solid_item = nullptr;
    std::vector<SPItem *> _translucent_items;

    void _generateTranslucentItems(SPItem *parent);
};

} /* namespace Inkscape::Display */

#endif // SEEN_INKSCAPE_NR_LIGHT_H
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
