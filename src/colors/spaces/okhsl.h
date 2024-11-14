// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2023 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_COLORS_SPACES_OKHSL_H
#define SEEN_COLORS_SPACES_OKHSL_H

#include "oklab.h"

namespace Inkscape::Colors::Space {

class OkHsl : public RGB
{
public:
    OkHsl() = default;
    ~OkHsl() override = default;

    Type getType() const override { return Type::OKHSL; }
    std::string const getName() const override { return "OkHsl"; }
    std::string const getIcon() const override { return "color-selector-okhsl"; }

protected:
    friend class Inkscape::Colors::Color;

    void spaceToProfile(std::vector<double> &output) const override
    {
        OkHsl::toOkLab(output);
        OkLab::toLinearRGB(output);
        LinearRGB::toRGB(output);
    }
    void profileToSpace(std::vector<double> &output) const override
    {
        LinearRGB::fromRGB(output);
        OkLab::fromLinearRGB(output);
        OkHsl::fromOkLab(output);
    }

public:
    static void toOkLab(std::vector<double> &output);
    static void fromOkLab(std::vector<double> &output);
};

} // namespace Inkscape::Colors::Space

#endif // SEEN_COLORS_SPACES_OKHSL_H
