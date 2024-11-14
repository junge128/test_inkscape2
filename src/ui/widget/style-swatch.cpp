// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Static style swatch (fill, stroke, opacity).
 */
/* Authors:
 *   buliabyak@gmail.com
 *   Krzysztof Kosiński <tweenk.pl@gmail.com>
 *
 * Copyright (C) 2005-2008 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "style-swatch.h"

#include <functional>
#include <gtkmm/enums.h>
#include <utility>
#include <glibmm/i18n.h>
#include <gtkmm/grid.h>
#include <sigc++/adaptors/bind.h>
#include <sigc++/functors/mem_fun.h>

#include "style.h"

#include "actions/actions-tools.h"  // Open tool preferences.
#include "object/sp-linear-gradient.h"
#include "object/sp-paint-server.h"
#include "object/sp-pattern.h"
#include "object/sp-radial-gradient.h"
#include "ui/controller.h"
#include "ui/pack.h"
#include "ui/util.h"
#include "ui/widget/color-preview.h"
#include "util/units.h"
#include "xml/sp-css-attr.h"

static constexpr int STYLE_SWATCH_WIDTH = 135;

enum {
    SS_FILL,
    SS_STROKE
};

namespace Inkscape::UI::Widget {

/**
 * Watches for changes in the observed style pref.
 */
void style_obs_callback(StyleSwatch &_style_swatch, Preferences::Entry const &val)
{
    SPCSSAttr *css = val.getInheritedStyle();
    _style_swatch.setStyle(css);
    sp_repr_css_attr_unref(css);
}

/**
 * Watches whether the tool uses the current style.
 */
void tool_obs_callback(StyleSwatch &_style_swatch, Preferences::Entry const &val)
{
    auto const prefs = Preferences::get();
    Glib::ustring path;
    SPCSSAttr *css = nullptr;

    bool usecurrent = val.getBool();
    if (usecurrent) {
        path = "/desktop/style";
        css = prefs->getStyle(path);
        const auto &al = css->attributeList();
        if (al.empty()) {
            // Fallback to own style if desktop style empty (does this ever happen?).
            sp_repr_css_attr_unref(css);
            css = nullptr;
        }
    }

    if (!css) {
        path = _style_swatch._tool_path + "/style";
        css = prefs->getInheritedStyle(path);
    }

    // Set style at least once.
    _style_swatch.setStyle(css);
    sp_repr_css_attr_unref(css);

    auto callback = sigc::bind<0>(&style_obs_callback, std::ref(_style_swatch));
    _style_swatch._style_obs = StyleSwatch::PrefObs::create(std::move(path), std::move(callback));
}

