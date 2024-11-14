// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Author:
 *   Johan B. C. Engelen
 *
 * Copyright (C) 2011 Author
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_WIDGET_SPINBUTTON_H
#define INKSCAPE_UI_WIDGET_SPINBUTTON_H

#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <gtkmm/spinbutton.h>

#include "ui/popup-menu.h"
#include "ui/widget/popover-menu.h"

namespace Gtk {
class Builder;
class EventControllerKey;
} // namespace Gtk

namespace Inkscape::UI { class DefocusTarget; }

namespace Inkscape::UI::Widget {

class UnitMenu;
class UnitTracker;

/**
 * A spin button for use with builders.
 */
class MathSpinButton : public Gtk::SpinButton
{
public:
    MathSpinButton(BaseObjectType *cobject, const Glib::RefPtr<Gtk::Builder> &refGlade);

private:
    int on_input(double &newvalue);
};

/**
 * SpinButton widget, that allows entry of simple math expressions (also units, when linked with UnitMenu),
 * and allows entry of both '.' and ',' for the decimal, even when in numeric mode.
 *
 * Calling "set_numeric()" effectively disables the expression parsing. If no unit menu is linked, all unitlike characters are ignored.
 */
class SpinButton : public Gtk::SpinButton
{
public:
    using NumericMenuData = std::map<double, Glib::ustring>;
    // We canʼt inherit ctors as if we declare SpinButton(), inherited ctors donʼt call it. Really!
    template <typename ...Args>
    SpinButton(Args &&...args)
        : Gtk::SpinButton(std::forward<Args>(args)...)
    { _construct(); } // Do the non-templated stuff

    SpinButton(BaseObjectType *cobject, Glib::RefPtr<Gtk::Builder> const &)
        : Gtk::SpinButton(cobject)
    { _construct(); }

    ~SpinButton() override;

    void setUnitMenu(UnitMenu* unit_menu) { _unit_menu = unit_menu; };
    void addUnitTracker(UnitTracker* ut) { _unit_tracker = ut; };

    // TODO: Might be better to just have a default value and a reset() method?
    inline void set_zeroable(const bool zeroable = true) { _zeroable = zeroable; }
    inline void set_oneable(const bool oneable = true) { _oneable = oneable; }

    inline bool get_zeroable() const { return _zeroable; }
    inline bool get_oneable() const { return _oneable; }

    void defocus();

    // set key up/down increment to override spin button adjustment step setting
    void set_increment(double delta);

private:
    UnitMenu    *_unit_menu    = nullptr; ///< Linked unit menu for unit conversion in entered expressions.
    UnitTracker *_unit_tracker = nullptr; ///< Linked unit tracker for unit conversion in entered expressions.
    double _on_focus_in_value  = 0.;
    Inkscape::UI::DefocusTarget *_defocus_target = nullptr; ///< Widget that should be informed when the spinbutton defocuses
    bool _zeroable = false; ///< Reset-value should be zero
    bool _oneable  = false; ///< Reset-value should be one
    bool _dont_evaluate = false; ///< Don't attempt to evaluate expressions
    NumericMenuData _custom_menu_data;
    bool _custom_popup = false;
    double _increment = 0.0;    // if > 0, key up/down will increment/decrement current value by this amount
    std::unique_ptr<UI::Widget::PopoverMenu> _popover_menu;

    void _construct();

    /**
     * This callback function should try to convert the entered text to a number and write it to newvalue.
     * It calls a method to evaluate the (potential) mathematical expression.
     *
     * @retval false No conversion done, continue with default handler.
     * @retval true  Conversion successful, don't call default handler.
     */
    int on_input(double &newvalue);

    /**
     * Handle specific keypress events, like Ctrl+Z.
     *
     * @retval false continue with default handler.
     * @retval true  don't call default handler.
     */
    bool on_key_pressed(Gtk::EventControllerKey const &controller,
                        unsigned keyval, unsigned keycode, Gdk::ModifierType state);

    bool on_popup_menu(PopupMenuOptionalClick);
    void create_popover_menu();
    void on_numeric_menu_item_activate(double value);

    /**
     * Undo the editing, by resetting the value upon when the spinbutton got focus.
     */
    void undo();

    void _unparentChildren();

public:
    inline void setDefocusTarget(decltype(_defocus_target) target) { _defocus_target = target; }
    inline void set_dont_evaluate(bool flag) { _dont_evaluate = flag; }

    void set_custom_numeric_menu_data(NumericMenuData &&custom_menu_data);
};

} // namespace Inkscape::UI::Widget

#endif // INKSCAPE_UI_WIDGET_SPINBUTTON_H

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
