// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2023 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_COLORS_SPACES_CMS_H
#define SEEN_COLORS_SPACES_CMS_H

#include "base.h"

namespace Inkscape::Colors {
class DocumentCMS;
}

namespace Inkscape::Colors::Space {

// A class for containing the icc profile and the machinery for converting colors
class CMS : public AnySpace
{
public:
    CMS(std::shared_ptr<Colors::CMS::Profile> profile);
    CMS(std::string profile_name, unsigned profile_size, Type profile_type = Type::NONE);
    ~CMS() override = default;

    Type getType() const override { return _profile_type; }
    std::string const getName() const override { return _profile_name; }
    std::string const getIcon() const override { return "color-selector-cms"; }
    unsigned int getComponentCount() const override;

    std::shared_ptr<Colors::CMS::Profile> const getProfile() const override;
    RenderingIntent getIntent() const override { return _intent; }
    void setIntent(RenderingIntent intent) { _intent = intent; }

    /** Returns false if this icc profile is not connected to any actual profile */
    bool isValid() const override { return (bool)_profile; }

    void spaceToProfile(std::vector<double> &io) const override;
protected:
    friend class Colors::Color;
    friend class Colors::DocumentCMS;

    std::string toString(std::vector<double> const &values, bool opacity = true) const override;
    uint32_t toRGBA(std::vector<double> const &values, double opacity = 1.0) const override;

    void setName(std::string name) { _profile_name = std::move(name); }
    bool overInk(std::vector<double> const &input) const override;

private:
    std::string _profile_name;
    unsigned _profile_size;
    Type _profile_type;
    std::shared_ptr<Colors::CMS::Profile> _profile;
    RenderingIntent _intent = RenderingIntent::UNKNOWN;

public:
    class CmsParser : public Parser
    {
    public:
        CmsParser()
            : Parser("icc-color", Type::CMS)
        {}
        std::string parseColor(std::istringstream &input, std::vector<double> &output, bool &more) const override;
    };
};

} // namespace Inkscape::Colors::Space

#endif // SEEN_COLORS_SPACES_CMS_H
