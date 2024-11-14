// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2023 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_COLORS_SPACES_OKLAB_H
#define SEEN_COLORS_SPACES_OKLAB_H

#include "linear-rgb.h"

namespace Inkscape::Colors::Space {

class OkLab : public RGB
{
public:
    OkLab() = default;
    ~OkLab() override = default;

    Type getType() const override { return Type::OKLAB; }
    std::string const getName() const override { return "OkLab"; }
    std::string const getIcon() const override { return "color-selector-oklab"; }

protected:
    friend class Inkscape::Colors::Color;

    void spaceToProfile(std::vector<double> &output) const override
    {
        scaleUp(output);
        OkLab::toLinearRGB(output);
        LinearRGB::toRGB(output);
    }
    void profileToSpace(std::vector<double> &output) const override
    {
        LinearRGB::fromRGB(output);
        OkLab::fromLinearRGB(output);
        scaleDown(output);
    }

    std::string toString(std::vector<double> const &values, bool opacity) const override;

public:
    class Parser : public Colors::Parser
    {
    public:
        Parser()
            : Colors::Parser("oklab", Type::OKLAB)
        {}
        bool parse(std::istringstream &input, std::vector<double> &output) const override;
    };

    static void toLinearRGB(std::vector<double> &output);
    static void fromLinearRGB(std::vector<double> &output);

    static void scaleUp(std::vector<double> &in_out);
    static void scaleDown(std::vector<double> &in_out);
};

} // namespace Inkscape::Colors::Space

#endif // SEEN_COLORS_SPACES_OKLAB_H
