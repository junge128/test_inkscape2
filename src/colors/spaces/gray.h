// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2023 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_COLORS_SPACES_GRAY_H
#define SEEN_COLORS_SPACES_GRAY_H

#include "rgb.h"

namespace Inkscape::Colors::Space {

class Gray : public RGB
{
    Type getType() const override { return Type::Gray; }
    std::string const getName() const override { return "Gray"; }
    std::string const getIcon() const override { return "color-selector-gray"; }
    unsigned int getComponentCount() const override { return 1; }

protected:
    friend class Inkscape::Colors::Color;

    void spaceToProfile(std::vector<double> &output) const override;
    void profileToSpace(std::vector<double> &output) const override;
};

} // namespace Inkscape::Colors::Space

#endif // SEEN_COLORS_SPACES_GRAY_H
