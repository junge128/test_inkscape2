// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2023 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "gray.h"

namespace Inkscape::Colors::Space {

/**
 * Convert a single gray channel into an RGB
 */
void Gray::spaceToProfile(std::vector<double> &io) const
{
    io.insert(io.begin(), io[0]);
    io.insert(io.begin(), io[0]);
}

/**
 * Convert an RGB into a gray channel using the HSL method
 */
void Gray::profileToSpace(std::vector<double> &io) const
{
    double max = std::max(std::max(io[0], io[1]), io[2]);
    double min = std::min(std::min(io[0], io[1]), io[2]);
    // Retain opacity if it exists by removing channels
    io.erase(io.begin());
    io.erase(io.begin());
    io[0] = (max + min) / 2.0; // Based on HSL conversion
}

}; // namespace Inkscape::Colors::Space
