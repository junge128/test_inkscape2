// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2023 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_COLORS_SPACES_HSLUV_H
#define SEEN_COLORS_SPACES_HSLUV_H

#include <2geom/line.h>
#include <2geom/ray.h>

#include "lch.h"

#include <2geom/line.h>
#include <2geom/ray.h>

namespace Inkscape::Colors::Space {

class HSLuv : public RGB
{
public:
    HSLuv() = default;
    ~HSLuv() override = default;

    Type getType() const override { return Type::HSLUV; }
    std::string const getName() const override { return "HSLuv"; }
    std::string const getIcon() const override { return "color-selector-hsluv"; }

protected:
    friend class Inkscape::Colors::Color;

    void spaceToProfile(std::vector<double> &output) const override
    {
        HSLuv::toLch(output);
        Lch::toLuv(output);
        Luv::toXYZ(output);
        XYZ::toLinearRGB(output);
        LinearRGB::toRGB(output);
    }
    void profileToSpace(std::vector<double> &output) const override
    {
        LinearRGB::fromRGB(output);
        XYZ::fromLinearRGB(output);
        Luv::fromXYZ(output);
        Lch::fromLuv(output);
        HSLuv::fromLch(output);
    }

public:
    static void toLch(std::vector<double> &output);
    static void fromLch(std::vector<double> &output);
    static std::array<Geom::Line, 6> get_bounds(double l);
};

} // namespace Inkscape::Colors::Space

#endif // SEEN_COLORS_SPACES_HSLUV_H
