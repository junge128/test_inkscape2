// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2023 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_COLORS_SPACES_HSL_H
#define SEEN_COLORS_SPACES_HSL_H

#include "rgb.h"

namespace Inkscape::Colors::Space {

class HSL : public RGB
{
public:
    HSL() = default;
    ~HSL() override = default;

    Type getType() const override { return Type::HSL; }
    std::string const getName() const override { return "HSL"; }
    std::string const getIcon() const override { return "color-selector-hsx"; }

protected:
    friend class Inkscape::Colors::Color;

    std::string toString(std::vector<double> const &values, bool opacity = true) const override;

    void spaceToProfile(std::vector<double> &output) const override;
    void profileToSpace(std::vector<double> &output) const override;

public:
    class Parser : public HueParser
    {
    public:
        Parser(bool alpha)
            : HueParser("hsl", Type::HSL, alpha)
        {}
    };
};

} // namespace Inkscape::Colors::Space

#endif // SEEN_COLORS_SPACES_HSL_H
