// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * A C++ wrapper for lcms2 profiles
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "profile.h"

#include <fcntl.h>
#include <glib/gstdio.h>
#include <glibmm.h>
#include <glibmm/checksum.h>
#include <iomanip>
#include <map>
#include <sstream>

#include "system.h"

namespace Inkscape::Colors::CMS {

/**
 * Construct a color profile object from the lcms2 object.
 */
std::shared_ptr<Profile> Profile::create(cmsHPROFILE handle, std::string path, bool in_home)
{
    return handle ? std::make_shared<Profile>(handle, std::move(path), in_home) : nullptr;
}

/**
 * Construct a color profile object from a uri. Ownership of the lcms2 object is contained
 * within the Profile object and will be destroyed when it is.
 */
std::shared_ptr<Profile> Profile::create_from_uri(std::string path, bool in_home)
{
    if (cmsHPROFILE profile = cmsOpenProfileFromFile(path.c_str(), "r"))
        return Profile::create(profile, std::move(path), in_home);
    return nullptr;
}

/**
 * Construct a color profile object from the raw data.
 */
std::shared_ptr<Profile> Profile::create_from_data(std::string const &contents)
{
    if (cmsHPROFILE profile = cmsOpenProfileFromMem(contents.data(), contents.size()))
        return Profile::create(profile, "", false);
    return nullptr;
}

/**
 * Construct the default lcms sRGB color profile and return.
 */
std::shared_ptr<Profile> Profile::create_srgb()
{
    return Profile::create(cmsCreate_sRGBProfile());
}

Profile::Profile(cmsHPROFILE handle, std::string path, bool in_home)
    : _handle(handle)
    , _path(std::move(path))
    , _id(generate_id())
    , _checksum(generate_checksum())
    , _in_home(in_home)
{
    assert(_handle);
}

/**
 * Return true if this profile is for display/monitor correction.
 */
bool Profile::isForDisplay() const
{
    // If the profile has a Video Card Gamma Table (VCGT), then it's very likely to
    // be an actual monitor/display icc profile, and not just a display RGB profile.
    return getProfileClass() == cmsSigDisplayClass && getColorSpace() == cmsSigRgbData &&
           cmsIsTag(_handle, cmsSigVcgtTag);
}

/**
 * Cleans up name to remove disallowed characters.
 *
 * Allowed ASCII first characters:  ':', 'A'-'Z', '_', 'a'-'z'
 * Allowed ASCII remaining chars add: '-', '.', '0'-'9',
 *
 * @param str the string to clean up.
 */
static void sanitize_name(std::string &str)
{
    if (str.empty())
        return;
    auto val = str[0];
    if ((val < 'A' || val > 'Z') && (val < 'a' || val > 'z') && val != '_' && val != ':') {
        str.insert(0, "_");
    }
    for (std::size_t i = 1; i < str.size(); i++) {
        auto val = str[i];
        if ((val < 'A' || val > 'Z') && (val < 'a' || val > 'z') && (val < '0' || val > '9') && val != '_' &&
            val != ':' && val != '-' && val != '.') {
            if (str.at(i - 1) == '-') {
                str.erase(i, 1);
                i--;
            } else {
                str[i] = '-';
            }
        }
    }
    if (str.back() == '-') {
        str.pop_back();
    }
}

/**
 * Returns the name inside the icc profile, or empty string if it couldn't be
 * parsed out of the icc data correctly.
 */
std::string Profile::getName(bool sanitize) const
{
    std::string name;
    cmsUInt32Number byteLen = cmsGetProfileInfoASCII(_handle, cmsInfoDescription, "en", "US", nullptr, 0);
    if (byteLen > 0) {
        std::vector<char> data(byteLen);
        cmsUInt32Number readLen =
            cmsGetProfileInfoASCII(_handle, cmsInfoDescription, "en", "US", data.data(), data.size());
        if (readLen < data.size()) {
            g_warning("Profile::get_name(): icc data read less than expected!");
            data.resize(readLen);
        }
        // Remove nulls at end which will cause an invalid utf8 string.
        while (!data.empty() && data.back() == 0) {
            data.pop_back();
        }
        name = std::string(data.begin(), data.end());
    }
    if (sanitize)
        sanitize_name(name);
    return name;
}

cmsColorSpaceSignature Profile::getColorSpace() const
{
    return cmsGetColorSpace(_handle);
}

cmsProfileClassSignature Profile::getProfileClass() const
{
    return cmsGetDeviceClass(_handle);
}

struct InputFormatMap
{
    cmsColorSpaceSignature space;
    cmsUInt32Number inForm;
};

/**
 * Returns the number of channels this profile stores for color information.
 */
unsigned int Profile::getSize() const
{
    switch (getColorSpace()) {
        case cmsSigGrayData:
            return 1;
        case cmsSigCmykData:
            return 4;
        default:
            return 3;
    }
}

bool Profile::isIccFile(std::string const &filepath)
{
    bool is_icc_file = false;
    GStatBuf st;
    if (g_stat(filepath.c_str(), &st) == 0 && st.st_size > 128) {
        // 0-3 == size
        // 36-39 == 'acsp' 0x61637370
        int fd = g_open(filepath.c_str(), O_RDONLY, S_IRWXU);
        if (fd != -1) {
            guchar scratch[40] = {0};
            size_t len = sizeof(scratch);

            ssize_t got = read(fd, scratch, len);
            if (got != -1) {
                size_t calcSize = (scratch[0] << 24) | (scratch[1] << 16) | (scratch[2] << 8) | (scratch[3]);
                if (calcSize > 128 && calcSize <= static_cast<size_t>(st.st_size)) {
                    is_icc_file =
                        (scratch[36] == 'a') && (scratch[37] == 'c') && (scratch[38] == 's') && (scratch[39] == 'p');
                }
            }
            close(fd);

            if (is_icc_file) {
                cmsHPROFILE profile = cmsOpenProfileFromFile(filepath.c_str(), "r");
                if (profile) {
                    cmsProfileClassSignature profClass = cmsGetDeviceClass(profile);
                    if (profClass == cmsSigNamedColorClass) {
                        is_icc_file = false; // Ignore named color profiles for now.
                    }
                    cmsCloseProfile(profile);
                }
            }
        }
    }
    return is_icc_file;
}

/**
 * Get or generate a profile Id, and save in the object for later use.
 */
std::string Profile::generate_id() const
{
    // 1. get the id from the cms header itself, ususally correct.
    cmsUInt8Number tmp[16];
    cmsGetHeaderProfileID(_handle, tmp);

    std::ostringstream oo;
    oo << std::hex << std::setfill('0');
    for (auto &digit : tmp) {
        // Setw must happen each loop
        oo << std::setw(2) << static_cast<unsigned>(digit);
    }
#ifdef __APPLE__
    auto s = oo.str();
    if (std::count_if(s.begin(), s.end(), [](char c){ return c == '0'; }) < 24)
#else
    if (std::ranges::count(oo.str(), '0') < 24)
#endif
        return oo.str(); // Done
    // If there's no path, then what we have is a generated or in-memory profile
    // which is unlikely to ever need to be matched with anything via id but it's
    // also true that this id would change between computers, and creation date.
    if (_path.empty())
        return "";
    return generate_checksum();
}

/**
 * Generate a checksum of the data according to the ICC specification
 */
std::string Profile::generate_checksum() const
{
    // 2. If the id is empty, for some reason, we're going to generate it
    // from the data using the same method that should have been used originally
    // See ICC.1-2022-05 7.2.18 Profile ID field.
    auto data = dumpData();
    if (data.size() < 100) {
        g_warning("Bad icc profile data when generating profile id.");
        return "~";
    }
    // Zero out the required bytes as per the above specification
    for (unsigned i = 64; i < 68; i++)
        data[i] = 0;
    for (unsigned i = 84; i < 100; i++)
        data[i] = 0;
    return Glib::Checksum::compute_checksum(Glib::Checksum::Type::MD5, std::string(data.begin(), data.end()));
}

/**
 * Dump the entire profile as a base64 encoded string. This is used for color-profile href data.
 */
std::string Profile::dumpBase64() const
{
    auto buf = dumpData();
    return Glib::Base64::encode(std::string(buf.begin(), buf.end()));
}

/**
 * Dump the entire profile as raw data. Used in dumpBase64 and generateId
 */
std::vector<unsigned char> Profile::dumpData() const
{
    cmsUInt32Number len = 0;
    if (!cmsSaveProfileToMem(_handle, nullptr, &len)) {
        throw CmsError("Can't extract profile data");
    }
    auto buf = std::vector<unsigned char>(len);
    cmsSaveProfileToMem(_handle, &buf.front(), &len);
    return buf;
}

} // namespace Inkscape::Colors::CMS

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
