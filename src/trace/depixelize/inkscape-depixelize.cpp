// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * This is the C++ glue between Inkscape and Potrace
 *
 * Authors:
 *   Bob Jamison <rjamison@titan.com>
 *   Stéphane Gimenez <dev@gim.name>
 *
 * Copyright (C) 2004-2006 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 *
 * Potrace, the wonderful tracer located at http://potrace.sourceforge.net,
 * is provided by the generosity of Peter Selinger, to whom we are grateful.
 *
 */
#include <iomanip>
#include <thread>
#include <glibmm/i18n.h>

#include "inkscape-depixelize.h"

#include "colors/utils.h"
#include "preferences.h"
#include "async/progress.h"
#include "svg/css-ostringstream.h"

namespace Inkscape {
namespace Trace {
namespace Depixelize {

DepixelizeTracingEngine::DepixelizeTracingEngine(TraceType traceType, double curves, int islands, int sparsePixels, double sparseMultiplier, bool optimize)
    : traceType(traceType)
{
    params.curvesMultiplier = curves;
    params.islandsWeight = islands;
    params.sparsePixelsRadius = sparsePixels;
    params.sparsePixelsMultiplier = sparseMultiplier;
    params.optimize = optimize;
    params.nthreads = Inkscape::Preferences::get()->getIntLimited("/options/threading/numthreads", std::thread::hardware_concurrency(), 1, 256);
}

TraceResult DepixelizeTracingEngine::trace(Glib::RefPtr<Gdk::Pixbuf> const &pixbuf, Async::Progress<double> &progress)
{
    TraceResult res;

    ::Tracer::Splines splines;

    if (traceType == TraceType::VORONOI) {
        splines = ::Tracer::Kopf2011::to_voronoi(pixbuf, params);
    } else {
        splines = ::Tracer::Kopf2011::to_splines(pixbuf, params);
    }

    progress.report_or_throw(0.5);

    auto subprogress = Async::SubProgress(progress, 0.5, 0.5);
    auto throttled = Async::ProgressStepThrottler(subprogress, 0.02);

    int num_splines = std::distance(splines.begin(), splines.end());
    int i = 0;

    for (auto &it : splines) {
        throttled.report_or_throw((double)i / num_splines);
        i++;

        auto hex = Inkscape::Colors::rgba_to_hex(
                           SP_RGBA32_U_COMPOSE(unsigned(it.rgba[0]),
                                               unsigned(it.rgba[1]),
                                               unsigned(it.rgba[2]),
                                               unsigned(it.rgba[3])));
        Inkscape::CSSOStringStream ss;
        ss << "fill:" << hex.c_str() << ";fill-opacity:" << (it.rgba[3] / 255.0f) << ";";
        res.emplace_back(ss.str(), std::move(it.pathVector));
    }

    return res;
}

Glib::RefPtr<Gdk::Pixbuf> DepixelizeTracingEngine::preview(Glib::RefPtr<Gdk::Pixbuf> const &pixbuf)
{
    return pixbuf;
}

bool DepixelizeTracingEngine::check_image_size(Geom::IntPoint const &size) const
{
    return size.x() > 256 || size.y() > 256;
}

} // namespace Depixelize
} // namespace Trace
} // namespace Inkscape

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
