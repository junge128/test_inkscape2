# SPDX-License-Identifier: GPL-2.0-or-later


set(path_SRC
  path-boolop.cpp
  path-object-set.cpp
  path-offset.cpp
  path-outline.cpp
  path-simplify.cpp
  path-util.cpp
  splinefit/bezier-fit.cpp
  splinefit/splinefit.c
  splinefit/splinefont.c
  splinefit/splinerefigure.c

  # -------
  # Headers
  path-boolop.h
  path-offset.h
  path-outline.h
  path-simplify.h
  path-util.h
  splinefit/splinefit.h
  splinefit/splinefont.h
)

add_inkscape_source("${path_SRC}")
