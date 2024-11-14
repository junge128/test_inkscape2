// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Manager - Look after all a document's icc profiles.
 *
 * Copyright 2023 Martin Owens <doctormo@geek-2.com>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "manager.h"

#include <algorithm>
#include <glib.h>

#include "colors/color.h"
#include "colors/parser.h"
#include "document.h"

// Each Internal space should be imported here.
#include "spaces/cms.h"
#include "spaces/cmyk.h"
#include "spaces/gray.h"
#include "spaces/hsl.h"
#include "spaces/hsluv.h"
#include "spaces/hsv.h"
#include "spaces/lab.h"
#include "spaces/lch.h"
#include "spaces/linear-rgb.h"
#include "spaces/luv.h"
#include "spaces/named.h"
#include "spaces/okhsl.h"
#include "spaces/oklab.h"
#include "spaces/oklch.h"
#include "spaces/rgb.h"
#include "spaces/xyz.h"
#include "util/reference.h"

namespace Inkscape::Colors {

Manager::Manager()
{
    // Regular SVG 1.1 Colors
    addSpace(new Space::RGB());
    addSpace(new Space::NamedColor());

    // Color module 4 and 5 support
    addSpace(new Space::DeviceCMYK());
    addSpace(new Space::Gray());
    addSpace(new Space::HSL());
    addSpace(new Space::HSLuv());
    addSpace(new Space::HSV());
    addSpace(new Space::Lab());
    addSpace(new Space::LinearRGB());
    addSpace(new Space::Lch());
    addSpace(new Space::Luv());
    addSpace(new Space::OkHsl());
    addSpace(new Space::OkLab());
    addSpace(new Space::OkLch());
    addSpace(new Space::XYZ());
}

/**
 * Add the given space and assume ownership over it.
 */
std::shared_ptr<Space::AnySpace> Manager::addSpace(Space::AnySpace *space)
{
    if (find(space->getType())) {
        throw ColorError("Can not add the same color space twice.");
    }
    _spaces.emplace_back(space);
    return _spaces.back();
}

/**
 * Removes the given space from the list of available spaces.
 */
bool Manager::removeSpace(std::shared_ptr<Space::AnySpace> space)
{
    return std::erase(_spaces, space);
}

std::vector<std::shared_ptr<Space::AnySpace>> Manager::spaces(Space::Traits traits) {
    std::vector<std::shared_ptr<Space::AnySpace>> out;
    std::copy_if(_spaces.begin(), _spaces.end(), std::back_inserter(out), [=](auto& p) {
        return (p->getComponents().traits() & traits) != Space::Traits::None;
    });
    return out;
}

/**
 * Finds the first global color space matching the given type
 *
 * @arg type - The type enum to match
 */
std::shared_ptr<Space::AnySpace> Manager::find(Space::Type type) const
{
    auto it = std::find_if(_spaces.begin(), _spaces.end(), [type](auto &v) { return v->getType() == type; });
    return it != _spaces.end() ? *it : nullptr;
}

/**
 * Finds the global space matching the given name
 *
 * @arg name - The name of the space as given by getName
 */
std::shared_ptr<Space::AnySpace> Manager::find(std::string const &name) const
{
    auto it = std::find_if(_spaces.begin(), _spaces.end(), [name](auto &v) { return v->getName() == name; });
    return it != _spaces.end() ? *it : nullptr;
}
} // namespace Inkscape::Colors

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
