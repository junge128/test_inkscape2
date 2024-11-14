// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * The data describing a single loaded font.
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2022 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef LIBNRTYPE_FONT_INSTANCE_H
#define LIBNRTYPE_FONT_INSTANCE_H

#include <map>
#include <vector>
#include <optional>
#include <unordered_map>

#include <2geom/pathvector.h>
#include <pango/pango-types.h>
#include <pango/pango-font.h>

#include "font-glyph.h"
#include "OpenTypeUtil.h"
#include "style-enums.h"

namespace Inkscape {
class Pixbuf;
} // namespace Inkscape

/**
 * FontInstance provides metrics, OpenType data, and glyph curves/pixbufs for a font.
 *
 * Most data is loaded upon construction. Some rarely-used OpenType tables are lazy-loaded,
 * as are the curves/pixbufs for each glyph.
 *
 * Although FontInstance can be used on its own, in practice it is always obtained through
 * a FontFactory.
 *
 * Note: The font size is a scale factor in the transform matrix of the style.
 */
class FontInstance
{
public:
    /// Constructor; takes ownership of both arguments, which must be non-null. Throws CtorException on failure.
    FontInstance(PangoFont *p_font, PangoFontDescription *descr);

    /// Exception thrown if construction fails.
    struct CtorException : std::runtime_error
    {
        template <typename... Args>
        CtorException(Args&&... args) : std::runtime_error(std::forward<Args>(args)...) {}
    };

    ~FontInstance();

    int MapUnicodeChar(gunichar c) const; // calls the relevant unicode->glyph index function

    // Loads the given glyph's info. Glyphs are lazy-loaded, but never unloaded or modified
    // as long as the FontInstance still exists. Pointers to FontGlyphs also remain valid.
    FontGlyph const *LoadGlyph(int glyph_id);

    // nota: all coordinates returned by these functions are on a [0..1] scale; you need to multiply
    // by the fontsize to get the real sizes

    // Return 2geom pathvector for glyph. Deallocated when font instance dies.
    Geom::PathVector const *PathVector(int glyph_id);

    // Return font has SVG OpenType enties.
    bool                  FontHasSVG() const { return data->openTypeSVGGlyphs.size() > 0; };
    auto const &get_opentype_varaxes() const { return data->openTypeVarAxes; }

    // Return the font's OpenType tables, possibly loading them on-demand.
    std::map<Glib::ustring, OTSubstitution> const &get_opentype_tables();

    // Return pixbuf of SVG glyph or nullptr if no SVG glyph exists. As with glyphs, pixbufs
    // are lazy-loaded but immutable once loaded. They are guaranteed to be in Cairo pixel format.
    Inkscape::Pixbuf const *PixBuf(int glyph_id);

    // Horizontal advance if 'vertical' is false, vertical advance if true.
    double Advance(int glyph_id, bool vertical);

    // Return a shared pointer that will keep alive the pathvector and pixbuf data, but nothing else.
    std::shared_ptr<void const> share_data() const { return data; }

    double        GetTypoAscent()  const { return _ascent; }
    double        GetTypoDescent() const { return _descent; }
    double        GetXHeight()     const { return _xheight; }
    double        GetMaxAscent()   const { return _ascent_max; }
    double        GetMaxDescent()  const { return _descent_max; }
    const double* GetBaselines()   const { return _baselines; }
    int           GetDesignUnits() const { return _design_units; }

    bool FontMetrics(double &ascent, double &descent, double &leading) const;

    bool FontDecoration(double &underline_position, double &underline_thickness,
                        double &linethrough_position, double &linethrough_thickness) const;

    bool FontSlope(double &run, double &rise) const; // for generating slanted cursors for oblique fonts

    Geom::OptRect BBox(int glyph_id);

    bool IsOutlineFont() const { return FT_IS_SCALABLE(face); }
    bool has_vertical() const { return FT_HAS_VERTICAL(face); }

    auto get_descr() const { return descr; }
    auto get_font() const { return p_font; }

    double get_caps_height() const { return _caps_height; }
    double get_E_height() const { return _E_height; }
    bool is_fixed_width() const { return _fixed_width; }
    bool is_oblique() const { return _oblique; }
    unsigned short family_class() const { return _family_class; }

private:
    void acquire(PangoFont *p_font, PangoFontDescription *descr);
    void release();
    void init_face();
    void find_font_metrics(); // Find ascent, descent, x-height, and baselines.

    /*
     * Resources
     */

    // The font's fingerprint; this particular PangoFontDescription gives the key at which this font instance
    // resides in the font cache. It may differ from the PangoFontDescription belonging to p_font.
    PangoFontDescription *descr;

    // The real source of the font
    PangoFont *p_font;

    // we need to keep around an rw copy of the (read-only) hb font to extract the freetype face
    hb_font_t *hb_font_copy;

    // it's a pointer in fact; no worries to ref/unref it, pango does its magic
    // as long as p_font is valid, face is too
    FT_Face face;

    /*
     * Metrics
     */

    // Font metrics in em-box units
    double  _ascent;       // Typographic ascent.
    double  _descent;      // Typographic descent.
    double  _xheight;      // x-height of font.
    double  _ascent_max;   // Maximum ascent of all glyphs in font.
    double  _descent_max;  // Maximum descent of all glyphs in font.
    int     _design_units; // Design units, (units per em, typically 1000 or 2048).
    double  _caps_height = 0.0; // Caps height, if found in OS/2
    double  _E_height = 0.0; // Capital E height, if E glyph present
    double  _italic_angle = 0.0; // angle for oblique fonts, if specified in a font
    bool _fixed_width = false; // monospaced font (if advertised as such)
    bool _oblique = false;  // oblique or italic font
    unsigned short _family_class = 0; // OS/2 sFamily class field

    // Baselines
    double _baselines[SP_CSS_BASELINE_SIZE];

    struct Data
    {
        /*
         * Tables
         */

        // Map of SVG in OpenType glyphs
        std::map<int, SVGTableEntry> openTypeSVGGlyphs;

        // Maps for font variations.
        std::map<Glib::ustring, OTVarAxis> openTypeVarAxes; // Axes with ranges

        // Map of OpenType tables found in font. Transparently lazy-loaded.
        std::optional<std::map<Glib::ustring, OTSubstitution>> openTypeTables;

        /*
         * Glyphs
         */

        // Lookup table mapping pango glyph ids to glyphs.
        std::unordered_map<int, std::unique_ptr<FontGlyph const>> glyphs;
    };

    std::shared_ptr<Data> data;
};

#endif // LIBNRTYPE_FONT_INSTANCE_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
