// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief A dialog which contains memory information and message logs.
 *
 * Authors: see git history
 *   Kaixo Gamorra
 *
 * Copyright (c) 2024 Kaixo Gamorra, Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "debug.h"

namespace Inkscape::UI::Dialog {

Debug::Debug()
    : DialogBase("/dialogs/debug", "DebugWindow")
{
    notebook.append_page(memory, "Memory");
    notebook.append_page(messages, "Messages");

    append(notebook);
};

Debug::~Debug() = default;

} // namespace Inkscape::UI::Dialog

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
