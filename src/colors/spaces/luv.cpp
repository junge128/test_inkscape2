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

#include "luv.h"

#include <cmath>

namespace Inkscape::Colors::Space {

constexpr double REF_U = 0.19783000664283680764;
constexpr double REF_V = 0.46831999493879100370;

// There's no CSS for LUV yet, so we don't know what scales
// the w3c might choose to use. Our own calculations use these.
constexpr double LUMA_SCALE = 100;
constexpr double MIN_U = -100;
constexpr double MAX_U = 200;
constexpr double MIN_V = -200;
constexpr double MAX_V = 120;

/**
 * Changes the values from 0..1, to typical luv scaling used in calculations.
 */
void Luv::scaleUp(std::vector<double> &in_out)
{
    in_out[0] = SCALE_UP(in_out[0], 0, LUMA_SCALE);
    in_out[1] = SCALE_UP(in_out[1], MIN_U, MAX_U);
    in_out[2] = SCALE_UP(in_out[2], MIN_V, MAX_V);
}

/**
 * Changes the values from typical luv scaling (see above) to values 0..1.
 */
void Luv::scaleDown(std::vector<double> &in_out)
{
    in_out[0] = SCALE_DOWN(in_out[0], 0, LUMA_SCALE);
    in_out[1] = SCALE_DOWN(in_out[1], MIN_U, MAX_U);
    in_out[2] = SCALE_DOWN(in_out[2], MIN_V, MAX_V);
}

std::vector<double> Luv::fromCoordinates(std::vector<double> const &in)
{
    auto out = in;
    scaleDown(out);
    return out;
}

std::vector<double> Luv::toCoordinates(std::vector<double> const &in)
{
    auto out = in;
    scaleUp(out);
    return out;
}

/**
 * Utility function used to convert from the XYZ colorspace to CIELuv.
 * https://en.wikipedia.org/wiki/CIELUV
 *
 * @param y Y component of the XYZ color.
 * @return Luminance component of Luv color.
 */
static double y2l(double y)
{
    if (y <= EPSILON)
        return y * KAPPA;
    else
        return 116.0 * std::cbrt(y) - 16.0;
}

/**
 * Utility function used to convert from CIELuv colorspace to XYZ.
 *
 * @param l Luminance component of Luv color.
 * @return Y component of the XYZ color.
 */
static double l2y(double l)
{
    if (l <= 8.0) {
        return l / KAPPA;
    } else {
        double x = (l + 16.0) / 116.0;
        return (x * x * x);
    }
}

/**
 * Convert a color from the the Luv colorspace to the XYZ colorspace.
 *
 * @param in_out[in,out] The Luv color converted to a XYZ color.
 */
void Luv::toXYZ(std::vector<double> &in_out)
{
    if (in_out[0] <= 0.00000001) {
        /* Black would create a divide-by-zero error. */
        in_out[0] = 0.0;
        in_out[1] = 0.0;
        in_out[2] = 0.0;
        return;
    }

    double var_u = in_out[1] / (13.0 * in_out[0]) + REF_U;
    double var_v = in_out[2] / (13.0 * in_out[0]) + REF_V;
    double y = l2y(in_out[0]);
    double x = -(9.0 * y * var_u) / ((var_u - 4.0) * var_v - var_u * var_v);
    double z = (9.0 * y - (15.0 * var_v * y) - (var_v * x)) / (3.0 * var_v);

    in_out[0] = x;
    in_out[1] = y;
    in_out[2] = z;
}

/**
 * Convert a color from the the XYZ colorspace to the Luv colorspace.
 *
 * @param in_out[in,out] The XYZ color converted to a Luv color.
 */
void Luv::fromXYZ(std::vector<double> &in_out)
{
    double const denominator = in_out[0] + (15.0 * in_out[1]) + (3.0 * in_out[2]);
    double var_u = 4.0 * in_out[0] / denominator;
    double var_v = 9.0 * in_out[1] / denominator;
    double l = y2l(in_out[1]);
    double u = 13.0 * l * (var_u - REF_U);
    double v = 13.0 * l * (var_v - REF_V);

    in_out[0] = l;
    if (l < 0.00000001) {
        in_out[1] = 0.0;
        in_out[2] = 0.0;
    } else {
        in_out[1] = u;
        in_out[2] = v;
    }
}

}; // namespace Inkscape::Colors::Space
