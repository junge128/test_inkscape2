# SPDX-License-Identifier: GPL-2.0-or-later
set(libgc_SRC
	gc.cpp

	# -------
	# Headers
	gc-alloc.h
	gc-core.h
	gc-managed.h
)

add_inkscape_lib(gc_LIB "${libgc_SRC}")
# add_inkscape_source("${libgc_SRC}")
