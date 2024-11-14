// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * Copyright (C) 2000 Lauris Kaplinski
 * Copyright (C) 2000-2001 Ximian, Inc.
 * Copyright (C) 2002 Lauris Kaplinski
 * Copyright (C) 2008 Johan Engelen
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_DISPLAY_CURVE_H
#define SEEN_DISPLAY_CURVE_H

#include <2geom/pathvector.h>
#include <cstddef>
#include <optional>
#include <vector>
#include <utility>

/**
 * Wrapper around a Geom::PathVector object.
 */
class SPCurve {
public:
    explicit SPCurve() = default;
    explicit SPCurve(Geom::PathVector pathv) : _pathv(std::move(pathv)) {}
    explicit SPCurve(Geom::Rect const &rect, bool all_four_sides = false);

    void set_pathvector(Geom::PathVector const &new_pathv);
    Geom::PathVector const &get_pathvector() const;

    size_t get_segment_count() const;
    size_t nodes_in_path() const;

    bool is_empty() const;
    bool is_unset() const;
    bool is_closed() const;
    bool is_equal(SPCurve const *other) const;
    bool is_similar(SPCurve const *other, double precision = 0.001) const;
    Geom::Curve const *last_segment() const;
    Geom::Path const *last_path() const;
    Geom::Curve const *first_segment() const;
    Geom::Path const *first_path() const;
    std::optional<Geom::Point> first_point() const;
    std::optional<Geom::Point> last_point() const;
    std::optional<Geom::Point> second_point() const;
    std::optional<Geom::Point> penultimate_point() const;

    void reset();

    void moveto(Geom::Point const &p);
    void moveto(double x, double y);
    void lineto(Geom::Point const &p);
    void lineto(double x, double y);
    void quadto(Geom::Point const &p1, Geom::Point const &p2);
    void quadto(double x1, double y1, double x2, double y2);
    void curveto(Geom::Point const &p0, Geom::Point const &p1, Geom::Point const &p2);
    void curveto(double x0, double y0, double x1, double y1, double x2, double y2);
    void closepath();
    void closepath_current();
    void backspace();

    void transform(Geom::Affine const &m);
    SPCurve transformed(Geom::Affine const &m) const;
    void stretch_endpoints(Geom::Point const &, Geom::Point const &);
    void move_endpoints(Geom::Point const &, Geom::Point const &);
    void last_point_additive_move(Geom::Point const &p);

    void append(Geom::PathVector const &, bool use_lineto = false);
    void append(SPCurve const &curve2, bool use_lineto = false);
    bool append_continuous(SPCurve const &c1, double tolerance = 0.0625);

    void reverse();
    SPCurve reversed() const;

    std::vector<SPCurve> split() const;
    std::vector<SPCurve> split_non_overlapping() const;

    template <typename T>
    static std::optional<SPCurve> ptr_to_opt(T const &p)
    {
        if (p) {
            return *p;
        } else {
            return {};
        }
    }

protected:
    Geom::PathVector _pathv;
};

#endif // !SEEN_DISPLAY_CURVE_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8 :
