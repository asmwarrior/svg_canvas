#include "svg_image_luna.h"

#include <fstream>
#include <vector>

SvgImageLuna::SvgImageLuna()
    : m_cachedWidth(0)
    , m_cachedHeight(0)
    , m_cachedScale(0.0)
    , m_dirty(true)
{
}

bool SvgImageLuna::LoadFromFile(const std::string& filePath)
{
    std::ifstream ifs(filePath, std::ios::binary);
    if (!ifs) return false;

    m_svgText.assign(
        std::istreambuf_iterator<char>(ifs),
        std::istreambuf_iterator<char>()
    );
    return ParseDocument();
}

bool SvgImageLuna::LoadFromString(const std::string& svgText)
{
    m_svgText = svgText;
    return ParseDocument();
}

bool SvgImageLuna::ParseDocument()
{
    m_document.reset();
    if (m_svgText.empty())
        return false;

    m_document = lunasvg::Document::loadFromData(m_svgText);
    m_dirty = true;
    return (bool)m_document;
}

wxBitmap SvgImageLuna::Render(int width, int height, double scale)
{
    if (!m_document)
        return wxBitmap();

    // Cache hit → return directly
    if (!m_dirty &&
        m_cachedBitmap.IsOk() &&
        m_cachedWidth == width &&
        m_cachedHeight == height &&
        m_cachedScale == scale)
    {
        return m_cachedBitmap;
    }

    // Render using lunasvg
    lunasvg::Bitmap lbmp = m_document->renderToBitmap(width, height);
    if (!lbmp.valid())
        return wxBitmap();

    const unsigned char* src = lbmp.data();
    const int stride = lbmp.stride();
    const int w = lbmp.width();
    const int h = lbmp.height();

    // wxImage stores RGB + separate alpha buffer
    wxImage img(w, h, false);
    img.SetAlpha();  // ensures alpha buffer exists

    unsigned char* rgb = img.GetData();
    unsigned char* alpha = img.GetAlpha();

    // Convert BGRA → RGBA (with unpremultiply)
    for (int y = 0; y < h; ++y)
    {
        const unsigned char* row = src + y * stride;

        for (int x = 0; x < w; ++x)
        {
            unsigned char b = row[0];
            unsigned char g = row[1];
            unsigned char r = row[2];
            unsigned char a = row[3];

            // Unpremultiply (lunasvg outputs premultiplied alpha)
            if (a != 0)
            {
                r = (r * 255) / a;
                g = (g * 255) / a;
                b = (b * 255) / a;
            }

            *rgb++ = r;
            *rgb++ = g;
            *rgb++ = b;
            *alpha++ = a;

            row += 4;
        }
    }

    // Convert to wxBitmap
    m_cachedBitmap = wxBitmap(img);
    m_cachedWidth = width;
    m_cachedHeight = height;
    m_cachedScale = scale;
    m_dirty = false;

    return m_cachedBitmap;
}

wxBitmap SvgImageLuna::GetCachedBitmap(int width, int height, double scale) const
{
    if (!m_cachedBitmap.IsOk()) return wxBitmap();
    if (!m_dirty &&
        m_cachedWidth == width &&
        m_cachedHeight == height &&
        m_cachedScale == scale)
    {
        return m_cachedBitmap;
    }
    return wxBitmap();
}
