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

#include "okhsl.h"

#include <2geom/angle.h>
#include <cmath>

#include "oklch.h" // max_chroma

namespace Inkscape::Colors::Space {

/**
 * Convert a color from the the OkHsl colorspace to the OkLab colorspace.
 *
 * @param in_out[in,out] The OkHsl color converted to an OkLab color.
 */
void OkHsl::toOkLab(std::vector<double> &in_out)
{
    double l = std::clamp(in_out[2], 0.0, 1.0);

    // Get max chroma for this hue and lightness and compute the absolute chroma.
    double const chromax = OkLch::max_chroma(l, in_out[0] * 360.0);
    double const absolute_chroma = in_out[1] * chromax;

    // Convert hue and chroma to the Cartesian a, b coordinates.
    Geom::sincos(in_out[0] * 2.0 * M_PI, in_out[2], in_out[1]);
    in_out[0] = l;
    in_out[1] *= absolute_chroma;
    in_out[2] *= absolute_chroma;
}

/**
 * Convert a color from the the OkLab colorspace to the OkHsl colorspace.
 *
 * @param in_out[in,out] The OkLab color converted to an OkHsl color.
 */
void OkHsl::fromOkLab(std::vector<double> &in_out)
{
    // Compute the chroma.
    double const absolute_chroma = std::hypot(in_out[1], in_out[2]);
    if (absolute_chroma < 1e-7) {
        // It would be numerically unstable to calculate the hue for this
        // color, so we set the hue and saturation to zero (grayscale color).
        in_out[2] = in_out[0];
        in_out[1] = 0.0;
        in_out[0] = 0.0;
    }

    // Compute the hue (in the unit interval).
    Geom::Angle const hue_angle = std::atan2(in_out[2], in_out[1]);
    in_out[2] = std::clamp(in_out[0], 0.0, 1.0);
    in_out[0] = hue_angle.radians0() / (2.0 * M_PI);

    // Compute the linear saturation.
    double const hue_degrees = Geom::deg_from_rad(hue_angle.radians0());
    double const chromax = OkLch::max_chroma(in_out[2], hue_degrees);
    in_out[1] = (chromax == 0.0) ? 0.0 : std::clamp(absolute_chroma / chromax, 0.0, 1.0);
}

}; // namespace Inkscape::Colors::Space
