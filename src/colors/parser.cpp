// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 AUTHORS
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "parser.h"

#include <iostream>
#include <sstream>

#include "spaces/cms.h"
#include "spaces/cmyk.h"
#include "spaces/gray.h"
#include "spaces/hsl.h"
#include "spaces/hsluv.h"
#include "spaces/hsv.h"
#include "spaces/lab.h"
#include "spaces/lch.h"
#include "spaces/linear-rgb.h"
#include "spaces/luv.h"
#include "spaces/named.h"
#include "spaces/okhsl.h"
#include "spaces/oklab.h"
#include "spaces/oklch.h"
#include "spaces/rgb.h"
#include "spaces/xyz.h"
#include "utils.h"

namespace Inkscape::Colors {

Parsers::Parsers()
{
    addParser(new HexParser());
    addParser(new Space::NamedColor::NameParser());
    addParser(new Space::CMS::CmsParser());
    addParser(new Space::RGB::Parser(false));
    addParser(new Space::RGB::Parser(true));
    addParser(new Space::HSL::Parser(false));
    addParser(new Space::HSL::Parser(true));
    addParser(new Space::HSV::fromHwbParser(false));
    addParser(new Space::HSV::fromHwbParser(true));
    addParser(new Space::Lab::Parser());
    addParser(new Space::Lch::Parser());
    addParser(new Space::OkLab::Parser());
    addParser(new Space::OkLch::Parser());
    addParser(new CssParser("srgb", Space::Type::RGB, 3));
    addParser(new CssParser("srgb-linear", Space::Type::linearRGB, 3));
    addParser(new CssParser("device-cmyk", Space::Type::CMYK, 4));
    addParser(new CssParser("xyz", Space::Type::XYZ, 3));
}

/**
 * Turn a string into a color data, used in Color object creation.
 *
 * Each available color parser will be asked to parse the color in turn
 * and the successful parser will return object data.
 *
 * @arg input - The string to parse.
 * @retval type - The type (enum) of the color space to understand the values
 * @retval cms - The name of the cms color space to understand the values
 * @retval values - A list of values for use with the color space.
 * @retval fallback - A returned list of values for fallback colors.
 *
 * @returns true if the color was parsed.
 */
bool Parsers::parse(std::string const &input, Space::Type &type, std::string &cms, std::vector<double> &values,
                    std::vector<double> &fallback) const
{
    std::istringstream ss(input);
    return _parse(ss, type, cms, values, fallback);
}

/**
 * Internal recursive parser that scans through a string stream.
 *
 * @arg ss - The string stream, parsing starts at it's set position
 * @retval type - The type (enum) of the color space to understand the values
 * @retval cms - The return name of the color space to understand the values
 * @retval values - A returned list of values for use with the color space.
 * @retval fallback - A returned list of values for the fallback (sRGB) color
 *
 * @returns true if the color was parsed.
 */
bool Parsers::_parse(std::istringstream &ss, Space::Type &type, std::string &name, std::vector<double> &values,
                     std::vector<double> &fallback) const
{
    auto ptype = Parser::getCssPrefix(ss);
    auto iter = _parsers.find(ptype);
    if (iter == _parsers.end())
        return false;

    for (auto &parser : iter->second) {
        auto pos = ss.tellg();
        bool more = false;
        values.clear();

        name = parser->parseColor(ss, values, more);

        // We have an RGB color but there's more string, look for an icc-color next.
        if (more && ptype == "#") {
            std::vector<double> icc_values;
            if (_parse(ss, type, name, icc_values, fallback)) {
                if (type == Space::Type::CMS) {
                    fallback = std::move(values);
                    values = std::move(icc_values);
                    return true;
                }
            }
        }

        if (!values.empty()) {
            type = parser->getType();
            return true;
        }

        ss.clear();
        ss.seekg(pos);
    }
    return false;
}

/**
 * Add a prser to the list of parser objects used when parsing color strings.
 */
void Parsers::addParser(Parser *parser)
{
    // Map parsers by their expected prefix for quicker lookup
    auto const [it, inserted] = _parsers.emplace(parser->getPrefix(), std::vector<std::shared_ptr<Parser>>{});
    it->second.emplace_back(parser);
}

/**
 * Parse this specific color format into output values
 *
 * @arg ss - The stream stream to parse
 * @arg output - The returned list of values
 * @arg more - Indicates if there is more string to parse.
 *
 * @returns The type of the space found by this parser (if any).
 */
std::string Parser::parseColor(std::istringstream &ss, std::vector<double> &output, bool &more) const
{
    if (!parse(ss, output, more)) {
        output.clear();
    }
    return "";
}

bool HueParser::parse(std::istringstream &ss, std::vector<double> &output) const
{
    bool end = false;
    return append_css_value(ss, output, end, ',', 360) && append_css_value(ss, output, end, ',') &&
           append_css_value(ss, output, end, !_alpha ? '/' : ',') && (append_css_value(ss, output, end) || !_alpha) &&
           end;
}

/**
 * Parse either a hex code or an rgb() css string.
 */
bool HexParser::parse(std::istringstream &ss, std::vector<double> &output, bool &more) const
{
    unsigned int hex;
    unsigned int size = 0;

    size = ss.tellg();
    ss >> std::hex >> hex;
    // This mess is required because istream countg is inconsistant
    size = (ss.tellg() == -1 ? ss.str().size() : (int)ss.tellg()) - size;

    if (size == 3 || size == 4) { // #rgb(a)
        for (int p = (4 * (size - 1)); p >= 0; p -= 4) {
            auto val = ((hex & (0xf << p)) >> p);
            output.emplace_back((val + (val << 4)) / 255.0);
        }
    } else if (size == 6 || size == 8) { // #rrggbb(aa)
        if (size == 6)
            hex <<= 8;
        output.emplace_back(SP_RGBA32_R_F(hex));
        output.emplace_back(SP_RGBA32_G_F(hex));
        output.emplace_back(SP_RGBA32_B_F(hex));
        if (size == 8)
            output.emplace_back(SP_RGBA32_A_F(hex));
    }
    ss >> std::ws;
    more = (ss.peek() == 'i'); // icc-color is next
    return !output.empty();
}

/**
 * Prase the given string stream as a CSS Color Module Level 4/5 string.
 */
bool CssParser::parse(std::istringstream &ss, std::vector<double> &output) const
{
    bool end = false;
    while (!end && output.size() < _channels + 1) {
        if (!append_css_value(ss, output, end, output.size() == _channels - 1 ? '/' : 0x0))
            break;
    }
    return end;
}

/**
 * Parse CSS color numbers after the function name
 *
 * @arg ss - The string stream to read
 *
 * @returns - The color prefix or color name detected in this color function.
 *            either the first part of the function, for example rgb or hsla
 *            or the first variable in the case of color(), icc-color() and var()
 */
std::string Parser::getCssPrefix(std::istringstream &ss)
{
    std::string token;
    ss >> std::ws;
    if (ss.peek() == '#') {
        return {(char)ss.get()};
    }
    auto pos = ss.tellg();
    if (!std::getline(ss, token, '(') || ss.eof()) {
        ss.seekg(pos);
        return ""; // No paren
    }

    if (token == "color") {
        // CSS Color module 4 color() function
        ss >> token;
    }
    return token;
}

/**
 * Parse a CSS color number after the function name
 *
 * @arg ss - The string stream to read
 * @returns value - The value read in without adjustment
 * @returns unit - The unit, ususally an empty string
 * @returns end - True if this is the end of the css function
 * @arg sep - An optional seperator argument
 *
 * @returns true if a number and unit was parsed correctly.
 */
bool Parser::css_number(std::istringstream &ss, double &value, std::string &unit, bool &end, char const sep)
{
    ss.imbue(std::locale("C"));

    /*
     * LLVM uses libc++ and this C library has a broken double parser compared to GCC/libstdc++
     * So we have written our own double parser to get around the limitations for APPLE
     *
     * Detailed description can be found at:
     * https://github.com/tardate/LittleCodingKata/blob/main/cpp/DoubleTrouble/README.md
     */
#ifdef __APPLE__
    bool parsed = false;
    bool dot = false;

    ss >> std::ws;
    std::string result;
    while (true) {
        auto c = ss.peek();

        if (c == '-' && result.empty()) {
            result += ss.get();
        } else if (c == '.') {
            if (dot)
                break;
            dot = true;
            result += ss.get();
        } else if (c >= '0' && c <= '9') {
            result += ss.get();
            parsed = true;
        } else {
            break;
        }
    }
    if (parsed) {
        value = std::stod(result);
    }
#else
    bool parsed = (bool)(ss >> value);
#endif
    if (!parsed) {
        ss.clear();
        return false;
    }

    unit.clear();
    auto c = ss.peek();
    if (c == '.' || (c >= '0' && c <= '9')) {
        return true;
    }
    while (ss && (c = ss.get())) {
        if (c == ')') {
            end = true;
            break;
        } else if (c == sep) {
            break;
        }
        if (c == ' ') {
            auto p = ss.peek();
            if (p != ' ' && p != sep && p != ')') {
                break;
            }
        } else {
            unit += c;
        }
    }
    return true;
}

/**
 * Parse a CSS color number and format it according to it's unit.
 *
 * @arg ss - The string stream set at the location of the next number.
 * @arg output - The vector to append the new number to.
 * @arg end - Is set to true if this number is the last one.
 * @arg sep - The separator to expect after this number (consumed)
 * @arg scale - The default scale of the number of no unit is detected.
 *
 * @returns True if a number was found and appended.
 */
bool Parser::append_css_value(std::istringstream &ss, std::vector<double> &output, bool &end, char const sep,
                              double scale)
{
    double value;
    std::string unit;
    if (!end && css_number(ss, value, unit, end, sep)) {
        if (unit == "%") {
            value /= 100;
        } else if (unit == "deg") {
            value /= 360;
        } else if (unit == "turn") {
            // no need to modify
        } else if (!unit.empty()) {
            std::cerr << "Unknown unit in css color parsing '" << unit.c_str() << "'\n";
            return false;
        } else {
            value /= scale;
        }
        output.emplace_back(value);
        return true;
    }
    return false;
}

}; // namespace Inkscape::Colors

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
