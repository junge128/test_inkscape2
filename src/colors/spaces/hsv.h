// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2023 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_COLORS_SPACES_HSV_H
#define SEEN_COLORS_SPACES_HSV_H

#include "rgb.h"

namespace Inkscape::Colors::Space {

class HSV : public RGB
{
public:
    HSV() = default;
    ~HSV() override = default;

    Type getType() const override { return Type::HSV; }
    std::string const getName() const override { return "HSV"; }
    std::string const getIcon() const override { return "color-selector-hsx"; }

    void spaceToProfile(std::vector<double> &output) const override;
    void profileToSpace(std::vector<double> &output) const override;

protected:
    friend class Inkscape::Colors::Color;

    std::string toString(std::vector<double> const &values, bool opacity) const override;

public:
    class fromHwbParser : public HueParser
    {
    public:
        fromHwbParser(bool alpha)
            : HueParser("hwb", Space::Type::HSV, alpha)
        {}
        bool parse(std::istringstream &input, std::vector<double> &output) const override;
    };
};

} // namespace Inkscape::Colors::Space

#endif // SEEN_COLORS_SPACES_HSV_H
