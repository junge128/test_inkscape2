// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 *
 * Authors: see git history
 *
 * Copyright (C) 2023 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_COLORS_CMS_PROFILE_H
#define SEEN_COLORS_CMS_PROFILE_H

#include <cassert>
#include <lcms2.h> // cmsHPROFILE
#include <memory>
#include <string>
#include <vector>

namespace Inkscape::Colors::CMS {

class Profile
{
public:
    static std::shared_ptr<Profile> create(cmsHPROFILE handle, std::string path = "", bool in_home = false);
    static std::shared_ptr<Profile> create_from_uri(std::string path, bool in_home = false);
    static std::shared_ptr<Profile> create_from_data(std::string const &contents);
    static std::shared_ptr<Profile> create_srgb();

    static bool sortByName(std::shared_ptr<Profile> const &p1, std::shared_ptr<Profile> const &p2)
    {
        return p1->getName() < p2->getName();
    }
    static bool sortById(std::shared_ptr<Profile> const &p1, std::shared_ptr<Profile> const &p2)
    {
        return p1->_id < p2->_id;
    }

    Profile(cmsHPROFILE handle, std::string path, bool in_home);
    ~Profile() { cmsCloseProfile(_handle); }
    Profile(Profile const &) = delete;
    Profile &operator=(Profile const &) = delete;
    bool operator==(Profile const &other) const { return _checksum == other._checksum; }

    cmsHPROFILE getHandle() const { return _handle; }
    std::string const &getPath() const { return _path; }
    bool inHome() const { return _in_home; }

    bool isForDisplay() const;
    const std::string &getId() const { return _id; }
    const std::string &getChecksum() const { return _checksum; }
    std::string getName(bool sanitize = false) const;
    unsigned int getSize() const;
    cmsColorSpaceSignature getColorSpace() const;
    cmsProfileClassSignature getProfileClass() const;

    static bool isIccFile(std::string const &filepath);
    std::string dumpBase64() const;
    std::vector<unsigned char> dumpData() const;

private:
    cmsHPROFILE _handle;
    std::string _path;
    std::string _id;
    std::string _checksum;
    bool _in_home = false;

    std::string generate_id() const;
    std::string generate_checksum() const;
};

} // namespace Inkscape::Colors::CMS

#endif // SEEN_COLORS_CMS_PROFILE_H

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
