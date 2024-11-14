// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2023 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "cms.h"

#include <iomanip>

#include "colors/cms/profile.h"
#include "colors/cms/transform.h"
#include "colors/color.h"
#include "colors/manager.h"
#include "colors/printer.h"

namespace Inkscape::Colors::Space {

// When we support a color space that lcms2 does not, record here
static cmsUInt32Number customSigOKLabData = 0x4f4b4c42; // 'OKLB';

static std::map<cmsUInt32Number, Space::Type> _lcmssig_to_space = {
    {cmsSigRgbData, Space::Type::RGB},        {cmsSigHlsData, Space::Type::HSL},
    {cmsSigCmykData, Space::Type::CMYK},      {cmsSigCmyData, Space::Type::CMY},
    {cmsSigHsvData, Space::Type::HSV},        {cmsSigLuvData, Space::Type::HSLUV},
    {customSigOKLabData, Space::Type::OKLAB}, {cmsSigXYZData, Space::Type::XYZ},
    {cmsSigXYZData, Space::Type::YXY},        {cmsSigLabData, Space::Type::LAB},
    {cmsSigYCbCrData, Space::Type::YCbCr},    {cmsSigGrayData, Space::Type::Gray},
};

CMS::CMS(std::shared_ptr<Inkscape::Colors::CMS::Profile> profile)
    : AnySpace()
    , _profile_name(profile->getName(true))
    , _profile_size(profile->getSize())
    , _profile_type(_lcmssig_to_space[profile->getColorSpace()])
    , _profile(profile)
{}

/**
 * Naked CMS space for testing and data retention where the profile is unavailable.
 */
CMS::CMS(std::string profile_name, unsigned profile_size, Space::Type profile_type)
    : AnySpace()
    , _profile_name(std::move(profile_name))
    , _profile_size(profile_size)
    , _profile_type(profile_type)
    , _profile(nullptr)
{}

/**
 * Return the profile for this cms space. If this is anonymous, it returns srgb
 * so the transformation on the fallback color is transparent.
 */
std::shared_ptr<Colors::CMS::Profile> const CMS::getProfile() const
{
    if (!isValid())
        return srgb_profile;
    return _profile;
}

/**
 * If this space lacks a profile, it's really the sRGB fallback values,
 * so we strip out any other values from the io.
 */
void CMS::spaceToProfile(std::vector<double> &io) const
{
    if (isValid())
        return; // Do nothing for valid spaces

    bool has_opacity = io.size() == _profile_size + 4;

    // Keep deleting the icc color values
    while (io.size() > (3 + has_opacity)) {
        io.erase(io.begin() + 3);
    }
}

/**
 * Get the number of components for this cms color space.
 *
 * If the color space is not valid, three extra channels are
 * used to hold the red green and blue values.
 */
unsigned int CMS::getComponentCount() const
{
    return _profile ? _profile_size : _profile_size + 3;
}

/**
 * Parse a string stream into a vector of doubles which are always values in
 * this CMS space / icc profile.
 *
 * @args ss - String input which doesn't have to be at the start of the string.
 * @returns output - The vector to populate with the numbers found in the string.
 * @returns the name of the cms profile requested.
 */
std::string CMS::CmsParser::parseColor(std::istringstream &ss, std::vector<double> &output, bool &more) const
{
    std::string icc_name;
    ss >> icc_name;

    if (!icc_name.empty() && icc_name.back() == ',')
        icc_name.pop_back();

    bool end = false;
    while (!end && append_css_value(ss, output, end, ','))
        continue;

    if (output.size() == 0) {
        std::string named;
        ss >> named;
        if (!named.empty() && ss.get() == ')') {
            std::cerr << "Found SVG2 ICC named color '" << named.c_str() << "' for profile '" << icc_name.c_str()
                      << "', which not supported yet.\n";
        }
    }

    return icc_name;
}

/**
 * Output these values into this CMS space.
 *
 * @args values - The values for each channel in the icc profile.
 * @args opacity - Should opacity be included. This is ignored since cms
 *                 output is ALWAYS without opacity.
 *
 * @returns the string suitable for css and style use.
 */
std::string CMS::toString(std::vector<double> const &values, bool /*opacity*/) const
{
    if (values.size() < _profile_size)
        return "";

    // RGBA Hex fallback plus icc-color section
    auto oo = IccColorPrinter(_profile_size, _profile_name);

    // When an icc color was parsed, but there is no profile
    if (!isValid()) {
        // Not enough values for a fallback option (maybe corrupt?)
        if (values.size() < _profile_size + 3)
            return "";
        std::vector<double> remains(values.begin() + 3, values.end());
        oo << remains;
    } else {
        oo << values;
    }
    // opacity is never added to the printer here, always ignored
    return rgba_to_hex(toRGBA(values), false) + " " + (std::string)oo;
}

/**
 * Convert this CMS color into a simple sRGB based RGBA value.
 *
 * @arg values - The values for each channel in the icc profile
 * @arg opacity - An extra opacity to mix into the output.
 *
 * @returns An integer of the sRGB value.
 */
uint32_t CMS::toRGBA(std::vector<double> const &values, double opacity) const
{
    if (!isValid()) {
        if (values.size() == _profile_size + 3)
            return SP_RGBA32_F_COMPOSE(values[0], values[1], values[2], opacity);
        if (values.size() == _profile_size + 4)
            return SP_RGBA32_F_COMPOSE(values[0], values[1], values[2], opacity * values.back());
        std::cerr << "Can not convert CMS color to sRGB, no profile available and no fallback color\n";
        return 0x0;
    }

    static auto rgb = Manager::get().find(Space::Type::RGB);
    std::vector<double> copy = values;
    if (convert(copy, rgb)) {
        // The opacity will be copied during conversion (if present) so we only
        // need to test if the rgb has opacity and add it on if it does.
        if (copy.size() == rgb->getComponentCount() + 1)
            opacity *= copy.back();

        // CMS color channels never include opacity, it's not in the specification
        return SP_RGBA32_F_COMPOSE(copy[0], copy[1], copy[2], opacity);
    }
    std::cerr << "Can not convert CMS color to sRGB.\n";
    return 0x0;
}

/**
 * Return true if, using a rough hueruistic, this color could be considered to be using
 * too much ink if it was printed using the ink as specified.
 *
 * NOTE: This is only useful for CMYK profiles. Anything else will return false.
 *
 * @arg input - Channel values in this space.
 */
bool CMS::overInk(std::vector<double> const &input) const
{
    if (!input.size() || getType() != Type::CMYK)
        return false;

    /* Some literature states that when the sum of paint values exceed 320%, it is considered to be a satured color,
       which means the paper can get too wet due to an excessive amount of ink. This may lead to several issues
       such as misalignment and poor quality of printing in general.*/
    return input[0] + input[1] + input[2] + input[3] > 3.2;
}

}; // namespace Inkscape::Colors::Space
