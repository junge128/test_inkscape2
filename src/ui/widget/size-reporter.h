// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Author: PBS <pbs3141@gmail.com>
 * Copyright (C) 2024 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */


#ifndef SIZEREPORTER_H
#define SIZEREPORTER_H

#include <gtkmm/layoutmanager.h>

class SizeReporter : public Gtk::LayoutManager
{
public:
    sigc::signal<void ()> resized;

    static Glib::RefPtr<SizeReporter> create()
    {
        return Glib::make_refptr_for_instance(new SizeReporter());
    }

protected:
    SizeReporter() = default;

    void allocate_vfunc(Gtk::Widget const &widget, int width, int height, int baseline) override
    {
        resized.emit();
    }
};

#endif // SIZEREPORTER_H
