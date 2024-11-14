// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2023 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_COLORS_SPACES_XYZ_H
#define SEEN_COLORS_SPACES_XYZ_H

#include "linear-rgb.h"

namespace Inkscape::Colors::Space {

// CIE standard illuminant D65, Observer= 2° [0.9504, 1.0000, 1.0888].
// Simulates noon daylight with correlated color temperature of 6504 K.
inline const std::vector<double> illuminant_d65 = {0.9504, 1.0000, 1.0888};

/* for sRGB, reference white D65 */
inline const std::vector<double> d65[3] = {{3.24096994190452134377, -1.53738317757009345794, -0.49861076029300328366},
                                           {-0.96924363628087982613, 1.87596750150772066772, 0.04155505740717561247},
                                           {0.05563007969699360846, -0.20397695888897656435, 1.05697151424287856072}};

inline const std::vector<double> d65_inv[3] = {{0.41239079926595949381, 0.35758433938387799725, 0.18048078840183429261},
                                               {0.21263900587151036595, 0.71516867876775596569, 0.07219231536073371975},
                                               {0.019330818715591851469, 0.1191947797946259924, 0.9505321522496605464}};

class XYZ : public RGB
{
public:
    XYZ() = default;
    ~XYZ() override = default;

    Type getType() const override { return Type::XYZ; }
    std::string const getName() const override { return "XYZ"; }
    std::string const getIcon() const override { return "color-selector-xyz"; }

protected:
    friend class Inkscape::Colors::Color;

    void spaceToProfile(std::vector<double> &output) const override
    {
        XYZ::toLinearRGB(output);
        LinearRGB::toRGB(output);
    }
    void profileToSpace(std::vector<double> &output) const override
    {
        LinearRGB::fromRGB(output);
        XYZ::fromLinearRGB(output);
    }

    std::string toString(std::vector<double> const &values, bool opacity = true) const override;

public:
    static void toLinearRGB(std::vector<double> &output);
    static void fromLinearRGB(std::vector<double> &output);
};

} // namespace Inkscape::Colors::Space

#endif // SEEN_COLORS_SPACES_XYZ_H
