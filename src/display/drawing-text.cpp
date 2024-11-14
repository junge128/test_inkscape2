// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Group belonging to an SVG drawing element.
 *//*
 * Authors:
 *   Krzysztof Kosiński <tweenk.pl@gmail.com>
 *
 * Copyright (C) 2011 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <2geom/pathvector.h>

#include "style.h"

#include "cairo-utils.h"
#include "drawing-context.h"
#include "drawing-surface.h"
#include "drawing-text.h"
#include "drawing.h"

#include "helper/geom.h"

#include "libnrtype/font-instance.h"

namespace Inkscape {


DrawingGlyphs::DrawingGlyphs(Drawing &drawing)
    : DrawingItem(drawing)
    , _glyph(0)
{
}

void DrawingGlyphs::setGlyph(std::shared_ptr<FontInstance> font, int glyph, Geom::Affine const &trans)
{
    defer([=, this, font = std::move(font)] {
        _markForRendering();

        assert(!_drawing.snapshotted());
        setTransform(trans);

        _font_data = font->share_data();
        _glyph = glyph;

        design_units = 1.0;
        pathvec = nullptr;
        pathvec_ref  = nullptr;
        pixbuf = nullptr;

        // Load pathvectors and pixbufs in advance, as must be done on main thread.
        if (font) {
            design_units = font->GetDesignUnits();
            pathvec      = font->PathVector(_glyph);
            pathvec_ref  = font->PathVector(42);

            if (font->FontHasSVG()) {
                pixbuf = font->PixBuf(_glyph);
            }
        }

        _markForUpdate(STATE_ALL, false);
    });
}

void DrawingGlyphs::setStyle(SPStyle const *, SPStyle const *)
{
    std::cerr << "DrawingGlyphs: Use parent style" << std::endl;
}

unsigned DrawingGlyphs::_updateItem(Geom::IntRect const &/*area*/, UpdateContext const &ctx, unsigned /*flags*/, unsigned /*reset*/)
{
    auto ggroup = cast<DrawingText>(&std::as_const(*_parent));
    if (!ggroup) {
        throw InvalidItemException();
    }

    if (!pathvec) {
        return STATE_ALL;
    }

    _pick_bbox = Geom::IntRect();
    _bbox = Geom::IntRect();

    /*
      Make a bounding box for drawing that is a little taller and lower (currently 10% extra) than
      the font's drawing box.  Extra space is to hold overline or underline, if present.  All
      characters in a font use the same ascent and descent, but different widths. This lets leading
      and trailing spaces have text decorations. If it is not done the bounding box is limited to
      the box surrounding the drawn parts of visible glyphs only, and draws outside are ignored.
      The box is also a hair wider than the text, since the glyphs do not always start or end at
      the left and right edges of the box defined in the font.
    */

    float scale_bigbox = 1.0;
    if (_transform) {
        scale_bigbox /= _transform->descrim();
    }

    /* Because there can be text decorations the bounding box must correspond in Y to a little above the glyph's ascend
    and a little below its descend.  This leaves room for overline and underline.  The left and right sides
    come from the glyph's bounding box.  Note that the initial direction of ascender is positive down in Y, and
    this flips after the transform is applied.  So change the sign on descender. 1.1 provides a little extra space
    above and below the max/min y positions of the letters to place the text decorations.*/

    Geom::Rect b;
    if (pathvec) {
        Geom::OptRect tiltb = Geom::bounds_exact(*pathvec);
        if (tiltb) {
            Geom::Rect bigbox(Geom::Point(tiltb->left(), -_dsc * scale_bigbox * 1.1), Geom::Point(tiltb->right(), _asc * scale_bigbox * 1.1));
            b = bigbox * ctx.ctm;
        }
    }
    if (b.hasZeroArea()) { // Fallback, spaces mostly
        Geom::Rect bigbox(Geom::Point(0.0, -_dsc * scale_bigbox * 1.1),  Geom::Point(_width * scale_bigbox, _asc * scale_bigbox * 1.1));
        b = bigbox * ctx.ctm;
    }

    /*
      The pick box matches the characters as best as it can, leaving no extra space above or below
      for decorations.  The pathvector may include spaces, and spaces have no drawable glyph.
      Catch those and do not pass them to bounds_exact_transformed(), which crashes Inkscape if it
      sees a nondrawable glyph. Instead mock up a pickbox for them using font characteristics.
      There may also be some other similar white space characters in some other unforeseen context
      which should be handled by this code as well..
    */

    Geom::OptRect pb;
    if (pathvec) {
        if (!pathvec->empty()) {
            pb = bounds_exact_transformed(*pathvec, ctx.ctm);
        }
        if (pathvec_ref && !pathvec_ref->empty()) {
            pb.unionWith(bounds_exact_transformed(*pathvec_ref, ctx.ctm));
            pb.expandTo(Geom::Point(pb->right() + (_width * ctx.ctm.descrim()), pb->bottom()));
        }
    }
    if (!pb) { // Fallback
        Geom::Rect pbigbox(Geom::Point(0.0, _asc * scale_bigbox * 0.66),Geom::Point(_width * scale_bigbox, 0.0));
        pb = pbigbox * ctx.ctm;
    }
 
#if 0
    /* FIXME  if this is commented out then not even an approximation of pick on decorations */
    /* adjust the pick box up or down to include the decorations.
       This is only approximate since at this point we don't know how wide that line is, if it has
       an unusual offset, and so forth.  The selection point is set at what is roughly the center of
       the decoration (vertically) for the wide ones, like wavy and double line.
       The text decorations are not actually selectable.
    */
    if (_decorations.overline || _decorations.underline) {
        double top = _asc*scale_bigbox*0.66;
        double bot = 0;
        if (_decorations.overline) {  top =   _asc * scale_bigbox * 1.025; }
        if (_decorations.underline) { bot =  -_dsc * scale_bigbox * 0.2;   }
        Geom::Rect padjbox(Geom::Point(0.0, top),Geom::Point(_width*scale_bigbox, bot));
        pb.unionWith(padjbox * ctx.ctm);
    }
#endif

    if (ggroup->_nrstyle.data.stroke.type != NRStyleData::PaintType::NONE) {
        // this expands the selection box for cases where the stroke is "thick"
        float scale = ctx.ctm.descrim();
        if (_transform) {
            scale /= _transform->descrim(); // FIXME temporary hack
        }
        float width = std::max<double>(0.125, ggroup->_nrstyle.data.stroke_width * scale);
        if (std::fabs(ggroup->_nrstyle.data.stroke_width * scale) > 0.01) { // FIXME: this is always true
            b.expandBy(0.5 * width);
            pb->expandBy(0.5 * width);
        }

        // save bbox without miters for picking
        _pick_bbox = pb->roundOutwards();

        float miterMax = width * ggroup->_nrstyle.data.miter_limit;
        if (miterMax > 0.01) {
            // grunt mode. we should compute the various miters instead
            // (one for each point on the curve)
            b.expandBy(miterMax);
        }
        _bbox = b.roundOutwards();
    } else {
        _bbox = b.roundOutwards();
        _pick_bbox = pb->roundOutwards();
    }

    return STATE_ALL;
}

DrawingItem *DrawingGlyphs::_pickItem(Geom::Point const &p, double /*delta*/, unsigned flags)
{
    auto ggroup = cast<DrawingText>(_parent);
    if (!ggroup) {
        throw InvalidItemException();
    }
    DrawingItem *result = nullptr;
    bool invisible = ggroup->_nrstyle.data.fill.type == NRStyleData::PaintType::NONE &&
                     ggroup->_nrstyle.data.stroke.type == NRStyleData::PaintType::NONE;
    bool outline = flags & PICK_OUTLINE;

    if (pathvec && _bbox && (outline || !invisible)) {
        // With text we take a simple approach: pick if the point is in a character bbox
        Geom::Rect expanded(_pick_bbox);
        // FIXME, why expand by delta?  When is the next line needed?
        // expanded.expandBy(delta);
        if (expanded.contains(p)) {
            result = this;
        }
    }
    return result;
}

DrawingText::DrawingText(Drawing &drawing)
    : DrawingGroup(drawing)
    , style_vector_effect_stroke(false)
    , style_stroke_extensions_hairline(false)
    , style_clip_rule(SP_WIND_RULE_EVENODD)
{
}

bool DrawingText::addComponent(std::shared_ptr<FontInstance> const &font, int glyph, Geom::Affine const &trans, float width, float ascent, float descent, float phase_length)
{
    // original, did not save a glyph for white space characters, causes problems for text-decoration
    /*if (!font || !font->PathVector(glyph)) {
        return false;
    }*/
    if (!font) return false;

    defer([=, this, font = std::move(font)] () mutable {
        _markForRendering();
        auto ng = new DrawingGlyphs(_drawing);
        assert(!_drawing.snapshotted());
        ng->setGlyph(font, glyph, trans);
        ng->_width  = width;   // used especially when _drawable = false, otherwise, it is the advance of the font
        ng->_asc    = ascent;  // of font, not of this one character
        ng->_dsc    = descent; // of font, not of this one character
        ng->_pl     = phase_length; // used for phase of dots, dashes, and wavy
        appendChild(ng);
    });

    return true;
}

void DrawingText::setStyle(SPStyle const *style, SPStyle const *context_style)
{
    DrawingGroup::setStyle(style, context_style);

    auto vector_effect_stroke = false;
    auto stroke_extensions_hairline = false;
    auto clip_rule = SP_WIND_RULE_EVENODD;
    if (_style) {
        vector_effect_stroke = _style->vector_effect.stroke;
        stroke_extensions_hairline = _style->stroke_extensions.hairline;
        clip_rule = _style->clip_rule.computed;
    }

    defer([=, this, nrstyle = NRStyleData(_style, _context_style)] () mutable {
        _nrstyle.set(std::move(nrstyle));
        style_vector_effect_stroke = vector_effect_stroke;
        style_stroke_extensions_hairline = stroke_extensions_hairline;
        style_clip_rule = clip_rule;
    });
}

void DrawingText::setChildrenStyle(SPStyle const *context_style)
{
    DrawingGroup::setChildrenStyle(context_style);

    defer([this, nrstyle = NRStyleData(_style, _context_style)] () mutable {
        _nrstyle.set(std::move(nrstyle));
    });
}

unsigned DrawingText::_updateItem(Geom::IntRect const &area, UpdateContext const &ctx, unsigned flags, unsigned reset)
{
    _nrstyle.invalidate();
    return DrawingGroup::_updateItem(area, ctx, flags, reset);
}

void DrawingText::decorateStyle(DrawingContext &dc, double vextent, double xphase, Geom::Point const &p1, Geom::Point const &p2, double thickness) const
{
    double wave[16]={
        // clang-format off
        0.000000,  0.382499,  0.706825,  0.923651,   1.000000,  0.923651,  0.706825,  0.382499,
        0.000000, -0.382499, -0.706825, -0.923651,  -1.000000, -0.923651, -0.706825, -0.382499,
        // clang-format on
    };
    int dashes[16]={
        // clang-format off
        8,   7,   6,   5,
        4,   3,   2,   1,
        -8, -7,  -6,  -5,
        -4, -3,  -2,  -1
        // clang-format on
    };
    int dots[16]={
        // clang-format off
        4,     3,   2,   1,
        -4,   -3,  -2,  -1,
        4,     3,   2,   1,
        -4,   -3,  -2,  -1
        // clang-format on
    };
    double   step = vextent/32.0;
    unsigned i  = 15 & (unsigned) round(xphase/step);  // xphase is >= 0.0

    /* For most spans draw the last little bit right to p2 or even a little beyond.
       This allows decoration continuity within the line, and does not step outside the clip box off the end
       For the first/last section on the line though, stay well clear of the edge, or when the
       text is dragged it may "spray" pixels.
    */
    /* snap to nearest step in X */
    Geom::Point ps = Geom::Point(step * round(p1[Geom::X]/step),p1[Geom::Y]);
    Geom::Point pf = Geom::Point(step * round(p2[Geom::X]/step),p2[Geom::Y]);
    Geom::Point poff = Geom::Point(0,thickness/2.0);

    if (_nrstyle.data.text_decoration_style & NRStyleData::TEXT_DECORATION_STYLE_ISDOUBLE) {
        ps -= Geom::Point(0, vextent/12.0);
        pf -= Geom::Point(0, vextent/12.0);
        dc.rectangle( Geom::Rect(ps + poff, pf - poff));
        ps += Geom::Point(0, vextent/6.0);
        pf += Geom::Point(0, vextent/6.0);
        dc.rectangle( Geom::Rect(ps + poff, pf - poff));
    }
    /* The next three have a problem in that they are phase dependent.  The bits of a line are not
    necessarily passing through this routine in order, so we have to use the xphase information
    to figure where in each of their cycles to start.  Only accurate to 1 part in 16.
    Huge positive offset should keep the phase calculation from ever being negative.
    */
    else if(_nrstyle.data.text_decoration_style & NRStyleData::TEXT_DECORATION_STYLE_DOTTED){
        // FIXME: Per spec, this should produce round dots.
        Geom::Point pv = ps;
        while(true){
            Geom::Point pvlast = pv;
            if(dots[i]>0){
                if(pv[Geom::X] > pf[Geom::X]) break;

                pv += Geom::Point(step * (double)dots[i], 0.0);

                if(pv[Geom::X]>= pf[Geom::X]){
                    // Last dot
                    dc.rectangle( Geom::Rect(pvlast + poff, pf - poff));
                    break;
                } else {
                    dc.rectangle( Geom::Rect(pvlast + poff, pv - poff));
                }

                pv += Geom::Point(step * 4.0, 0.0);

            } else {
                pv += Geom::Point(step * -(double)dots[i], 0.0);
            }
            i = 0;  // once in phase, it stays in phase
        }
    }
    else if (_nrstyle.data.text_decoration_style & NRStyleData::TEXT_DECORATION_STYLE_DASHED) {
        Geom::Point pv = ps;
        while(true){
            Geom::Point pvlast = pv;
            if(dashes[i]>0){
                if(pv[Geom::X]> pf[Geom::X]) break;

                pv += Geom::Point(step * (double)dashes[i], 0.0);

                if(pv[Geom::X]>= pf[Geom::X]){
                    // Last dash
                    dc.rectangle( Geom::Rect(pvlast + poff, pf - poff));
                    break;
                } else {
                    dc.rectangle( Geom::Rect(pvlast + poff, pv - poff));
                }

                pv += Geom::Point(step * 8.0, 0.0);

            } else {
                pv += Geom::Point(step * -(double)dashes[i], 0.0);
            }
            i = 0;  // once in phase, it stays in phase
        }
    }
    else if (_nrstyle.data.text_decoration_style & NRStyleData::TEXT_DECORATION_STYLE_WAVY) {
        double   amp  = vextent/10.0;
        double   x    = ps[Geom::X];
        double   y    = ps[Geom::Y] + poff[Geom::Y];
        dc.moveTo(Geom::Point(x, y + amp * wave[i]));
        while(true){
           i = ((i + 1) & 15);
           x += step;
           dc.lineTo(Geom::Point(x, y + amp * wave[i]));
           if(x >= pf[Geom::X])break;
        }
        y = ps[Geom::Y] - poff[Geom::Y];
        dc.lineTo(Geom::Point(x, y + amp * wave[i]));
        while(true){
           i = ((i - 1) & 15);
           x -= step;
           dc.lineTo(Geom::Point(x, y + amp * wave[i]));
           if(x <= ps[Geom::X])break;
        }
        dc.closePath();
    }
    else { // TEXT_DECORATION_STYLE_SOLID, also default in case it was not set for some reason
        dc.rectangle( Geom::Rect(ps + poff, pf - poff));
    }
}

/* returns scaled line thickness */
void DrawingText::decorateItem(DrawingContext &dc, double phase_length, bool under) const
{
    if ( _nrstyle.data.font_size <= 1.0e-32 )return;  // might cause a divide by zero or overflow and nothing would be visible anyway
    double tsp_width_adj                = _nrstyle.data.tspan_width                     / _nrstyle.data.font_size;
    double tsp_asc_adj                  = _nrstyle.data.ascender                        / _nrstyle.data.font_size;
    double tsp_size_adj                 = (_nrstyle.data.ascender + _nrstyle.data.descender) / _nrstyle.data.font_size;

    double final_underline_thickness    = CLAMP(_nrstyle.data.underline_thickness,    tsp_size_adj/30.0, tsp_size_adj/10.0);
    double final_line_through_thickness = CLAMP(_nrstyle.data.line_through_thickness, tsp_size_adj/30.0, tsp_size_adj/10.0);

    double xphase = phase_length/ _nrstyle.data.font_size; // used to figure out phase of patterns

    Geom::Point p1;
    Geom::Point p2;
    // All lines must be the same thickness, in combinations, line_through trumps underline
    double thickness = final_underline_thickness;
    if ( thickness <= 1.0e-32 )return;  // might cause a divide by zero or overflow and nothing would be visible anyway
    dc.setTolerance(0.5); // Is this really necessary... could effect dots.

    if( under ) {

        if(_nrstyle.data.text_decoration_line & NRStyleData::TEXT_DECORATION_LINE_UNDERLINE){
            p1 = Geom::Point(0.0,          -_nrstyle.data.underline_position);
            p2 = Geom::Point(tsp_width_adj,-_nrstyle.data.underline_position);
            decorateStyle(dc, tsp_size_adj, xphase, p1, p2, thickness);
        }

        if(_nrstyle.data.text_decoration_line & NRStyleData::TEXT_DECORATION_LINE_OVERLINE){
            p1 = Geom::Point(0.0,          tsp_asc_adj -_nrstyle.data.underline_position + 1 * final_underline_thickness);
            p2 = Geom::Point(tsp_width_adj,tsp_asc_adj -_nrstyle.data.underline_position + 1 * final_underline_thickness);
            decorateStyle(dc, tsp_size_adj, xphase,  p1, p2, thickness);
        }

    } else {
        // Over

        if(_nrstyle.data.text_decoration_line & NRStyleData::TEXT_DECORATION_LINE_LINETHROUGH){
            thickness = final_line_through_thickness;
            p1 = Geom::Point(0.0,          _nrstyle.data.line_through_position);
            p2 = Geom::Point(tsp_width_adj,_nrstyle.data.line_through_position);
            decorateStyle(dc, tsp_size_adj, xphase,  p1, p2, thickness);
        }

        // Obviously this does not blink, but it does indicate which text has been set with that attribute
        if(_nrstyle.data.text_decoration_line & NRStyleData::TEXT_DECORATION_LINE_BLINK){
            thickness = final_line_through_thickness;
            p1 = Geom::Point(0.0,          _nrstyle.data.line_through_position - 2*final_line_through_thickness);
            p2 = Geom::Point(tsp_width_adj,_nrstyle.data.line_through_position - 2*final_line_through_thickness);
            decorateStyle(dc, tsp_size_adj, xphase,  p1, p2, thickness);
            p1 = Geom::Point(0.0,          _nrstyle.data.line_through_position + 2*final_line_through_thickness);
            p2 = Geom::Point(tsp_width_adj,_nrstyle.data.line_through_position + 2*final_line_through_thickness);
            decorateStyle(dc, tsp_size_adj, xphase,  p1, p2, thickness);
        }
    }
}

unsigned DrawingText::_renderItem(DrawingContext &dc, RenderContext &rc, Geom::IntRect const &area, unsigned flags, DrawingItem const *stop_at) const
{
    auto visible = area & _bbox;
    if (!visible) return RENDER_OK;

    bool outline = flags & RENDER_OUTLINE;

    if (outline) {
        auto rgba = rc.outline_color;
        Inkscape::DrawingContext::Save save(dc);
        dc.setSource(rgba);
        dc.setTolerance(0.5); // low quality, but good enough for outline mode

        for (auto & i : _children) {
            auto g = cast<DrawingGlyphs>(&i);
            if (!g) throw InvalidItemException();

            Inkscape::DrawingContext::Save save(dc);
            // skip glyphs with singular transforms
            if (g->_ctm.isSingular()) continue;
            dc.transform(g->_ctm);
            if (g->pathvec){
                dc.path(*g->pathvec);
                dc.fill();
            }
        }
        return RENDER_OK;
    }

    // NOTE: This is very similar to drawing-shape.cpp; the only differences are in path feeding
    // and in applying text decorations.

    // Do we have text decorations?
    bool decorate = (_nrstyle.data.text_decoration_line != NRStyleData::TEXT_DECORATION_LINE_CLEAR );

    // prepareFill / prepareStroke need to be called with _ctm in effect.
    // However, we might need to apply a different ctm for glyphs.
    // Therefore, only apply this ctm temporarily.
    CairoPatternUniqPtr has_stroke;
    CairoPatternUniqPtr has_fill;
    CairoPatternUniqPtr has_td_fill;
    CairoPatternUniqPtr has_td_stroke;

    {
        Inkscape::DrawingContext::Save save(dc);
        dc.transform(_ctm);

        has_fill   = _nrstyle.prepareFill  (dc, rc, *visible, _item_bbox, _fill_pattern);
        has_stroke = _nrstyle.prepareStroke(dc, rc, *visible, _item_bbox, _stroke_pattern);

        // Avoid creating patterns if not needed
        if (decorate) {
            has_td_fill   = _nrstyle.prepareTextDecorationFill  (dc, rc, *visible, _item_bbox, _fill_pattern);
            has_td_stroke = _nrstyle.prepareTextDecorationStroke(dc, rc, *visible, _item_bbox, _stroke_pattern);
        }
    }

    if (has_fill || has_stroke || has_td_fill || has_td_stroke) {

        // Determine order for fill and stroke.
        // Text doesn't have markers, we can do paint-order quick and dirty.
        bool fill_first = false;
        if( _nrstyle.data.paint_order_layer[0] == NRStyleData::PAINT_ORDER_NORMAL ||
            _nrstyle.data.paint_order_layer[0] == NRStyleData::PAINT_ORDER_FILL   ||
            _nrstyle.data.paint_order_layer[2] == NRStyleData::PAINT_ORDER_STROKE ) {
            fill_first = true;
        } // Won't get "stroke fill stroke" but that isn't 'valid'


        // Determine geometry of text decoration
        double phase_length = 0.0;
        Geom::Affine aff;
        if (decorate) {

            Geom::Affine rotinv;
            bool   invset    = false;
            double leftmost  = DBL_MAX;
            bool   first_y   = true;
            double start_y   = 0.0;
            for (auto & i : _children) {

                auto g = cast<DrawingGlyphs>(&i);
                if (!g) throw InvalidItemException();

                if (!invset) {
                    rotinv = g->_ctm.withoutTranslation().inverse();
                    invset = true;
                }

                Geom::Point pt = g->_ctm.translation() * rotinv;
                if (pt[Geom::X] < leftmost) {
                    leftmost     = pt[Geom::X];
                    aff          = g->_ctm;
                    phase_length = g->_pl;
                }

                // Check for text on a path. FIXME: This needs better test (and probably not here).
                if (first_y) {
                    first_y = false;
                    start_y = pt[Geom::Y];
                }
                else if (std::fabs(pt[Geom::Y] - start_y) > 1.0e-6) {
                    //  If the text has been mapped onto a path, which causes y to vary, drop the
                    //  text decorations.  To handle that properly would need a conformal map.
                    decorate = false;
                }
            }
        }

        // Draw text decorations that go UNDER the text (underline, over-line)
        if (decorate) {

            {
                Inkscape::DrawingContext::Save save(dc);
                dc.transform(aff);  // must be leftmost affine in span
                decorateItem(dc, phase_length, true);
            }

            {
                Inkscape::DrawingContext::Save save(dc);
                dc.transform(_ctm);  // Needed so that fill pattern rotates with text

                if (has_td_fill && fill_first) {
                    _nrstyle.applyTextDecorationFill(dc, has_td_fill);
                    dc.fillPreserve();
                }

                if (has_td_stroke) {
                    _nrstyle.applyTextDecorationStroke(dc, has_td_stroke);
                    dc.strokePreserve();
                }

                if (has_td_fill && !fill_first) {
                    _nrstyle.applyTextDecorationFill(dc, has_td_fill);
                    dc.fillPreserve();
                }

            }

            dc.newPath(); // Clear text-decoration path
        }

        // Accumulate the path that represents the glyphs and/or draw SVG glyphs.
        for (auto &i : _children) {
            auto g = cast<DrawingGlyphs>(&i);
            if (!g) throw InvalidItemException();

            Inkscape::DrawingContext::Save save(dc);
            if (g->_ctm.isSingular()) continue;
            dc.transform(g->_ctm);
            if (g->pathvec) {
                if (g->pixbuf) {
                    // Geom::OptRect box = bounds_exact(*g->pathvec);
                    // if (box) {
                    //     Inkscape::DrawingContext::Save save(dc);
                    //     dc.newPath();
                    //     dc.rectangle(*box);
                    //     dc.setLineWidth(0.01);
                    //     dc.setSource(0x8080ffff);
                    //     dc.stroke();
                    // }
                    {
                        // pixbuf is in font design units, scale to embox.
                        double scale = g->design_units;
                        if (scale <= 0) scale = 1000;
                        Inkscape::DrawingContext::Save save(dc);
                        dc.translate(0, 1);
                        dc.scale(1.0 / scale, -1.0 / scale);
                        dc.setSource(g->pixbuf->getSurfaceRaw(), 0, 0);
                        dc.paint(1);
                    }
                } else {
                    dc.path(*g->pathvec);
                }
            }
        }

        // Draw the glyphs (non-SVG glyphs).
        {
            Inkscape::DrawingContext::Save save(dc);
            dc.transform(_ctm);
            if (has_fill && fill_first) {
                _nrstyle.applyFill(dc, has_fill);
                dc.fillPreserve();
            }
        }
        {
            Inkscape::DrawingContext::Save save(dc);
            if (!style_vector_effect_stroke) {
                dc.transform(_ctm);
            }
            if (has_stroke) {
                _nrstyle.applyStroke(dc, has_stroke);

                // If the stroke is a hairline, set it to exactly 1px on screen.
                // If visible hairline mode is on, make sure the line is at least 1px.
                if (flags & RENDER_VISIBLE_HAIRLINES || style_stroke_extensions_hairline) {
                    double dx = 1.0, dy = 0.0;
                    dc.device_to_user_distance(dx, dy);
                    auto pixel_size = std::hypot(dx, dy);
                    if (style_stroke_extensions_hairline || _nrstyle.data.stroke_width < pixel_size) {
                       dc.setHairline();
                    }
                }

                dc.strokePreserve();
            }
        }
        {
            Inkscape::DrawingContext::Save save(dc);
            dc.transform(_ctm);
            if (has_fill && !fill_first) {
                _nrstyle.applyFill(dc, has_fill);
                dc.fillPreserve();
            }
        }
        dc.newPath(); // Clear glyphs path

        // Draw text decorations that go OVER the text (line through, blink)
        if (decorate) {

            {
                Inkscape::DrawingContext::Save save(dc);
                dc.transform(aff);  // must be leftmost affine in span
                decorateItem(dc, phase_length, false);
            }

            {
                Inkscape::DrawingContext::Save save(dc);
                dc.transform(_ctm);  // Needed so that fill pattern rotates with text

                if (has_td_fill && fill_first) {
                    _nrstyle.applyTextDecorationFill(dc, has_td_fill);
                    dc.fillPreserve();
                }

                if (has_td_stroke) {
                    _nrstyle.applyTextDecorationStroke(dc, has_td_stroke);
                    dc.strokePreserve();
                }

                if (has_td_fill && !fill_first) {
                    _nrstyle.applyTextDecorationFill(dc, has_td_fill);
                    dc.fillPreserve();
                }

            }

            dc.newPath(); // Clear text-decoration path
        }

    }
    return RENDER_OK;
}

void DrawingText::_clipItem(DrawingContext &dc, RenderContext &rc, Geom::IntRect const &/*area*/) const
{
    Inkscape::DrawingContext::Save save(dc);

    if (style_clip_rule == SP_WIND_RULE_EVENODD) {
        dc.setFillRule(CAIRO_FILL_RULE_EVEN_ODD);
    } else {
        dc.setFillRule(CAIRO_FILL_RULE_WINDING);
    }

    for (auto & i : _children) {
        auto g = cast<DrawingGlyphs>(&i);
        if (!g) {
            throw InvalidItemException();
        }

        Inkscape::DrawingContext::Save save(dc);
        dc.transform(g->_ctm);
        if (g->pathvec){
            dc.path(*g->pathvec);
        }
    }
    dc.fill();
}

DrawingItem *DrawingText::_pickItem(Geom::Point const &p, double delta, unsigned flags)
{
    return DrawingGroup::_pickItem(p, delta, flags) ? this : nullptr;
}

} // end namespace Inkscape

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
