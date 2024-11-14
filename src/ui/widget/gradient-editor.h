// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Gradient editor widget for "Fill and Stroke" dialog
 *
 * Author:
 *   Michael Kowalski
 *
 * Copyright (C) 2020-2021 Michael Kowalski
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_GRADIENT_EDITOR_H
#define SEEN_GRADIENT_EDITOR_H

#include <memory>
#include <optional>
#include <glibmm/refptr.h>
#include <gtkmm/box.h>
#include <gtkmm/treemodelcolumn.h>

#include "colors/color-set.h"
#include "object/sp-gradient.h"
#include "object/sp-stop.h"
#include "spin-scale.h"
#include "gradient-with-stops.h"
#include "gradient-selector-interface.h"
#include "ui/operation-blocker.h"

namespace Gtk {
class Adjustment;
class Builder;
class Button;
class Expander;
class Grid;
class Image;
class ListStore;
class SpinButton;
class ToggleButton;
class TreeRow;
class TreeView;
} // namespace Gtk

namespace Inkscape::UI::Widget {

class GradientSelector;
class PopoverMenu;

class GradientEditor final : public Gtk::Box, public GradientSelectorInterface {
public:
    GradientEditor(const char* prefs);
    ~GradientEditor() noexcept final;

private:
    sigc::signal<void ()> _signal_grabbed;
    sigc::signal<void ()> _signal_dragged;
    sigc::signal<void ()> _signal_released;
    sigc::signal<void (SPGradient*)> _signal_changed;

public:
    decltype(_signal_changed) signal_changed() const { return _signal_changed; }
    decltype(_signal_grabbed) signal_grabbed() const { return _signal_grabbed; }
    decltype(_signal_dragged) signal_dragged() const { return _signal_dragged; }
    decltype(_signal_released) signal_released() const { return _signal_released; }

    void setGradient(SPGradient *gradient) final;
    SPGradient *getVector() final;
    void setVector(SPDocument *doc, SPGradient *vector) final;
    void setMode(SelectorMode mode) final;
    void setUnits(SPGradientUnits units) final;
    SPGradientUnits getUnits() final;
    void setSpread(SPGradientSpread spread) final;
    SPGradientSpread getSpread() final;
    void selectStop(SPStop *selected) final;

private:
    void set_gradient(SPGradient* gradient);
    void stop_selected();
    void insert_stop_at(double offset);
    void add_stop(int index);
    void duplicate_stop();
    void delete_stop(int index);
    void show_stops(bool visible);
    void update_stops_layout();
    void set_repeat_mode(SPGradientSpread mode);
    void set_repeat_icon(SPGradientSpread mode);
    void reverse_gradient();
    void turn_gradient(double angle, bool relative);
    void set_stop_color(Colors::Color const &color);
    std::optional<Gtk::TreeRow> current_stop();
    SPStop* get_nth_stop(size_t index);
    SPStop* get_current_stop();
    bool select_stop(size_t index);
    void set_stop_offset(size_t index, double offset);
    SPGradient* get_gradient_vector();
    void fire_stop_selected(SPStop* stop);

    Glib::RefPtr<Gtk::Builder> _builder;
    GradientSelector* _selector;
    std::shared_ptr<Colors::ColorSet> _colors;
    std::unique_ptr<UI::Widget::PopoverMenu> _repeat_popover;
    Gtk::Image& _repeat_icon;
    GradientWithStops _gradient_image;
    Glib::RefPtr<Gtk::ListStore> _stop_list_store;
    Gtk::TreeModelColumnRecord _stop_columns;
    Gtk::TreeModelColumn<SPStop*> _stopObj;
    Gtk::TreeModelColumn<size_t> _stopIdx;
    Gtk::TreeModelColumn<Glib::ustring> _stopID;
    Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf>> _stop_color;
    Gtk::TreeView& _stop_tree;
    Gtk::Button& _turn_gradient;
    Glib::RefPtr<Gtk::Adjustment> _angle_adj;
    Gtk::SpinButton& _offset_btn;
    Gtk::Button& _add_stop;
    Gtk::Button& _delete_stop;
    Gtk::Expander& _show_stops_list;
    bool _stops_list_visible = true;
    Gtk::Box& _stops_gallery;
    Gtk::Box& _colors_box;
    Gtk::ToggleButton& _linear_btn;
    Gtk::ToggleButton& _radial_btn;
    Gtk::Grid& _main_grid;
    SPGradient* _gradient = nullptr;
    SPDocument* _document = nullptr;
    OperationBlocker _update;
    OperationBlocker _notification;
    Glib::ustring _prefs;
};

} // namespace Inkscape::UI::Widget

#endif // SEEN_GRADIENT_EDITOR_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim:filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99:
