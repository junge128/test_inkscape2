// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2023 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_COLORS_SPACES_LCH_H
#define SEEN_COLORS_SPACES_LCH_H

#include "luv.h"

namespace Inkscape::Colors::Space {

class Lch : public RGB
{
public:
    Lch() = default;
    ~Lch() override = default;

    Type getType() const override { return Type::LCH; }
    std::string const getName() const override { return "Lch"; }
    std::string const getIcon() const override { return "color-selector-lch"; }

protected:
    friend class Inkscape::Colors::Color;

    void spaceToProfile(std::vector<double> &output) const override
    {
        Lch::scaleUp(output);
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
        Lch::scaleDown(output);
    }

    std::string toString(std::vector<double> const &values, bool opacity) const override;

public:
    class Parser : public Colors::Parser
    {
    public:
        Parser()
            : Colors::Parser("lch", Type::LCH)
        {}
        bool parse(std::istringstream &input, std::vector<double> &output) const override;
    };

    static void toLuv(std::vector<double> &output);
    static void fromLuv(std::vector<double> &output);

    static void scaleDown(std::vector<double> &in_out);
    static void scaleUp(std::vector<double> &in_out);
};

} // namespace Inkscape::Colors::Space

#endif // SEEN_COLORS_SPACES_LCH_H
