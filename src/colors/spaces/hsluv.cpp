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

#include "hsluv.h"

#include <cmath>

namespace Inkscape::Colors::Space {

/**
 * Calculate the bounds of the Luv colors in RGB gamut.
 *
 * @param l Lightness. Between 0.0 and 100.0.
 * @return Bounds of Luv colors in RGB gamut.
 */
std::array<Geom::Line, 6> HSLuv::get_bounds(double l)
{
    std::array<Geom::Line, 6> bounds;

    double tl = l + 16.0;
    double sub1 = (tl * tl * tl) / 1560896.0;
    double sub2 = (sub1 > EPSILON ? sub1 : (l / KAPPA));
    int channel;
    int t;

    for (channel = 0; channel < 3; channel++) {
        double m1 = d65[channel][0];
        double m2 = d65[channel][1];
        double m3 = d65[channel][2];

        for (t = 0; t < 2; t++) {
            double top1 = (284517.0 * m1 - 94839.0 * m3) * sub2;
            double top2 = (838422.0 * m3 + 769860.0 * m2 + 731718.0 * m1) * l * sub2 - 769860.0 * t * l;
            double bottom = (632260.0 * m3 - 126452.0 * m2) * sub2 + 126452.0 * t;

            bounds[channel * 2 + t].setCoefficients(top1, -bottom, top2);
        }
    }

    return bounds;
}

/**
 * Calculate the maximum in gamut chromaticity for the given luminance and hue.
 *
 * @param l Luminance.
 * @param h Hue.
 * @return The maximum chromaticity.
 */
static double max_chroma_for_lh(double l, double h)
{
    double min_len = std::numeric_limits<double>::max();
    auto const ray = Geom::Ray(Geom::Point(0, 0), Geom::rad_from_deg(h));

    for (auto const &line : HSLuv::get_bounds(l)) {
        auto intersections = line.intersect(ray);
        if (intersections.empty()) {
            continue;
        }
        double len = intersections[0].point().length();

        if (len >= 0 && len < min_len) {
            min_len = len;
        }
    }

    return min_len;
}

/**
 * Convert a color from the the HSLuv colorspace to the LCH colorspace.
 *
 * @param in_out[in,out] The HSLuv color converted to a LCH color.
 */
void HSLuv::toLch(std::vector<double> &in_out)
{
    double h = in_out[0] * 360;
    double s = in_out[1] * 100;
    double l = in_out[2] * 100;
    double c;

    /* White and black: disambiguate chroma */
    if (l > 99.9999999 || l < 0.00000001) {
        c = 0.0;
    } else {
        c = max_chroma_for_lh(l, h) / 100.0 * s;
    }

    /* Grays: disambiguate hue */
    if (s < 0.00000001) {
        h = 0.0;
    }

    in_out[0] = l;
    in_out[1] = c;
    in_out[2] = h;
}

/**
 * Convert a color from the the LCH colorspace to the HSLuv colorspace.
 *
 * @param in_out[in,out] The LCH color converted to a HSLuv color.
 */
void HSLuv::fromLch(std::vector<double> &in_out)
{
    double l = in_out[0];
    double c = in_out[1];
    double h = in_out[2];
    double s;

    /* White and black: disambiguate saturation */
    if (l > 99.9999999 || l < 0.00000001) {
        s = 0.0;
    } else {
        s = c / max_chroma_for_lh(l, h) * 100.0;
    }

    /* Grays: disambiguate hue */
    if (c < 0.00000001) {
        h = 0.0;
    }

    in_out[0] = h / 360;
    in_out[1] = s / 100;
    in_out[2] = l / 100;
}

}; // namespace Inkscape::Colors::Space
