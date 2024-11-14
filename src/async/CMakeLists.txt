# SPDX-License-Identifier: GPL-2.0-or-later

set(async_SRC
	async.cpp
	async.h
	channel.h
	background-progress.h
	background-task.h
	operation-stream.h
	progress.h
	progress-splitter.h
)

add_inkscape_source("${async_SRC}")
