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

#include "xyz.h"

#include <cmath>

#include "colors/printer.h"

namespace Inkscape::Colors::Space {

/**
 * Calculate the dot product of the given arrays.
 *
 * @param t1 The first array.
 * @param t2 The second array.
 * @return The resulting dot product.
 */
static double dot_product(std::vector<double> const &t1, std::vector<double> const &t2)
{
    return (t1[0] * t2[0] + t1[1] * t2[1] + t1[2] * t2[2]);
}

/**
 * Convert a color from the the XYZ colorspace to the RGB colorspace.
 *
 * @param in_out[in,out] The XYZ color converted to a RGB color.
 */
void XYZ::toLinearRGB(std::vector<double> &in_out)
{
    std::vector<double> result = in_out; // copy
    for (size_t i : {0, 1, 2}) {
        result[i] = dot_product(d65[i], in_out);
    }
    in_out = result;
}

/**
 * Convert from sRGB icc values to XYZ values
 *
 * @param in_out[in,out] The RGB color converted to a XYZ color.
 */
void XYZ::fromLinearRGB(std::vector<double> &in_out)
{
    std::vector<double> result = in_out; // copy
    for (size_t i : {0, 1, 2}) {
        result[i] = dot_product(in_out, d65_inv[i]);
    }
    in_out = result;
}

/**
 * Print the RGB color to a CSS Color module 4 xyz-d65 color.
 *
 * @arg values - A vector of doubles for each channel in the RGB space
 * @arg opacity - True if the opacity should be included in the output.
 */
std::string XYZ::toString(std::vector<double> const &values, bool opacity) const
{
    auto os = CssColorPrinter(3, "xyz");
    os << values;
    if (opacity && values.size() == 4)
        os << values[3];
    return os;
}

}; // namespace Inkscape::Colors::Space
