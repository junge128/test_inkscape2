// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2023 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_COLORS_SPACES_LAB_H
#define SEEN_COLORS_SPACES_LAB_H

#include "xyz.h"

namespace Inkscape::Colors::Space {

class Lab : public RGB
{
public:
    Lab() = default;
    ~Lab() override = default;

    Type getType() const override { return Type::LAB; }
    std::string const getName() const override { return "Lab"; }
    std::string const getIcon() const override { return "color-selector-lab"; }

protected:
    friend class Inkscape::Colors::Color;

    void spaceToProfile(std::vector<double> &output) const override
    {
        Lab::toXYZ(output);
        XYZ::toLinearRGB(output);
        LinearRGB::toRGB(output);
    }
    void profileToSpace(std::vector<double> &output) const override
    {
        LinearRGB::fromRGB(output);
        XYZ::fromLinearRGB(output);
        Lab::fromXYZ(output);
    }

    std::string toString(std::vector<double> const &values, bool opacity) const override;

public:
    class Parser : public Colors::Parser
    {
    public:
        Parser()
            : Colors::Parser("lab", Type::LAB)
        {}
        bool parse(std::istringstream &input, std::vector<double> &output) const override;
    };

    static void toXYZ(std::vector<double> &output);
    static void fromXYZ(std::vector<double> &output);

    static void scaleDown(std::vector<double> &in_out);
    static void scaleUp(std::vector<double> &in_out);
};

} // namespace Inkscape::Colors::Space

#endif // SEEN_COLORS_SPACES_LAB_H
