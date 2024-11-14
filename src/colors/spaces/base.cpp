// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Manage color spaces
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2023 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "base.h"

#include <sstream>

#include "colors/cms/profile.h"
#include "colors/cms/transform.h"
#include "colors/color.h"
#include "components.h"

namespace Inkscape::Colors::Space {

AnySpace::AnySpace()
{
    if (!srgb_profile) {
        srgb_profile = Colors::CMS::Profile::create_srgb();
    }
}

/**
 * Return true if the given data would be valid for this color space.
 */
bool AnySpace::isValidData(std::vector<double> const &values) const
{
    auto const n_values = values.size();
    auto const n_space = getComponentCount();
    return n_values == n_space || n_values == n_space + 1;
}

/**
 * In place conversion of a color object to the given space.
 *
 * This three part conversion may not mutate the input at all, depending on
 * the space it's already in and the format of the data.
 */
bool AnySpace::convert(std::vector<double> &io, std::shared_ptr<AnySpace> to_space) const
{
    // Firstly convert from the formatted values (i.e. hsl) into the profile values (i.e. sRGB)
    spaceToProfile(io);
    // Secondly convert the color profile itself using lcms2 if the profiles are different
    if (profileToProfile(io, to_space)) {
        // Thirdly convert to the formatted values (i.e. hsl) from the profile values (i.e. sRGB)
        to_space->profileToSpace(io);
        return true;
    }
    profileToSpace(io);
    return false;
}

/**
 * Convert from the space's format, to the profile's data format.
 */
void AnySpace::spaceToProfile(std::vector<double> &io) const {}

/**
 * Convert from the profile's format, to the space's data format.
 */
void AnySpace::profileToSpace(std::vector<double> &io) const {}

/**
 * Step two in coverting a color, convert it's profile to another profile (if needed)
 */
bool AnySpace::profileToProfile(std::vector<double> &io, std::shared_ptr<AnySpace> to_space) const
{
    auto from_profile = getProfile();
    auto to_profile = to_space->getProfile();
    if (*to_profile == *from_profile)
        return true;

    // Choose best rendering intent, first ours, then theirs, finally a default
    auto intent = getIntent();
    if (intent == RenderingIntent::UNKNOWN)
        intent = to_space->getIntent();
    if (intent == RenderingIntent::UNKNOWN)
        intent = RenderingIntent::PERCEPTUAL;

    // Look in the transform cache for the color profile
    auto to_profile_id = to_profile->getChecksum() + intentIds[intent];

    if (!_transforms.contains(to_profile_id)) {
        // Create a new transform for this one way profile-pair
        _transforms.emplace(to_profile_id, Colors::CMS::Transform::create_for_cms(from_profile, to_profile, intent));
    }

    // Use the transform to convert the output colors.
    if (auto &tr = _transforms[to_profile_id]) {
        return tr->do_transform(io);
    }
    return false;
}

/**
 * Return true if the color would be out of gamut in the target color space.
 *
 * NOTE: This can NOT work if the base color spaces are exactly the same. i.e. device-cmyk(sRGB)
 * will always return false despite not being reversable with RGB (which is also sRGB).
 *
 * If you want gamut checking via lcms2, you must use different icc profiles.
 *
 * @arg input - Channel values in this space.
 * @arg to_space - The target space to compare against.
 */
bool AnySpace::outOfGamut(std::vector<double> const &input, std::shared_ptr<AnySpace> to_space) const
{
    auto from_profile = getProfile();
    auto to_profile = to_space->getProfile();
    if (*to_profile == *from_profile)
        return false;
    // 1. Look in the checker cache for the color profile
    auto to_profile_id = to_profile->getId();
    if (_gamut_checkers.find(to_profile_id) == _gamut_checkers.end()) {
        // 2. Create a new transform for this one way profile-pair
        _gamut_checkers.emplace(to_profile_id,
                                Colors::CMS::Transform::create_for_cms_checker(from_profile, to_profile));
    }

    return _gamut_checkers[to_profile_id]->check_gamut(input);
}

/**
 * Return a list of Component objects, in order of the channels in this color space
 *
 * @arg alpha - If true, returns the alpha component as well
 */
Components const &AnySpace::getComponents(bool alpha) const
{
    return Space::Components::get(getComponentType(), alpha);
}

}; // namespace Inkscape::Colors::Space
