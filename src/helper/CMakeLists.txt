# SPDX-License-Identifier: GPL-2.0-or-later

set(helper_SRC
	choose-file.cpp
	geom.cpp
	geom-nodetype.cpp
	geom-pathstroke.cpp
	geom-pathvector_nodesatellites.cpp
	geom-nodesatellite.cpp
	gettext.cpp
	pixbuf-ops.cpp
	png-write.cpp
	save-image.cpp
	stock-items.cpp
	#units-test.cpp

	# -------
	# Headers
	choose-file.h
	geom-curves.h
	geom-nodetype.h
	geom-pathstroke.h
	geom-pathvector_nodesatellites.h
	geom-nodesatellite.h
	geom.h
	gettext.h
	mathfns.h
	pixbuf-ops.h
	png-write.h
    save-image.h
	stock-items.h
)

# add_inkscape_lib(helper_LIB "${helper_SRC}")
add_inkscape_source("${helper_SRC}")
