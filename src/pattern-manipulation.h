// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef INKSCAPE_PATTERN_MANIPULATION_H
#define INKSCAPE_PATTERN_MANIPULATION_H

#include <vector>
#include <2geom/transforms.h>
#include <2geom/2geom.h>

class SPDocument;
class SPPattern;

namespace Inkscape::Colors {
class Color;
}

// Find and load stock patterns if not yet loaded and return them.
// Their lifetime is bound to StockPaintDocuments.
std::vector<SPDocument *> sp_get_stock_patterns();

// Returns a list of "root" patterns in the defs of the given source document
// Note: root pattern is the one with elements that are rendered; other patterns
// may refer to it (through href) and have their own transformation; those are skipped
std::vector<SPPattern*> sp_get_pattern_list(SPDocument* source);

// Set fill color for a pattern.
// If elements comprising pattern have no fill, they will inherit it.
// Some patterns may not be affected at all if not designed to support color change.
void sp_pattern_set_color(SPPattern* pattern, Inkscape::Colors::Color const &color);

// Set 'patternTransform' attribute
void sp_pattern_set_transform(SPPattern* pattern, const Geom::Affine& transform);

// set pattern 'x' & 'y' attributes; TODO: handle units, as necessary
void sp_pattern_set_offset(SPPattern* pattern, const Geom::Point& offset);

// simplified "preservedAspectRatio" for patterns; only yes/no ('xMidYMid' / 'none')
void sp_pattern_set_uniform_scale(SPPattern* pattern, bool uniform);

// Add a "gap" to pattern tile by enlarging its apparent size or overlap by shrinking;
// gap percent values:
// o 0% - no gap, seamless pattern
// o >0% - positive gap; 100% is the gap the size of pattern itself
// o <0% & >-100% - negative gap / overlap
void sp_pattern_set_gap(SPPattern* link_pattern, Geom::Scale gap_percent);
// Get pattern gap size as a percentage
Geom::Scale sp_pattern_get_gap(SPPattern* link_pattern);

// get pattern display name
std::string sp_get_pattern_label(SPPattern* pattern);

#endif
