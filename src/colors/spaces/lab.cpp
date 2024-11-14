// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 *//*
 * Authors:
 *   2023 Martin Owens
 *
 * Copyright (C) 2023 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "lab.h"

#include <cmath>

#include "colors/color.h"
#include "colors/printer.h"

namespace Inkscape::Colors::Space {

constexpr double LUMA_SCALE = 100;

// NOTE! Inkscape's calculations use a range of 256, while CSS uses 250
constexpr double MIN_SCALE = -128;
constexpr double MAX_SCALE = 128;
constexpr double MIN_CSS_SCALE = -125;
constexpr double MAX_CSS_SCALE = 125;

/**
 * Changes the values from 0..1, to typical lab scaling used in calculations.
 */
void Lab::scaleUp(std::vector<double> &in_out)
{
    in_out[0] = SCALE_UP(in_out[0], 0, LUMA_SCALE);
    in_out[1] = SCALE_UP(in_out[1], MIN_SCALE, MAX_SCALE);
    in_out[2] = SCALE_UP(in_out[2], MIN_SCALE, MAX_SCALE);
}

/**
 * Changes the values from typical lab scaling (see above) to values 0..1.
 */
void Lab::scaleDown(std::vector<double> &in_out)
{
    in_out[0] = SCALE_DOWN(in_out[0], 0, LUMA_SCALE);
    in_out[1] = SCALE_DOWN(in_out[1], MIN_SCALE, MAX_SCALE);
    in_out[2] = SCALE_DOWN(in_out[2], MIN_SCALE, MAX_SCALE);
}

/**
 * Convert a color from the the Lab colorspace to the XYZ colorspace.
 *
 * @param in_out[in,out] The Lab color converted to a XYZ color.
 */
void Lab::toXYZ(std::vector<double> &in_out)
{
    scaleUp(in_out);

    double y = (in_out[0] + 16.0) / 116.0;
    in_out[0] = in_out[1] / 500.0 + y;
    in_out[1] = y;
    in_out[2] = y - in_out[2] / 200.0;

    for (unsigned i = 0; i < 3; i++) {
        double x3 = std::pow(in_out[i], 3);
        if (x3 > 0.008856) {
            in_out[i] = x3;
        } else {
            in_out[i] = (in_out[i] - 16.0 / 116.0) / 7.787;
        }
        in_out[i] *= illuminant_d65[i];
    }
}

/**
 * Convert a color from the the XYZ colorspace to the Lab colorspace.
 *
 * @param in_out[in,out] The XYZ color converted to a Lab color.
 */
void Lab::fromXYZ(std::vector<double> &in_out)
{
    for (unsigned i = 0; i < 3; i++) {
        in_out[i] /= illuminant_d65[i];
    }

    double l;
    if (in_out[1] > 0.008856) {
        l = 116 * std::pow(in_out[1], 0.33333) - 16;
    } else {
        l = 903.3 * in_out[1];
    }

    for (unsigned i = 0; i < 3; i++) {
        if (in_out[i] > 0.008856) {
            in_out[i] = std::pow(in_out[i], 0.33333);
        } else {
            in_out[i] = 7.787 * in_out[i] + 16.0 / 116.0;
        }
    };
    in_out[2] = 200 * (in_out[1] - in_out[2]);
    in_out[1] = 500 * (in_out[0] - in_out[1]);
    in_out[0] = l;

    scaleDown(in_out);
}

/**
 * Print the Lab color to a CSS string.
 *
 * @arg values - A vector of doubles for each channel in the Lab space
 * @arg opacity - True if the opacity should be included in the output.
 */
std::string Lab::toString(std::vector<double> const &values, bool opacity) const
{
    auto os = CssFuncPrinter(3, "lab");

    os << values[0] * LUMA_SCALE                             // Luminance
       << SCALE_UP(values[1], MIN_CSS_SCALE, MAX_CSS_SCALE)  // Chroma A
       << SCALE_UP(values[2], MIN_CSS_SCALE, MAX_CSS_SCALE); // Chroma B

    if (opacity && values.size() == 4)
        os << values[3]; // Optional opacity

    return os;
}

bool Lab::Parser::parse(std::istringstream &ss, std::vector<double> &output) const
{
    bool end = false;
    if (append_css_value(ss, output, end, ',', LUMA_SCALE)       // Lightness
        && append_css_value(ss, output, end, ',', MAX_CSS_SCALE) // Chroma-A
        && append_css_value(ss, output, end, '/', MAX_CSS_SCALE) // Chroma-B
        && (append_css_value(ss, output, end) || true)           // Optional opacity
        && end) {
        // The A and B portions are between -100% and 100% leading to this post unit aditional conversion.
        output[1] = (output[1] + 1) / 2;
        output[2] = (output[2] + 1) / 2;
        return true;
    }
    return false;
}

}; // namespace Inkscape::Colors::Space
