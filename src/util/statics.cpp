// SPDX-License-Identifier: GPL-2.0-or-later
#include "statics.h"

#include <cassert>

namespace Inkscape::Util {

StaticsBin &StaticsBin::get()
{
    static StaticsBin instance;
    return instance;
}

void StaticsBin::destroy()
{
    for (auto n = head; n; n = n->next) {
        n->destroy();
    }
}

StaticsBin::~StaticsBin()
{
    for (auto n = head; n; n = n->next) {
        // If this assertion triggers, then destroy() wasn't called close enough to the end of main().
        assert(!n->active() && "StaticsBin::destroy() must be called before main() exit");
    }
}

StaticHolderBase::StaticHolderBase()
    : StaticHolderBase(StaticsBin::get())
{}

StaticHolderBase::StaticHolderBase(StaticsBin &bin)
    : next{bin.head}
{
    bin.head = this;
}

} // namespace Inkscape::Util