StyleSwatch::StyleSwatch(SPCSSAttr *css, gchar const *main_tip, Gtk::Orientation orient)
    : Gtk::Box(Gtk::Orientation::HORIZONTAL),
      _desktop(nullptr),
      _css(nullptr),
      _table(Gtk::make_managed<Gtk::Grid>()),
      _sw_unit(nullptr),
      _stroke(Gtk::Orientation::HORIZONTAL)
{
    set_name("StyleSwatch");
    add_css_class(orient == Gtk::Orientation::HORIZONTAL ? "horizontal" : "vertical");
    _label[SS_FILL].set_markup(_("Fill:"));
    _label[SS_STROKE].set_markup(_("Stroke:"));

    for (int i = SS_FILL; i <= SS_STROKE; i++) {
        _label[i].set_halign(Gtk::Align::START);
        _label[i].set_valign(Gtk::Align::CENTER);
        _label[i].set_margin_top(0);
        _label[i].set_margin_bottom(0);
        _label[i].set_margin_start(0);
        _label[i].set_margin_end(0);

        _color_preview[i] = std::make_unique<ColorPreview>(0);
    }

    _opacity_value.set_halign(Gtk::Align::START);
    _opacity_value.set_valign(Gtk::Align::CENTER);
    _opacity_value.set_margin_top(0);
    _opacity_value.set_margin_bottom(0);
    _opacity_value.set_margin_start(0);
    _opacity_value.set_margin_end(0);

    _table->set_column_spacing(2);
    _table->set_row_spacing(0);

    // We let pack()ed children expand but donʼt propagate expand upwards.
    set_hexpand(false);
    _stroke.set_hexpand(false);

    UI::pack_start(_stroke, _place[SS_STROKE]);
    UI::pack_start(_stroke, _stroke_width, UI::PackOptions::shrink);
    
    if (orient == Gtk::Orientation::VERTICAL) {
        _table->attach(_label[SS_FILL],   0, 0, 1, 1);
        _table->attach(_label[SS_STROKE], 0, 1, 1, 1);
        _table->attach(_place[SS_FILL],   1, 0, 1, 1);
        _table->attach(_stroke,           1, 1, 1, 1);
        _table->attach(_empty_space,      2, 0, 1, 2);
        _table->attach(_opacity_value,    2, 0, 1, 2);
        UI::pack_start(*this, *_table, true, true);

        set_size_request (STYLE_SWATCH_WIDTH, -1);
    }
    else {
        _table->set_column_spacing(4);
        _table->attach(_label[SS_FILL],   0, 0, 1, 1);
        _table->attach(_place[SS_FILL],   1, 0, 1, 1);
        _label[SS_STROKE].set_margin_start(6);
        _table->attach(_label[SS_STROKE], 2, 0, 1, 1);
        _table->attach(_stroke,           3, 0, 1, 1);
        _opacity_value.set_margin_start(6);
        _table->attach(_opacity_value,    4, 0, 1, 1);
        UI::pack_start(*this, *_table, true, true);

        int patch_w = 6 * 6;
        _place[SS_FILL].set_size_request(patch_w, -1);
        _place[SS_STROKE].set_size_request(patch_w, -1);
    }

    setStyle (css);

    if (main_tip) {
        _table->set_tooltip_text(main_tip);
    }
}

void StyleSwatch::setToolName(const Glib::ustring& tool_name) {
    _tool_name = tool_name;
}

void StyleSwatch::setDesktop(SPDesktop *desktop) {
    _desktop = desktop;
}

StyleSwatch::~StyleSwatch()
{
    if (_css)
        sp_repr_css_attr_unref (_css);
}

void
StyleSwatch::setWatchedTool(const char *path, bool synthesize)
{
    _tool_obs.reset();

    if (path) {
        _tool_path = path;
        _tool_obs = PrefObs::create(_tool_path + "/usecurrent",
                                    sigc::bind<0>(&tool_obs_callback, std::ref(*this)));
    } else {
        _tool_path = "";
    }
    
    if (synthesize && _tool_obs) {
        _tool_obs->call();
    }
}


void StyleSwatch::setStyle(SPCSSAttr *css)
{
    if (_css)
        sp_repr_css_attr_unref (_css);

    if (!css)
        return;

    _css = sp_repr_css_attr_new();
    sp_repr_css_merge(_css, css);

    Glib::ustring css_string;
    sp_repr_css_write_string (_css, css_string);

    SPStyle style(_desktop ? _desktop->getDocument() : nullptr);
    if (!css_string.empty()) {
        style.mergeString(css_string.c_str());
    }
    setStyle (&style);
}

