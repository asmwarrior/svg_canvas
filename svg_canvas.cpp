#include "svg_canvas.h"
#include <wx/dcbuffer.h>
#include <wx/dcmirror.h>
#include <algorithm>

SvgCanvas::SvgCanvas(wxWindow* parent)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL | wxVSCROLL | wxBORDER_SIMPLE)
    , m_dragItem(nullptr)
    , m_panning(false)
    , m_zoom(1.0)
    , m_labelHeight(18)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetScrollRate(10, 10);

    Bind(wxEVT_PAINT, &SvgCanvas::OnPaint, this);
    Bind(wxEVT_SIZE, &SvgCanvas::OnSize, this);
    Bind(wxEVT_LEFT_DOWN, &SvgCanvas::OnLeftDown, this);
    Bind(wxEVT_LEFT_UP, &SvgCanvas::OnLeftUp, this);
    Bind(wxEVT_MOTION, &SvgCanvas::OnMouseMove, this);
    Bind(wxEVT_RIGHT_DOWN, &SvgCanvas::OnRightDown, this);
    Bind(wxEVT_RIGHT_UP, &SvgCanvas::OnRightUp, this);
    Bind(wxEVT_MOUSEWHEEL, &SvgCanvas::OnMouseWheel, this);
}

bool SvgCanvas::AddSvgFile(const std::string& filePath, const wxPoint& pos, const wxSize& baseSize, const wxString& label)
{
    auto item = std::make_shared<SvgItem>();
    if (!item) return false;

    if (!item->svg.LoadFromFile(filePath))
        return false;

    item->pos = pos;
    item->baseSize = baseSize;
    item->label = label;
    item->visible = true;

    // initial render at current zoom (we leave it dirty so it will render on paint)
    item->svg.MarkDirty();

    m_items.push_back(item);
    UpdateVirtualSize();
    Refresh();
    return true;
}

void SvgCanvas::Clear()
{
    m_items.clear();
    UpdateVirtualSize();
    Refresh();
}

void SvgCanvas::SetZoom(double zoom)
{
    if (zoom <= 0.05) zoom = 0.05;
    if (zoom > 10.0) zoom = 10.0;
    m_zoom = zoom;

    // Mark all items dirty so they will be re-rendered at new size on next paint
    for (auto& it : m_items)
        it->svg.MarkDirty();

    UpdateVirtualSize();
    Refresh();
}

void SvgCanvas::OnSize(wxSizeEvent& evt)
{
    evt.Skip();
    UpdateVirtualSize();
}

void SvgCanvas::OnPaint(wxPaintEvent& WXUNUSED(evt))
{
    wxAutoBufferedPaintDC dc(this);
    PrepareDC(dc);

    dc.SetBackground(*wxWHITE_BRUSH);
    dc.Clear();

    // Draw items
    for (auto& item : m_items)
    {
        if (!item->visible) continue;

        // compute scaled size
        int w = static_cast<int>(std::round(item->baseSize.GetWidth() * m_zoom));
        int h = static_cast<int>(std::round(item->baseSize.GetHeight() * m_zoom));

        // Logical position to device (dc coordinates use scrolled coords already)
        wxPoint deviceTopLeft = item->pos;
        // Render item if needed
        wxBitmap bmp = item->svg.GetCachedBitmap(w, h, m_zoom);
        if (!bmp.IsOk() || item->svg.IsDirty())
        {
            bmp = item->svg.Render(w, h, m_zoom);
        }

        if (bmp.IsOk())
        {
            dc.DrawBitmap(bmp, deviceTopLeft.x, deviceTopLeft.y, true);

            // Draw selection rectangle
            if (item.get() == m_selectedItem)
            {
                wxPen selPen(*wxBLUE, 2);
                dc.SetPen(selPen);
                dc.SetBrush(*wxTRANSPARENT_BRUSH);
                dc.DrawRectangle(deviceTopLeft.x, deviceTopLeft.y, w, h);
            }

            // Draw label underneath
            if (!item->label.IsEmpty())
            {
                wxFont f = dc.GetFont();
                dc.SetFont(f);
                int tx = deviceTopLeft.x;
                int ty = deviceTopLeft.y + bmp.GetHeight() + 4;
                dc.DrawText(item->label, tx, ty);
            }
        }
        else
        {
            // draw placeholder rectangle
            dc.SetBrush(*wxLIGHT_GREY_BRUSH);
            dc.DrawRectangle(deviceTopLeft.x, deviceTopLeft.y, std::max(10, w), std::max(10, h));
            if (!item->label.IsEmpty())
                dc.DrawText(item->label, deviceTopLeft.x, deviceTopLeft.y + h + 4);
        }
    }
}

