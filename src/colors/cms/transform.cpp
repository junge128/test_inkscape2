// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * A C++ wrapper for lcms2 transforms
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "transform.h"

#include <boost/range/adaptor/reversed.hpp>
#include <cairo.h>
#include <numeric>
#include <string>

#include "colors/color.h"
#include "profile.h"

namespace Inkscape::Colors::CMS {

/**
 * Construct a color transform object from the lcms2 object.
 */
std::shared_ptr<Transform> const Transform::create(cmsHTRANSFORM handle, bool global)
{
    return handle ? std::make_shared<Transform>(handle, global) : nullptr;
}

/**
 * Construct a transformation suitable for display conversion in a cairo buffer
 *
 * @arg from  - The RGB CMS Profile the cairo data will start in.
 * @arg to    - The target RGB CMS Profile the cairo data needs to end up in.
 * @arg proof - A profile to apply a proofing step to, this can be CMYK for example.
 */
std::shared_ptr<Transform> const Transform::create_for_cairo(std::shared_ptr<Profile> const &from,
                                                             std::shared_ptr<Profile> const &to,
                                                             std::shared_ptr<Profile> const &proof,
                                                             RenderingIntent proof_intent, bool with_gamut_warn)
{
    if (!to || !from)
        return nullptr;

    auto cms_context = cmsCreateContext(nullptr, nullptr);

    if (proof) {
        unsigned int flags = cmsFLAGS_SOFTPROOFING | (with_gamut_warn ? cmsFLAGS_GAMUTCHECK : 0);
        unsigned int lt = lcms_intent(proof_intent, flags);

        return create(cmsCreateProofingTransformTHR(cms_context, from->getHandle(), TYPE_BGRA_8, to->getHandle(),
                                                    TYPE_BGRA_8, proof->getHandle(), INTENT_PERCEPTUAL, lt, flags));
    }
    return create(cmsCreateTransformTHR(cms_context, from->getHandle(), TYPE_BGRA_8, to->getHandle(), TYPE_BGRA_8,
                                        INTENT_PERCEPTUAL, 0));
}

/**
 * Construct a transformation suitable for Space::CMS transformations using the given rendering intent
 *
 * @arg from - The CMS Profile the color data will start in
 * @arg to - The target CMS Profile the color data needs to end up in.
 * @arg intent - The rendering intent to use when changing the gamut and white balance.
 */
std::shared_ptr<Transform> const Transform::create_for_cms(std::shared_ptr<Profile> const &from,
                                                           std::shared_ptr<Profile> const &to, RenderingIntent intent)
{
    if (!to || !from)
        return nullptr;
    unsigned int flags = 0;
    unsigned int lt = lcms_intent(intent, flags);

    // Format is 16bit integer in whatever color space it's in.
    auto from_format = cmsFormatterForColorspaceOfProfile(from->getHandle(), 2, false);
    auto to_format = cmsFormatterForColorspaceOfProfile(to->getHandle(), 2, false);
    return create(cmsCreateTransform(from->getHandle(), from_format, to->getHandle(), to_format, lt, flags));
}

/**
 * Construct a transformation suitable for gamut checking Space::CMS colors.
 *
 * @arg from - The CMS Profile the color data will start in
 * @arg to - The target CMS Profile the color data needs to end up in.
 */
std::shared_ptr<Transform> const Transform::create_for_cms_checker(std::shared_ptr<Profile> const &from,
                                                                   std::shared_ptr<Profile> const &to)
{
    if (!to || !from)
        return nullptr;

    static cmsContext check_context = nullptr;
    if (!check_context) {
        // Create an lcms context just for checking out of gamut colors, this can live as long as inkscape.
        check_context = cmsCreateContext(nullptr, nullptr);
        cmsUInt16Number alarmCodes[cmsMAXCHANNELS] = {0, 0, 0, 0, 0};
        cmsSetAlarmCodesTHR(check_context, alarmCodes);
    }
    // Format is 16bit integer in whatever color space it's in.
    auto from_format = cmsFormatterForColorspaceOfProfile(from->getHandle(), 2, false);
    return create(cmsCreateProofingTransformTHR(check_context, from->getHandle(), from_format,
                                                from->getHandle(), from_format, to->getHandle(),
                                                INTENT_RELATIVE_COLORIMETRIC, INTENT_RELATIVE_COLORIMETRIC,
                                                cmsFLAGS_GAMUTCHECK | cmsFLAGS_SOFTPROOFING),
                  true);
}

/**
 * Set the gamut alarm code for this cms transform (and only this one).
 *
 * NOTE: If the transform doesn't have a context because it was created for cms color
 * transforms instead of cairo transforms, this won't do anything.
 *
 * @arg input - The values per channel in the _output_ to use. For example if the transform
 *              is RGB to CMYK, the input vector should be four channels in size.
 *
 */
void Transform::set_gamut_warn(std::vector<double> const &input)
{
    std::vector<cmsUInt16Number> color(_channels_in);
    for (unsigned int i = 0; i < _channels_in; i++) {
        // double to uint16 conversion
        color[i] = input.size() > i ? input[i] * 65535 : 0;
    }
    if (_context) {
        cmsSetAlarmCodesTHR(_context, &color.front());
    }
}

/**
 * Wrap lcms2 cmsDoTransform to transform the pixel buffer's color channels.
 *
 * @arg inBuf - The input pixel buffer to transform.
 * @arg outBug - The output pixel buffer, which can be the same as inBuf.
 * @arg size - The size of the buffer to transform.
 */
void Transform::do_transform(unsigned char *inBuf, unsigned char *outBuf, unsigned size) const
{
    if ((cmsGetTransformInputFormat(_handle) & TYPE_BGRA_8) != TYPE_BGRA_8 ||
        (cmsGetTransformOutputFormat(_handle) & TYPE_BGRA_8) != TYPE_BGRA_8) {
        throw ColorError("Using a color-channel transform object to do a cairo transform operation!");
    }

    cmsDoTransform(_handle, inBuf, outBuf, size);
}

/**
 * Apply the CMS transform to the cairo surface and paint it into the output surface.
 *
 * @arg in - The source cairo surface with the pixels to transform.
 * @arg out - The destination cairo surface which may be the same as in.
 */
void Transform::do_transform(cairo_surface_t *in, cairo_surface_t *out) const
{
    cairo_surface_flush(in);

    auto px_in = cairo_image_surface_get_data(in);
    auto px_out = cairo_image_surface_get_data(out);

    int stride = cairo_image_surface_get_stride(in);
    int width = cairo_image_surface_get_width(in);
    int height = cairo_image_surface_get_height(in);

    if (stride != cairo_image_surface_get_stride(out) || width != cairo_image_surface_get_width(out) ||
        height != cairo_image_surface_get_height(out)) {
        throw ColorError("Different image formats while applying CMS!");
    }

    for (int i = 0; i < height; i++) {
        auto row_in = px_in + i * stride;
        auto row_out = px_out + i * stride;
        do_transform(row_in, row_out, width);
    }

    cairo_surface_mark_dirty(out);
}

/**
 * Apply the CMS transform to the cairomm surface and paint it into the output surface.
 *
 * @arg in - The source cairomm surface with the pixels to transform.
 * @arg out - The destination cairomm surface which may be the same as in.
 */
void Transform::do_transform(Cairo::RefPtr<Cairo::ImageSurface> &in, Cairo::RefPtr<Cairo::ImageSurface> &out) const
{
    do_transform(in->cobj(), out->cobj());
}

/**
 * Apply the CMS transform to a single Color object's data.
 *
 * @arg io - The input/output color as a vector of numbers between 0.0 and 1.0
 *
 * @returns the modified color in io
 */
bool Transform::do_transform(std::vector<double> &io) const
{
    std::vector<cmsUInt16Number> input(_channels_in);
    std::vector<cmsUInt16Number> output(_channels_out);

    for (unsigned int a = 0; a < _channels_in; a++) {
        // double to uint16 conversion
        input[a] = io[0] * 65535;
        io.erase(io.begin());
    }
    cmsDoTransform(_handle, &input.front(), &output.front(), 1);

    // Preserve any extra non-color channels (i.e. transparency) by inserting
    // into the front of the vector instead of the end.
    for (auto &val : boost::adaptors::reverse(output)) {
        io.insert(io.begin(), val / 65535.0);
    }
    return true;
}

/**
 * Return true if the input color is outside of the gamut if it was transformed using
 * this cms transform.
 *
 * @arg input - The input color as a vector of numbers between 0.0 and 1.0
 */
bool Transform::check_gamut(std::vector<double> const &input) const
{
    cmsUInt16Number in[cmsMAXCHANNELS];
    cmsUInt16Number out[cmsMAXCHANNELS];
    for (unsigned int i = 0; i < cmsMAXCHANNELS; i++) {
        in[i] = input.size() > i ? input[i] * 65535 : 0;
        out[i] = 0;
    }
    cmsDoTransform(_handle, &in, &out, 1);

    // All channels are set to zero in the alarm context, so the result is zero if out of gamut.
    return std::accumulate(std::begin(out), std::end(out), 0, std::plus<int>()) == 0;
}

/**
 * Get the lcms2 intent enum from the inkscape intent enum
 *
 * @args intent - The Inkscape RenderingIntent enum
 *
 * @returns flags - Any flags modifications for the given intent
 * @returns lcms intent enum, default is INTENT_PERCEPTUAL
 */
unsigned int Transform::lcms_intent(RenderingIntent intent, unsigned int &flags)
{
    switch (intent) {
        case RenderingIntent::RELATIVE_COLORIMETRIC:
            // Black point compensation only matters to relative colorimetric
            flags |= cmsFLAGS_BLACKPOINTCOMPENSATION;
        case RenderingIntent::RELATIVE_COLORIMETRIC_NOBPC:
            return INTENT_RELATIVE_COLORIMETRIC;
        case RenderingIntent::SATURATION:
            return INTENT_SATURATION;
        case RenderingIntent::ABSOLUTE_COLORIMETRIC:
            return INTENT_ABSOLUTE_COLORIMETRIC;
        case RenderingIntent::PERCEPTUAL:
        case RenderingIntent::UNKNOWN:
        case RenderingIntent::AUTO:
        default:
            return INTENT_PERCEPTUAL;
    }
}

} // namespace Inkscape::Colors::CMS

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
