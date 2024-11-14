// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors: see git history
 *
 * Copyright (C) 2017 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_COLORS_CMS_TRANSFORM_H
#define SEEN_COLORS_CMS_TRANSFORM_H

#include <cairomm/surface.h>
#include <cassert>
#include <lcms2.h> // cmsHTRANSFORM
#include <memory>
#include <vector>

#include "colors/spaces/enum.h"

namespace Inkscape::Colors::CMS {

class Profile;
class Transform
{
public:
    static std::shared_ptr<Transform> const create(cmsHTRANSFORM handle, bool global = false);
    static std::shared_ptr<Transform> const create_for_cairo(std::shared_ptr<Profile> const &from,
                                                             std::shared_ptr<Profile> const &to,
                                                             std::shared_ptr<Profile> const &proof = nullptr,
                                                             RenderingIntent proof_intent = RenderingIntent::AUTO,
                                                             bool with_gamut_warn = false);
    static std::shared_ptr<Transform> const create_for_cms(std::shared_ptr<Profile> const &from,
                                                           std::shared_ptr<Profile> const &to, RenderingIntent intent);
    static std::shared_ptr<Transform> const create_for_cms_checker(std::shared_ptr<Profile> const &from,
                                                                   std::shared_ptr<Profile> const &to);

    Transform(cmsHTRANSFORM handle, bool global = false)
        : _handle(handle)
        , _context(!global ? cmsGetTransformContextID(handle) : nullptr)
        , _channels_in(T_CHANNELS(cmsGetTransformInputFormat(handle)))
        , _channels_out(T_CHANNELS(cmsGetTransformOutputFormat(handle)))
    {
        assert(_handle);
    }
    ~Transform()
    {
        cmsDeleteTransform(_handle);
        if (_context)
            cmsDeleteContext(_context);
    }
    Transform(Transform const &) = delete;
    Transform &operator=(Transform const &) = delete;

    cmsHTRANSFORM getHandle() const { return _handle; }

    void do_transform(unsigned char *inBuf, unsigned char *outBuf, unsigned size) const;
    void do_transform(cairo_surface_t *in, cairo_surface_t *out) const;
    void do_transform(Cairo::RefPtr<Cairo::ImageSurface> &in, Cairo::RefPtr<Cairo::ImageSurface> &out) const;
    bool do_transform(std::vector<double> &io) const;

    void set_gamut_warn(std::vector<double> const &input);
    bool check_gamut(std::vector<double> const &input) const;

private:
    cmsHTRANSFORM _handle;
    cmsContext _context;

    static unsigned int lcms_intent(RenderingIntent intent, unsigned int &flags);

public:
    unsigned int _channels_in;
    unsigned int _channels_out;
};

} // namespace Inkscape::Colors::CMS

#endif // SEEN_COLORS_CMS_TRANSFORM_H

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