wxPoint SvgCanvas::ScreenToLogical(const wxPoint& pt) const
{
    // Convert screen (device) to logical coordinates
    wxPoint logical = CalcUnscrolledPosition(pt);
    logical.x = static_cast<int>(logical.x / m_zoom);
    logical.y = static_cast<int>(logical.y / m_zoom);
    return logical;
}

wxPoint SvgCanvas::LogicalToScreen(const wxPoint& pt) const
{
    // Convert logical to screen/device coordinates
    wxPoint scaled(static_cast<int>(pt.x * m_zoom), static_cast<int>(pt.y * m_zoom));
    return CalcScrolledPosition(scaled);
}


// Pixel-perfect hit test: check bitmap alpha at local point
SvgItem* SvgCanvas::HitTest(const wxPoint& logicalPt, wxPoint* hitLocalOut)
{
    // iterate in reverse order to prefer topmost
    for (auto it = m_items.rbegin(); it != m_items.rend(); ++it)
    {
        SvgItem* item = it->get();
        if (!item->visible) continue;
        int w = static_cast<int>(std::round(item->baseSize.GetWidth() * m_zoom));
        int h = static_cast<int>(std::round(item->baseSize.GetHeight() * m_zoom));
        wxRect rect(item->pos, wxSize(w, h));
        if (!rect.Contains(logicalPt)) continue;

        // local coordinates within bitmap
        int lx = logicalPt.x - item->pos.x;
        int ly = logicalPt.y - item->pos.y;
        if (lx < 0 || ly < 0 || lx >= w || ly >= h) continue;

        // get cached or render the bitmap temporarily (no cache mutation here)
        wxBitmap bmp = item->svg.GetCachedBitmap(w, h, m_zoom);
        if (!bmp.IsOk())
            bmp = item->svg.Render(w, h, m_zoom);

        if (!bmp.IsOk()) continue;

        wxImage img = bmp.ConvertToImage();
        if (!img.HasAlpha())
        {
            // If no alpha, treat entire rect as hit
            if (hitLocalOut) *hitLocalOut = wxPoint(lx, ly);
            return item;
        }

        unsigned char alpha = img.GetAlpha(lx, ly);
        if (alpha > 10) // threshold
        {
            if (hitLocalOut) *hitLocalOut = wxPoint(lx, ly);
            return item;
        }
    }
    return nullptr;
}

void SvgCanvas::OnLeftDown(wxMouseEvent& evt)
{
    wxPoint logical = ScreenToLogical(evt.GetPosition());
    wxPoint local;
    auto hit = HitTest(logical, &local);

    if (hit)
    {
        // Set the clicked SVG as selected
        m_selectedItem = hit;

        // Start dragging
        m_dragItem = std::shared_ptr<SvgItem>(hit, [](SvgItem*){ /* non-owning */ });
        m_dragOffset = wxPoint(local.x, local.y);

        CaptureMouse();

        Refresh(false); // optional: redraw selection rectangle
    }
    else
    {
        // Clicked empty space → clear selection
        m_selectedItem = nullptr;
        evt.Skip();
        Refresh(false);
    }
}


void SvgCanvas::OnLeftUp(wxMouseEvent& evt)
{
    if (m_dragItem && HasCapture())
    {
        ReleaseMouse();
        m_dragItem.reset();
    }
    else
        evt.Skip();
}

