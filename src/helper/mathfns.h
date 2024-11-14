// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Mathematical/numerical functions.
 *
 * Authors:
 *   Johan Engelen <goejendaagh@zonnet.nl>
 *
 * Copyright (C) 2007 Johan Engelen
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_HELPER_MATHFNS_H
#define INKSCAPE_HELPER_MATHFNS_H

#include <cmath>
#include <2geom/point.h>

namespace Inkscape {
namespace Util {

/**
 * \return x rounded to the nearest multiple of c1 plus c0.
 *
 * \note
 * If c1==0 (and c0 is finite), then returns +/-inf.  This makes grid spacing of zero
 * mean "ignore the grid in this dimension".
 */
inline double round_to_nearest_multiple_plus(double x, double c1, double c0)
{
    return std::floor((x - c0) / c1 + 0.5) * c1 + c0;
}

/**
 * \return x rounded to the lower multiple of c1 plus c0.
 *
 * \note
 * If c1 == 0 (and c0 is finite), then returns +/-inf. This makes grid spacing of zero
 * mean "ignore the grid in this dimension".
 */
inline double round_to_lower_multiple_plus(double x, double c1, double c0 = 0.0)
{
    return std::floor((x - c0) / c1) * c1 + c0;
}

/**
 * \return x rounded to the upper multiple of c1 plus c0.
 *
 * \note
 * If c1 == 0 (and c0 is finite), then returns +/-inf. This makes grid spacing of zero
 * mean "ignore the grid in this dimension".
 */
inline double round_to_upper_multiple_plus(double x, double const c1, double const c0 = 0)
{
    return std::ceil((x - c0) / c1) * c1 + c0;
}

/// Returns floor(log_2(x)), assuming x >= 1.
// Note: This is a naive implementation.
// Todo: (C++20) Replace with std::bit_floor.
template <typename T>
int constexpr floorlog2(T x)
{
    int n = -1;
    while (x > 0) {
        x /= 2;
        n++;
    }
    return n;
}

/// Returns \a a mod \a b, always in the range 0..b-1, assuming b >= 1.
template <typename T, typename std::enable_if<std::is_integral<T>::value, bool>::type = true>
T constexpr safemod(T a, T b)
{
    a %= b;
    return a < 0 ? a + b : a;
}

/// Returns \a a rounded down to the nearest multiple of \a b, assuming b >= 1.
template <typename T, typename std::enable_if<std::is_integral<T>::value, bool>::type = true>
T constexpr round_down(T a, T b)
{
    return a - safemod(a, b);
}

/// Returns \a a rounded up to the nearest multiple of \a b, assuming b >= 1.
template <typename T, typename std::enable_if<std::is_integral<T>::value, bool>::type = true>
T constexpr round_up(T a, T b)
{
    return round_down(a - 1, b) + b;
}

/**
 * Just like std::clamp, except it doesn't deliberately crash if lo > hi due to rounding errors,
 * so is safe to use with floating-point types. (Note: compiles to branchless.)
 */
template <typename T>
T safeclamp(T val, T lo, T hi)
{
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

} // namespace Util
} // namespace Inkscape

#endif // INKSCAPE_HELPER_MATHFNS_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
