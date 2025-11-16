#include "svg_image_luna.h"

#include <fstream>
#include <vector>

#ifdef __WXMSW__
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#endif

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

#ifdef __WXMSW__
    // Try the fast MSW DIB path when stride is DWORD-aligned
    // (SetDIBits expects aligned scanlines for 32bpp DIB usage)
    if (stride % sizeof(LONG) == 0)
    {
        wxBitmap bmp(w, h, 32);

        // Prepare BITMAPINFO for top-down DIB
        BITMAPINFO bmi;
        ZeroMemory(&bmi, sizeof(bmi));
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        // negative height -> top-down DIB
        bmi.bmiHeader.biHeight = -h;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        const unsigned char* rowData = src;
        bool success = false;

        const HDC hScreenDC = ::GetDC(nullptr);
        if (hScreenDC)
        {
            // SetDIBits expects a pointer to the pixels in device independent format.
            // lunasvg returns BGRA bytes per pixel (B,G,R,A). For BI_RGB 32bpp we'll pass the rowData as-is.
            // Note: This path does not un-premultiply alpha. If you need that, we must allocate a temp buffer and unpremultiply.
            int ret = ::SetDIBits(hScreenDC, bmp.GetHBITMAP(), 0, h, (const void*)rowData, &bmi, DIB_RGB_COLORS);
            if (ret > 0)
                success = true;
            ::ReleaseDC(nullptr, hScreenDC);
        }

        if (success)
        {
            // Ensure wxWidgets treats the bitmap as having alpha
            bmp.UseAlpha();
            m_cachedBitmap = bmp;
            m_cachedWidth = width;
            m_cachedHeight = height;
            m_cachedScale = scale;
            m_dirty = false;
            return m_cachedBitmap;
        }
        // else fall through to safe (per-pixel) path
    }
#endif // __WXMSW__

    // Fallback: safe, portable path with BGRA -> RGB and unpremultiply alpha
    wxImage img(w, h, false);
    img.SetAlpha(); // ensures alpha buffer exists

    unsigned char* rgb = img.GetData();
    unsigned char* alpha = img.GetAlpha();

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
                // Use integer math to avoid floating point
                r = static_cast<unsigned char>((r * 255) / a);
                g = static_cast<unsigned char>((g * 255) / a);
                b = static_cast<unsigned char>((b * 255) / a);
            }

            *rgb++ = r;
            *rgb++ = g;
            *rgb++ = b;
            *alpha++ = a;

            row += 4;
        }
    }

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
