/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkScalarContext_win_dw_DEFINED
#define SkScalarContext_win_dw_DEFINED

#include "include/core/SkScalar.h"
#include "include/core/SkTypes.h"
#include "include/private/SkTDArray.h"
#include "src/core/SkScalerContext.h"
#include "src/ports/SkTypeface_win_dw.h"

#include <dwrite.h>
#include <dwrite_2.h>

class SkGlyph;
class SkDescriptor;

class SkScalerContext_DW : public SkScalerContext {
public:
    SkScalerContext_DW(sk_sp<DWriteFontTypeface>,
                       const SkScalerContextEffects&,
                       const SkDescriptor*);
    ~SkScalerContext_DW() override;

protected:
    bool generateAdvance(SkGlyph* glyph) override;
    void generateMetrics(SkGlyph* glyph, SkArenaAlloc*) override;
    void generateImage(const SkGlyph& glyph) override;
    bool generatePath(const SkGlyph&, SkPath*) override;
    sk_sp<SkDrawable> generateDrawable(const SkGlyph&) override;
    void generateFontMetrics(SkFontMetrics*) override;

private:
    static void BilevelToBW(const uint8_t* SK_RESTRICT src, const SkGlyph& glyph);

    template<bool APPLY_PREBLEND>
    static void GrayscaleToA8(const uint8_t* SK_RESTRICT src,
                              const SkGlyph& glyph,
                              const uint8_t* table8);

    template<bool APPLY_PREBLEND>
    static void RGBToA8(const uint8_t* SK_RESTRICT src,
                        const SkGlyph& glyph,
                        const uint8_t* table8);

    template<bool APPLY_PREBLEND, bool RGB>
    static void RGBToLcd16(const uint8_t* SK_RESTRICT src, const SkGlyph& glyph,
                           const uint8_t* tableR, const uint8_t* tableG, const uint8_t* tableB);

    const void* drawDWMask(const SkGlyph& glyph,
                           DWRITE_RENDERING_MODE renderingMode,
                           DWRITE_TEXTURE_TYPE textureType);

    HRESULT getBoundingBox(SkGlyph* glyph,
                           DWRITE_RENDERING_MODE renderingMode,
                           DWRITE_TEXTURE_TYPE textureType,
                           RECT* bbox);

    DWriteFontTypeface* getDWriteTypeface() {
        return static_cast<DWriteFontTypeface*>(this->getTypeface());
    }

    bool isColorGlyph(const SkGlyph&);
    bool getColorGlyphRun(const SkGlyph&, IDWriteColorGlyphRunEnumerator**);
    bool generateColorMetrics(SkGlyph*);
    void generateColorGlyphImage(const SkGlyph&);
    void drawColorGlyphImage(const SkGlyph&, SkCanvas&);

    bool isPngGlyph(const SkGlyph&);
    bool generatePngMetrics(SkGlyph*);
    void generatePngGlyphImage(const SkGlyph&);
    void drawPngGlyphImage(const SkGlyph&, SkCanvas&);


    SkTDArray<uint8_t> fBits;
    /** The total matrix without the text height scale. */
    SkMatrix fSkXform;
    /** The total matrix without the text height scale. */
    DWRITE_MATRIX fXform;
    /** The text size to render with. */
    SkScalar fTextSizeRender;
    /** The text size to measure with. */
    SkScalar fTextSizeMeasure;
    int fGlyphCount;
    DWRITE_RENDERING_MODE fRenderingMode;
    DWRITE_TEXTURE_TYPE fTextureType;
    DWRITE_MEASURING_MODE fMeasuringMode;
    DWRITE_TEXT_ANTIALIAS_MODE fAntiAliasMode;
    DWRITE_GRID_FIT_MODE fGridFitMode;
};

#endif
