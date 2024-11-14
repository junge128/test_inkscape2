// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 *//*
 * Authors:
 *   2015 Alexei Boronine (original idea, JavaScript implementation)
 *   2015 Roger Tallada (Obj-C implementation)
 *   2017 Martin Mitas (C implementation, based on Obj-C implementation)
 *   2021 Massinissa Derriche (C++ implementation for Inkscape, based on C implementation)
 *   2023 Martin Owens (New Color classes)
 *
 * Copyright (C) 2023 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "lch.h"

#include <2geom/ray.h>

#include "colors/printer.h"

namespace Inkscape::Colors::Space {

constexpr double LUMA_SCALE = 100;
constexpr double CHROMA_SCALE = 150;
constexpr double HUE_SCALE = 360;

/**
 * Changes the values from 0..1, to typical lch scaling used
 * in calculations. L:0..100, C:0..150 H:0..360
 */
void Lch::scaleUp(std::vector<double> &in_out)
{
    in_out[0] = SCALE_UP(in_out[0], 0, LUMA_SCALE);
    in_out[1] = SCALE_UP(in_out[1], 0, CHROMA_SCALE);
    in_out[2] = SCALE_UP(in_out[2], 0, HUE_SCALE);
}

/**
 * Changes the values from typical lch scaling (see above) to
 * values 0..1 used in the color module.
 */
void Lch::scaleDown(std::vector<double> &in_out)
{
    in_out[0] = SCALE_DOWN(in_out[0], 0, LUMA_SCALE);
    in_out[1] = SCALE_DOWN(in_out[1], 0, CHROMA_SCALE);
    in_out[2] = SCALE_DOWN(in_out[2], 0, HUE_SCALE);
}

/**
 * Convert a color from the the LCH colorspace to the Luv colorspace.
 *
 * @param in_out[in,out] The LCH color converted to a Luv color.
 */
void Lch::toLuv(std::vector<double> &in_out)
{
    double sinhrad, coshrad;
    Geom::sincos(Geom::rad_from_deg(in_out[2]), sinhrad, coshrad);
    double u = coshrad * in_out[1];
    double v = sinhrad * in_out[1];

    in_out[1] = u;
    in_out[2] = v;
}

/**
 * Convert a color from the the Luv colorspace to the LCH colorspace.
 *
 * @param in_out[in,out] The Luv color converted to a LCH color.
 */
void Lch::fromLuv(std::vector<double> &in_out)
{
    double l = in_out[0];
    auto uv = Geom::Point(in_out[1], in_out[2]);
    double h;
    double const c = uv.length();

    /* Grays: disambiguate hue */
    if (c < 0.00000001) {
        h = 0;
    } else {
        h = Geom::deg_from_rad(Geom::atan2(uv));
        if (h < 0.0) {
            h += 360.0;
        }
    }
    in_out[0] = l;
    in_out[1] = c;
    in_out[2] = h;
}

/**
 * Print the Lch color to a CSS string.
 *
 * @arg values - A vector of doubles for each channel in the Lch space
 * @arg opacity - True if the opacity should be included in the output.
 */
std::string Lch::toString(std::vector<double> const &values, bool opacity) const
{
    auto os = CssFuncPrinter(3, "lch");

    os << values[0] * LUMA_SCALE   // Luminance
       << values[1] * CHROMA_SCALE // Chroma
       << values[2] * HUE_SCALE;   // Hue

    if (opacity && values.size() == 4)
        os << values[3]; // Optional opacity
    return os;
}

bool Lch::Parser::parse(std::istringstream &ss, std::vector<double> &output) const
{
    bool end = false;
    return append_css_value(ss, output, end, ',', LUMA_SCALE)      // Luminance
           && append_css_value(ss, output, end, ',', CHROMA_SCALE) // Chroma
           && append_css_value(ss, output, end, '/', HUE_SCALE)    // Hue
           && (append_css_value(ss, output, end) || true)          // Optional opacity
           && end;
}

}; // namespace Inkscape::Colors::Space
