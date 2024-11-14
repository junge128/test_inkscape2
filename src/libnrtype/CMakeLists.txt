# SPDX-License-Identifier: GPL-2.0-or-later

set(nrtype_SRC
	font-factory.cpp
	font-instance.cpp
	font-lister.cpp
	Layout-TNG.cpp
	Layout-TNG-Compute.cpp
	Layout-TNG-Input.cpp
	Layout-TNG-OutIter.cpp
	Layout-TNG-Output.cpp
	Layout-TNG-Scanline-Makers.cpp
	OpenTypeUtil.cpp
	style-attachments.cpp

	# -------
	# Headers
	font-factory.h
	font-glyph.h
	font-instance.h
	font-lister.h
	Layout-TNG-Scanline-Maker.h
	Layout-TNG.h
	OpenTypeUtil.h
	style-attachments.h
)

add_inkscape_source("${nrtype_SRC}")
