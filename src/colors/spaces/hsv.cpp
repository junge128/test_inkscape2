// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2023 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "hsv.h"

#include <cmath>

#include "colors/color.h"
#include "colors/printer.h"

namespace Inkscape::Colors::Space {

/**
 * Convert the HSV color into sRGB components used in the sRGB icc profile.
 */
void HSV::spaceToProfile(std::vector<double> &output) const
{
    double v = output[2];
    double d = output[0] * 5.99999999;
    double f = d - std::floor(d);
    double w = v * (1.0 - output[1]);
    double q = v * (1.0 - (output[1] * f));
    double t = v * (1.0 - (output[1] * (1.0 - f)));

    if (d < 1.0) {
        output[0] = v;
        output[1] = t;
        output[2] = w;
    } else if (d < 2.0) {
        output[0] = q;
        output[1] = v;
        output[2] = w;
    } else if (d < 3.0) {
        output[0] = w;
        output[1] = v;
        output[2] = t;
    } else if (d < 4.0) {
        output[0] = w;
        output[1] = q;
        output[2] = v;
    } else if (d < 5.0) {
        output[0] = t;
        output[1] = w;
        output[2] = v;
    } else {
        output[0] = v;
        output[1] = w;
        output[2] = q;
    }
}

/**
 * Convert from sRGB icc values to HSV values
 */
void HSV::profileToSpace(std::vector<double> &output) const
{
    double r = output[0];
    double g = output[1];
    double b = output[2];

    double max = std::max(std::max(r, g), b);
    double min = std::min(std::min(r, g), b);
    double delta = max - min;

    output[2] = max;
    output[1] = max > 0 ? delta / max : 0.0;

    if (output[1] != 0.0) {
        if (r == max) {
            output[0] = (g - b) / delta;
        } else if (g == max) {
            output[0] = 2.0 + (b - r) / delta;
        } else {
            output[0] = 4.0 + (r - g) / delta;
        }
        output[0] = output[0] / 6.0;
        if (output[0] < 0)
            output[0] += 1.0;
    } else
        output[0] = 0.0;
}

/**
 * Parse the hwb css string and convert to hsv inline, if it exists in the input string stream.
 */
bool HSV::fromHwbParser::parse(std::istringstream &ss, std::vector<double> &output) const
{
    if (HueParser::parse(ss, output)) {
        // See https://en.wikipedia.org/wiki/HWB_color_model#Converting_to_and_from_HSV
        auto scale = output[1] + output[2];
        if (scale > 1.0) {
            output[1] /= scale;
            output[2] /= scale;
        }
        output[1] = output[2] == 1.0 ? 0.0 : (1.0 - (output[1] / (1.0 - output[2])));
        output[2] = 1.0 - output[2];
        return true;
    }
    return false;
}

/**
 * Print the HSV color to a CSS hwb() string.
 *
 * @arg values - A vector of doubles for each channel in the HSV space
 * @arg opacity - True if the opacity should be included in the output.
 */
std::string HSV::toString(std::vector<double> const &values, bool opacity) const
{
    auto oo = CssLegacyPrinter(3, "hwb", opacity && values.size() == 4);
    // First entry is Hue, which is in degrees, white and black are dirived
    return oo << (int)(values[0] * 360)        // Hue, degrees, 0..360
              << (1.0 - values[1]) * values[2] // White
              << 1.0 - values[2]               // Black
              << values.back();                // Opacity
}

}; // namespace Inkscape::Colors::Space