void SvgCanvas::OnMouseMove(wxMouseEvent& evt)
{
    wxPoint logical = ScreenToLogical(evt.GetPosition());

    if (m_dragItem)
    {
        // Update item position to keep the offset constant
        wxPoint newTopLeft(logical.x - m_dragOffset.x, logical.y - m_dragOffset.y);
        m_dragItem->pos = newTopLeft;
        UpdateVirtualSize();
        Refresh(false);
        return;
    }

    if (m_panning && evt.Dragging() && evt.RightIsDown())
    {
        // Panning: adjust view start based on mouse movement
        wxPoint cur = GetViewStart();
        // Compute delta in pixels between anchor and current mouse
        // We will simulate moving view by changing scroll start
        // Simpler: use Scroll and track difference
        // But here we'll compute delta in pixels:
        // dx = anchor.x - logical.x; dy = anchor.y - logical.y
        // Convert to scroll units
        wxPoint curPixels = GetViewStart();
        // Not necessary complicated: call Scroll with appropriate units
        // We'll compute pixel difference relative to last known pan anchor
        // For simplicity we just Scroll by -delta/scrollRate
        // NOTE: Keep it simple: use Screen coords for panning movement

        // We'll handle panning in OnRightDrag - simpler approach
    }

    evt.Skip();
}

void SvgCanvas::OnRightDown(wxMouseEvent& evt)
{
    m_panning = true;
    m_panAnchor = ScreenToLogical(evt.GetPosition());
    m_panStartView = GetViewStart();
    CaptureMouse();
}

void SvgCanvas::OnRightUp(wxMouseEvent& evt)
{
    if (m_panning)
    {
        m_panning = false;
        if (HasCapture()) ReleaseMouse();
    }
}

void SvgCanvas::OnMouseWheel(wxMouseEvent& evt)
{
    // Ctrl + wheel -> zoom
    if (evt.CmdDown() || evt.ControlDown())
    {
        int rot = evt.GetWheelRotation();
        double delta = rot > 0 ? 1.1 : 1.0 / 1.1;
        double oldZoom = m_zoom;
        SetZoom(m_zoom * delta);

        // Keep mouse position stable (zoom to cursor): adjust scroll so the point under cursor stays
        wxPoint mouseScreen = evt.GetPosition();
        wxPoint beforeLogical = ScreenToLogical(mouseScreen);

        // After zoom, the logical coordinates remain same, but contents scale so we need to adjust scroll
        // Compute new scroll position so that beforeLogical maps to same screen point
        // New device position of beforeLogical = beforeLogical (since logical coords same) => but we changed virtual size
        // We'll compute scrolled position needed:
        // We'll find new view start so that CalcScrolledPosition(beforeLogical) == mouseScreen
        wxPoint newDevice = beforeLogical;
        // CalcScrolledPosition gives device coords from logical
        int newDevX = newDevice.x;
        int newDevY = newDevice.y;
        // Convert desired device to scroll start:
        // desiredViewStartPixels = beforeLogical - mouseScreen
        wxPoint desiredViewStart(
            std::max(0, beforeLogical.x - mouseScreen.x),
            std::max(0, beforeLogical.y - mouseScreen.y)
        );

        // Convert to scroll units
        int unitX, unitY;
        GetScrollPixelsPerUnit(&unitX, &unitY);
        if (unitX <= 0) unitX = 1;
        if (unitY <= 0) unitY = 1;
        int scrollX = desiredViewStart.x / unitX;
        int scrollY = desiredViewStart.y / unitY;
        Scroll(scrollX, scrollY);

        Refresh(false);
    }
    else
    {
        // Default: vertical scroll
        evt.Skip();
    }
}

void SvgCanvas::UpdateVirtualSize()
{
    // Compute bounding box of all items
    int maxX = 0;
    int maxY = 0;
    for (auto& it : m_items)
    {
        if (!it->visible) continue;
        int w = static_cast<int>(std::round(it->baseSize.GetWidth() * m_zoom));
        int h = static_cast<int>(std::round(it->baseSize.GetHeight() * m_zoom));
        maxX = std::max(maxX, it->pos.x + w);
        maxY = std::max(maxY, it->pos.y + h + m_labelHeight);
    }

    // Some minimum size
    maxX += 20;
    maxY += 20;

    SetVirtualSize(maxX, maxY);
}

