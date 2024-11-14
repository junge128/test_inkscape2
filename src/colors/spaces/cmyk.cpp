// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * DeviceCMYK is NOT a color managed color space for ink values, for those
 * please see the CMS icc profile based color spaces.
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2023 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "cmyk.h"

#include "colors/color.h"
#include "colors/printer.h"

namespace Inkscape::Colors::Space {

/**
 * Convert the DeviceCMYK color into sRGB components used in the sRGB icc profile.
 *
 * See CSS Color Module Level 5, device-cmyk Uncalibrated conversion.
 *
 * @arg io - A vector of the input values, where the new values will be stored.
 */
void DeviceCMYK::spaceToProfile(std::vector<double> &io) const
{
    double white = 1.0 - io[3];
    io[0] = 1.0 - std::min(1.0, io[0] * white + io[3]);
    io[1] = 1.0 - std::min(1.0, io[1] * white + io[3]);
    io[2] = 1.0 - std::min(1.0, io[2] * white + io[3]);

    // Delete black from position 3
    io.erase(io.begin() + 3);
}

/**
 * Convert from sRGB icc values to DeviceCMYK values
 *
 * See CSS Color Module Level 5, device-cmyk Uncalibrated conversion.
 *
 * @arg io - A vector of the input values, where the new values will be stored.
 */
void DeviceCMYK::profileToSpace(std::vector<double> &io) const
{
    // Insert black channel at position 3
    io.insert(io.begin() + 3, 1.0 - std::max(std::max(io[0], io[1]), io[2]));
    double const white = 1.0 - io[3];

    // Each channel is it's color chart oposite (cyan->red) with a bit of white removed.
    io[0] = white ? (1.0 - io[0] - io[3]) / white : 0.0;
    io[1] = white ? (1.0 - io[1] - io[3]) / white : 0.0;
    io[2] = white ? (1.0 - io[2] - io[3]) / white : 0.0;
}

/**
 * Print the DeviceCMYK color to a CSS Color Module Level 5 string.
 *
 * @arg values - A vector of doubles for each channel in the DeviceCMYK space
 * @arg opacity - True if the opacity should be included in the output.
 */
std::string DeviceCMYK::toString(std::vector<double> const &values, bool opacity) const
{
    auto os = CssFuncPrinter(4, "device-cmyk");
    os << values;
    if (opacity && values.size() == 5)
        os << values[4];
    return os;
}

/**
 * Return true if, using a rough hueruistic, this color could be considered to be using
 * too much ink if it was printed using the ink as specified.
 *
 * @arg input - Channel values in this space.
 */
bool DeviceCMYK::overInk(std::vector<double> const &input) const
{
    if (!input.size())
        return false;
    // Over 320% is considered over inked, see cms.cpp for details.
    return input[0] + input[1] + input[2] + input[3] > 3.2;
}

}; // namespace Inkscape::Colors::Space
