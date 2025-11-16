#pragma once

#include <memory>
#include <string>

#include <wx/bitmap.h>
#include "lunasvg.h"

// Minimal wrapper: exposes document, supports load, render, dirty flag, per-scale caching.
class SvgImageLuna
{
public:
    SvgImageLuna();
    ~SvgImageLuna() = default;

    // Load SVG
    bool LoadFromFile(const std::string& filePath);
    bool LoadFromString(const std::string& svgText);

    // Document access
    std::shared_ptr<lunasvg::Document> GetDocument() const { return m_document; }

    // Mark as modified externally
    void MarkDirty() { m_dirty = true; }

    // Render at exact pixel size (width, height). This updates the internal cache
    // and cachedScale (scale is user logical scale factor; for our usage we'll use zoom as scale).
    wxBitmap Render(int width, int height, double scale);

    // Get cached bitmap if matches size and scale; returns invalid bitmap if none.
    wxBitmap GetCachedBitmap(int width, int height, double scale) const;

    bool IsDirty() const { return m_dirty; }

private:
    bool ParseDocument(); // (re)parse m_svgText

private:
    std::string m_svgText;
    std::shared_ptr<lunasvg::Document> m_document;

    // Cached bitmap for last Render()
    mutable wxBitmap m_cachedBitmap;
    mutable int m_cachedWidth;
    mutable int m_cachedHeight;
    mutable double m_cachedScale;
    mutable bool m_dirty;
};
