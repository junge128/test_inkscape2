// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 *//*
 * Authors:
 *   Rafał Siejakowski <rs@rs-math.net>
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2023 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "linear-rgb.h"

#include <cmath>

#include "colors/printer.h"

namespace Inkscape::Colors::Space {

/**
 * Convenience function used for RGB conversions.
 *
 * @param c Value.
 * @return RGB color component.
 */
double from_linear(double c)
{
    if (c <= 0.0031308) {
        return 12.92 * c;
    } else {
        return 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
    }
}

/**
 * Convenience function used for RGB conversions.
 *
 * @param c Value.
 * @return XYZ color component.
 */
double to_linear(double c)
{
    if (c > 0.04045) {
        return std::pow((c + 0.055) / 1.055, 2.4);
    } else {
        return c / 12.92;
    }
}

/**
 * Convert a color from the a linear RGB colorspace to the sRGB colorspace.
 *
 * @param in_out[in,out] The linear RGB color converted to a RGB color.
 */
void LinearRGB::toRGB(std::vector<double> &in_out)
{
    in_out[0] = from_linear(in_out[0]);
    in_out[1] = from_linear(in_out[1]);
    in_out[2] = from_linear(in_out[2]);
}

/**
 * Convert from sRGB icc values to linear RGB values
 *
 * @param in_out[in,out] The RGB color converted to a linear RGB color.
 */
void LinearRGB::fromRGB(std::vector<double> &in_out)
{
    in_out[0] = to_linear(in_out[0]);
    in_out[1] = to_linear(in_out[1]);
    in_out[2] = to_linear(in_out[2]);
}

/**
 * Print the RGB color to a CSS Color module 4 srgb-linear color.
 *
 * @arg values - A vector of doubles for each channel in the RGB space
 * @arg opacity - True if the opacity should be included in the output.
 */
std::string LinearRGB::toString(std::vector<double> const &values, bool opacity) const
{
    auto os = CssColorPrinter(3, "srgb-linear");
    os << values;
    if (opacity && values.size() == 4)
        os << values.back();
    return os;
}

}; // namespace Inkscape::Colors::Space
