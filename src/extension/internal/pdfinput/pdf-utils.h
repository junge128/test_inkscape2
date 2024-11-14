// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * PDF Parsing utility functions and classes.
 *//*
 * 
 * Copyright (C) 2022 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef PDF_UTILS_H
#define PDF_UTILS_H

#include <2geom/rect.h>
#include "poppler-transition-api.h"
#include <2geom/affine.h>
#include <poppler/Gfx.h>
#include <poppler/GfxState.h>
#include <poppler/Page.h>

class ClipHistoryEntry
{
public:
    ClipHistoryEntry(GfxPath *clipPath = nullptr, GfxClipType clipType = clipNormal);
    virtual ~ClipHistoryEntry();

    // Manipulate clip path stack
    ClipHistoryEntry *save(bool cleared = false);
    ClipHistoryEntry *restore();
    bool hasSaves() { return saved != nullptr; }
    bool hasClipPath() { return clipPath != nullptr && !cleared; }
    bool isCopied() { return copied; }
    bool isBoundingBox() { return is_bbox; }
    void setClip(GfxState *state, GfxClipType newClipType = clipNormal, bool bbox = false);
    GfxPath *getClipPath() { return clipPath; }
    GfxClipType getClipType() { return clipType; }
    const Geom::Affine &getAffine() { return affine; }
    bool evenOdd() { return clipType != clipNormal; }
    void clear() { cleared = true; }

private:
    ClipHistoryEntry *saved; // next clip path on stack

    Geom::Affine affine = Geom::identity(); // Saved affine state of the clipPath
    GfxPath *clipPath;                      // used as the path to be filled for an 'sh' operator
    GfxClipType clipType;
    bool is_bbox = false;
    bool cleared = false;
    bool copied = false;

    ClipHistoryEntry(ClipHistoryEntry *other, bool cleared = false);
};

Geom::Rect getRect(_POPPLER_CONST PDFRectangle *box);

#endif /* PDF_UTILS_H */
