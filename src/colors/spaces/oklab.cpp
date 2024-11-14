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

#include "oklab.h"

#include <2geom/math-utils.h>
#include <cmath>

#include "colors/printer.h"

namespace Inkscape::Colors::Space {

/* These values are technically unbounded in the actual calculations but are
 * defined between -0.4 and 0.4 by the CSS Color Module specification
 * as the reasonable upper and lower limits for display. Our internal model
 * always scales from 0 to 1 of this expected range of values.
 */
constexpr double MIN_SCALE = -0.4;
constexpr double MAX_SCALE = 0.4;

void OkLab::scaleUp(std::vector<double> &in_out)
{
    in_out[1] = SCALE_UP(in_out[1], MIN_SCALE, MAX_SCALE);
    in_out[2] = SCALE_UP(in_out[2], MIN_SCALE, MAX_SCALE);
}

void OkLab::scaleDown(std::vector<double> &in_out)
{
    in_out[1] = SCALE_DOWN(in_out[1], MIN_SCALE, MAX_SCALE);
    in_out[2] = SCALE_DOWN(in_out[2], MIN_SCALE, MAX_SCALE);
}

/** Two-dimensional array to store a constant 3x3 matrix. */
using Matrix = const double[3][3];

// clang-format off
/** Matrix of the linear transformation from linear RGB space to linear
 * cone responses, used in the first step of RGB to OKLab conversion.
 */
Matrix LRGB2CONE = {{0.4122214708, 0.5363325363, 0.0514459929},
                    {0.2119034982, 0.6806995451, 0.1073969566},
                    {0.0883024619, 0.2817188376, 0.6299787005}};

/** The inverse of the matrix LRGB2CONE. */
Matrix CONE2LRGB = {
    {  4.0767416613479942676681908333711298900607278264432, -3.30771159040819331315866078424893188865618253342,  0.230969928729427886449650619561935920170561518112 },
    { -1.2684380040921760691815055595117506020901414005992,  2.60975740066337143024050095284233623056192338553, -0.341319396310219620992658250306535533187548361872 },
    { -0.0041960865418371092973767821251846315637521173374, -0.70341861445944960601310996913659932654899822384,  1.707614700930944853864541790660472961199090408527 }
};

/** The matrix M2 used in the second step of RGB to OKLab conversion.
 * Taken from https://bottosson.github.io/posts/oklab/ (retrieved 2022).
 */
Matrix M2 = {{0.2104542553, 0.793617785, -0.0040720468},
             {1.9779984951, -2.428592205, 0.4505937099},
             {0.0259040371, 0.7827717662, -0.808675766}};

/** The inverse of the matrix M2. The first column looks like it wants to be 1 but
 * this form is closer to the actual inverse (due to numerics). */
Matrix M2_INVERSE = {
    { 0.99999999845051981426207542502031373637162589278552,  0.39633779217376785682345989261573192476766903603,  0.215803758060758803423141461830037892590617787467 },
    { 1.00000000888176077671607524567047071276183677410134, -0.10556134232365634941095687705472233997368274024, -0.063854174771705903405254198817795633810975771082 },
    { 1.00000005467241091770129286515344610721841028698942, -0.08948418209496575968905274586339134130669669716, -1.291485537864091739948928752914772401878545675371 }
};
// clang-format on

/** Compute the dot-product between two 3D-vectors. */
template <typename A1, typename A2>
inline constexpr double dot3(const A1 &a1, const A2 &a2)
{
    return a1[0] * a2[0] + a1[1] * a2[1] + a1[2] * a2[2];
}

/**
 * Convert a color from the the OKLab colorspace to the Linear RGB colorspace.
 *
 * @param in_out[in,out] The OKLab color converted to a Linear RGB color.
 */
void OkLab::toLinearRGB(std::vector<double> &in_out)
{
    std::array<double, 3> cones;
    for (unsigned i = 0; i < 3; i++) {
        cones[i] = Geom::cube(dot3(M2_INVERSE[i], in_out));
    }
    for (unsigned i = 0; i < 3; i++) {
        in_out[i] = std::clamp(dot3(CONE2LRGB[i], cones), 0.0, 1.0);
    }
}

/**
 * Convert a color from the the Linear RGB colorspace to the OKLab colorspace.
 *
 * @param in_out[in,out] The Linear RGB color converted to a OKLab color.
 */
void OkLab::fromLinearRGB(std::vector<double> &in_out)
{
    std::vector<double> cones(3);
    for (unsigned i = 0; i < 3; i++) {
        cones[i] = std::cbrt(dot3(LRGB2CONE[i], in_out));
    }
    for (unsigned i = 0; i < 3; i++) {
        in_out[i] = dot3(M2[i], cones);
    }
}

bool OkLab::Parser::parse(std::istringstream &ss, std::vector<double> &output) const
{
    bool end = false;
    if (append_css_value(ss, output, end, ',')               // Luminance
        && append_css_value(ss, output, end, ',', MAX_SCALE) // Chroma A
        && append_css_value(ss, output, end, '/', MAX_SCALE) // Chroma B
        && (append_css_value(ss, output, end) || true)       // Optional opacity
        && end) {
        // Values are between -100% and 100% so post processed into the range 0 to 1
        output[1] = (output[1] + 1) / 2;
        output[2] = (output[2] + 1) / 2;
        return true;
    }
    return false;
}

/**
 * Print the Lab color to a CSS string.
 *
 * @arg values - A vector of doubles for each channel in the Lch space
 * @arg opacity - True if the opacity should be included in the output.
 */
std::string OkLab::toString(std::vector<double> const &values, bool opacity) const
{
    auto os = CssFuncPrinter(3, "oklab");

    os << values[0]                                  // Luminance
       << SCALE_UP(values[1], MIN_SCALE, MAX_SCALE)  // Chroma A
       << SCALE_UP(values[2], MIN_SCALE, MAX_SCALE); // Chroma B

    if (opacity && values.size() == 4)
        os << values[3]; // Optional opacity

    return os;
}

}; // namespace Inkscape::Colors::Space