void StyleSwatch::setStyle(SPStyle *query)
{
    UI::remove_all_children(_place[SS_FILL  ]);
    UI::remove_all_children(_place[SS_STROKE]);

    bool has_stroke = true;

    for (int i = SS_FILL; i <= SS_STROKE; i++) {
        auto const place = &_place[i];

        SPIPaint *paint;
        if (i == SS_FILL) {
            paint = &(query->fill);
        } else {
            paint = &(query->stroke);
        }

        if (paint->set && paint->isPaintserver()) {
            SPPaintServer *server = (i == SS_FILL)? SP_STYLE_FILL_SERVER (query) : SP_STYLE_STROKE_SERVER (query);

            if (is<SPLinearGradient>(server)) {
                _value[i].set_markup(_("L Gradient"));
                place->append(_value[i]);
                place->set_tooltip_text((i == SS_FILL)? (_("Linear gradient (fill)")) : (_("Linear gradient (stroke)")));
            } else if (is<SPRadialGradient>(server)) {
                _value[i].set_markup(_("R Gradient"));
                place->append(_value[i]);
                place->set_tooltip_text((i == SS_FILL)? (_("Radial gradient (fill)")) : (_("Radial gradient (stroke)")));
            } else if (is<SPPattern>(server)) {
                _value[i].set_markup(_("Pattern"));
                place->append(_value[i]);
                place->set_tooltip_text((i == SS_FILL)? (_("Pattern (fill)")) : (_("Pattern (stroke)")));
            }
        } else if (paint->set && paint->isColor()) {
            auto color = paint->getColor();
            color.addOpacity(i == SS_FILL ? query->fill_opacity : query->stroke_opacity);
            _color_preview[i]->setRgba32(color.toRGBA());
            place->append(*_color_preview[i]);
            gchar *tip;
            if (i == SS_FILL) {
                tip = g_strdup_printf (_("Fill: %s"), color.toString().c_str());
            } else {
                tip = g_strdup_printf (_("Stroke: %s"), color.toString().c_str());
            }
            place->set_tooltip_text(tip);
            g_free (tip);
        } else if (paint->set && paint->isNone()) {
            _value[i].set_markup(C_("Fill and stroke", "<i>None</i>"));
            place->append(_value[i]);
            place->set_tooltip_text((i == SS_FILL)? (C_("Fill and stroke", "No fill")) : (C_("Fill and stroke", "No stroke")));
            if (i == SS_STROKE) has_stroke = false;
        } else if (!paint->set) {
            _value[i].set_markup(_("<b>Unset</b>"));
            place->append(_value[i]);
            place->set_tooltip_text((i == SS_FILL)? (_("Unset fill")) : (_("Unset stroke")));
            if (i == SS_STROKE) has_stroke = false;
        }
    }

// Now query stroke_width
    if (has_stroke) {
        if (query->stroke_extensions.hairline) {
            Glib::ustring swidth = "<small>";
            swidth += _("Hairline");
            swidth += "</small>";
            _stroke_width.set_markup(swidth.c_str());
            auto str = Glib::ustring::compose(_("Stroke width: %1"), _("Hairline"));
            _stroke_width.set_tooltip_text(str);
        } else {
            double w;
            if (_sw_unit) {
                w = Util::Quantity::convert(query->stroke_width.computed, "px", _sw_unit);
            } else {
                w = query->stroke_width.computed;
            }

            {
                gchar *str = g_strdup_printf(" %.3g", w);
                Glib::ustring swidth = "<small>";
                swidth += str;
                swidth += "</small>";
                _stroke_width.set_markup(swidth.c_str());
                g_free (str);
            }
            {
                gchar *str = g_strdup_printf(_("Stroke width: %.5g%s"),
                                             w,
                                             _sw_unit? _sw_unit->abbr.c_str() : "px");
                _stroke_width.set_tooltip_text(str);
                g_free (str);
            }
        }
    } else {
        _stroke_width.set_tooltip_text("");
        _stroke_width.set_markup("");
    }

    gdouble op = SP_SCALE24_TO_FLOAT(query->opacity.value);
    if (op != 1) {
        {
            gchar *str;
            str = g_strdup_printf(_("O: %2.0f"), (op*100.0));
            Glib::ustring opacity = "<small>";
            opacity += str;
            opacity += "</small>";
            _opacity_value.set_markup (opacity.c_str());
            g_free (str);
        }
        {
            gchar *str = g_strdup_printf(_("Opacity: %2.1f %%"), (op*100.0));
            _opacity_value.set_tooltip_text(str);
            g_free (str);
        }
    } else {
        _opacity_value.set_tooltip_text("");
        _opacity_value.set_markup("");
    }
}

} // namespace Inkscape::UI::Widget

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
