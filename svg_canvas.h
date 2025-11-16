#pragma once

#include <wx/scrolwin.h>
#include <wx/dcbuffer.h>
#include <vector>
#include <memory>
#include "svg_image_luna.h"

// Represents one item on the canvas
struct SvgItem
{
    SvgImageLuna svg;    // svg doc + render cache
    wxPoint pos;         // top-left position in virtual (logical) coordinates
    wxSize  baseSize;    // base logical size (before zoom), e.g., {128,128}
    wxString label;      // label to draw under icon
    bool visible = true;

    // Per-item convenience
    bool IsPointInside(const wxPoint& logicalPt, double zoom) const;
};

class SvgCanvas : public wxScrolledWindow
{
public:
    SvgCanvas(wxWindow* parent);

    // API
    bool AddSvgFile(const std::string& filePath, const wxPoint& pos, const wxSize& baseSize, const wxString& label = wxEmptyString);
    void Clear();

    // Query / operations
    void SetZoom(double zoom); // sets zoom and marks items dirty to re-render at new size
    double GetZoom() const { return m_zoom; }

protected:
    // paint, mouse, wheel handlers
    void OnPaint(wxPaintEvent& evt);
    void OnSize(wxSizeEvent& evt);
    void OnLeftDown(wxMouseEvent& evt);
    void OnLeftUp(wxMouseEvent& evt);
    void OnMouseMove(wxMouseEvent& evt);
    void OnRightDown(wxMouseEvent& evt);
    void OnRightUp(wxMouseEvent& evt);
    void OnMouseWheel(wxMouseEvent& evt);

    // helpers
    wxPoint ScreenToLogical(const wxPoint& pt) const;
    wxPoint LogicalToScreen(const wxPoint& pt) const;
    SvgItem* HitTest(const wxPoint& logicalPt, wxPoint* hitLocalOut = nullptr);

    void UpdateVirtualSize();

private:
    std::vector<std::shared_ptr<SvgItem>> m_items;

    // Dragging state
    std::shared_ptr<SvgItem> m_dragItem;
    wxPoint m_dragOffset; // offset from item top-left to mouse logical pos while dragging

    // Panning state (right-drag)
    bool m_panning;
    wxPoint m_panStartView; // view start (pixels)
    wxPoint m_panAnchor;    // mouse logical pos at start

    // Zoom
    double m_zoom;

    // Visual
    int m_labelHeight;
};
