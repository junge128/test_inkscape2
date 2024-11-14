// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2023 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "hsl.h"

#include <cmath>

#include "colors/color.h"
#include "colors/printer.h"

namespace Inkscape::Colors::Space {

static float hue_2_rgb(float v1, float v2, float h)
{
    if (h < 0)
        h += 6.0;
    if (h > 6)
        h -= 6.0;
    if (h < 1)
        return v1 + (v2 - v1) * h;
    if (h < 3)
        return v2;
    if (h < 4)
        return v1 + (v2 - v1) * (4 - h);
    return v1;
}

/**
 * Convert the HSL color into sRGB components used in the sRGB icc profile.
 */
void HSL::spaceToProfile(std::vector<double> &output) const
{
    double h = output[0];
    double s = output[1];
    double l = output[2];

    if (s == 0) { // Gray
        output[0] = l;
        output[1] = l;
        output[2] = l;
    } else {
        double v2;
        if (l < 0.5) {
            v2 = l * (1 + s);
        } else {
            v2 = l + s - l * s;
        }
        double v1 = 2 * l - v2;

        output[0] = hue_2_rgb(v1, v2, h * 6 + 2.0);
        output[1] = hue_2_rgb(v1, v2, h * 6);
        output[2] = hue_2_rgb(v1, v2, h * 6 - 2.0);
    }
}

/**
 * Convert from sRGB icc values to HSL values
 */
void HSL::profileToSpace(std::vector<double> &output) const
{
    double r = output[0];
    double g = output[1];
    double b = output[2];

    double max = std::max(std::max(r, g), b);
    double min = std::min(std::min(r, g), b);
    double delta = max - min;

    double h = 0;
    double s = 0;
    double l = (max + min) / 2.0;

    if (delta != 0) {
        if (l <= 0.5)
            s = delta / (max + min);
        else
            s = delta / (2 - max - min);

        if (r == max)
            h = (g - b) / delta;
        else if (g == max)
            h = 2.0 + (b - r) / delta;
        else if (b == max)
            h = 4.0 + (r - g) / delta;

        h = h / 6.0;

        if (h < 0)
            h += 1;
        if (h > 1)
            h -= 1;
    }
    output[0] = h;
    output[1] = s;
    output[2] = l;
}

/**
 * Print the HSL color to a CSS string.
 *
 * @arg values - A vector of doubles for each channel in the HSL space
 * @arg opacity - True if the opacity should be included in the output.
 */
std::string HSL::toString(std::vector<double> const &values, bool opacity) const
{
    auto oo = CssLegacyPrinter(3, "hsl", opacity && values.size() == 4);
    // First entry is Hue, which is in degrees
    return oo << (int)(values[0] * 360) << values[1] << values[2] << values.back();
}

}; // namespace Inkscape::Colors::Space
