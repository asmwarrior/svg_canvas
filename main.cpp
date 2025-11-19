#include <wx/wx.h>
#include "svg_canvas.h"

// Main frame
class MainFrame : public wxFrame
{
public:
    MainFrame()
        : wxFrame(nullptr, wxID_ANY, "SVG Canvas Example", wxDefaultPosition, wxSize(900, 700))
    {
        m_canvas = new SvgCanvas(this);

        // Example: add several SVG files (replace paths with your files)
        // We'll place them vertically with some spacing
        int x = 20;
        int y = 20;
        const int padding = 20;
        const wxSize baseSize(128, 128);

        // Replace these names with your actual SVG files
        m_canvas->AddSvgFile("assets/icon1.svg", wxPoint(x, y), baseSize, "icon1.svg");
        y += baseSize.GetHeight() + padding;
        m_canvas->AddSvgFile("assets/icon2.svg", wxPoint(x, y), baseSize, "icon2.svg");
        y += baseSize.GetHeight() + padding;
        m_canvas->AddSvgFile("assets/icon3.svg", wxPoint(x, y), baseSize, "icon3.svg");
        y += baseSize.GetHeight() + padding;
        m_canvas->AddSvgFile("assets/icon4.svg", wxPoint(x, y), baseSize, "icon4.svg");

        enum
        {
            ID_MODIFY_SVG_COLOR = wxID_HIGHEST + 1,
            ID_MODIFY_SVG_TEXT  = wxID_HIGHEST + 2
        };

        wxMenu* menuEdit = new wxMenu;
        menuEdit->Append(ID_MODIFY_SVG_COLOR,
                         "Modify SVG Color...",
                         "Change the fill color of the selected SVG");
        menuEdit->Append(ID_MODIFY_SVG_TEXT,
                         "Modify SVG Text...",
                         "Change the text content of the selected SVG");

        wxMenuBar* menuBar = new wxMenuBar;
        menuBar->Append(menuEdit, "&Edit");
        SetMenuBar(menuBar);

        Bind(wxEVT_MENU, &MainFrame::OnChangeSvgColor, this, ID_MODIFY_SVG_COLOR);
        Bind(wxEVT_MENU, &MainFrame::OnChangeSvgText, this, ID_MODIFY_SVG_TEXT);

    }

    void OnChangeSvgColor(wxCommandEvent&);
    void OnChangeSvgText(wxCommandEvent&);

private:
    SvgCanvas* m_canvas;
};


void MainFrame::OnChangeSvgColor(wxCommandEvent&)
{
    // get currently selected SVG item
    auto hit = m_canvas->GetSelectedSvg();
    if (!hit)
    {
        wxMessageBox("Please click an SVG first.", "No selection", wxICON_INFORMATION);
        return;
    }

    // Ask for a CSS selector (allow '*' for all)
    wxTextEntryDialog selDlg(this, "Enter CSS selector for elements to change (e.g. '#myid', 'path', 'rect, circle', or '*' for all):",
                             "Select Elements", "*");
    if (selDlg.ShowModal() != wxID_OK) return;
    std::string selector = selDlg.GetValue().ToStdString();
    if (selector.empty()) selector = "*";

    // Ask for a color value
    wxTextEntryDialog colorDlg(this, "Enter CSS color (examples: red, #00ff00, rgb(20,180,90), rgba(255,0,0,0.5))",
                               "Set Fill Color", "#ff0000");
    if (colorDlg.ShowModal() != wxID_OK) return;
    std::string color = colorDlg.GetValue().ToStdString();

    // Access the lunasvg document
    auto doc = hit->svg.GetDocument();
    if (!doc)
    {
        wxMessageBox("SVG document not loaded.", "Error", wxICON_ERROR);
        return;
    }

    // Use querySelectorAll to find matching elements
    try
    {
        auto elems = doc->querySelectorAll(selector);
        if (elems.empty())
        {
            // No matches — inform user and return
            wxMessageBox("No elements matched the selector.", "No match", wxICON_INFORMATION);
            return;
        }

        // Apply the fill attribute to each element
        for (auto &el : elems)
        {
            el.setAttribute("fill", color);
        }
    }
    catch (...)
    {
        // If querySelectorAll throws (very unlikely), fall back to setting document-level property
        // (some builds may differ slightly). Use a safe fallback:
        try {
            doc->applyStyleSheet("* { fill: " + color + " !important; }");
        } catch (...) {
            wxMessageBox("Failed to modify document with DOM API and stylesheet fallback.", "Error", wxICON_ERROR);
            return;
        }
    }

    // Mark dirty and re-render the modified SVG at the current zoom/size so the cache gets updated immediately.
    hit->svg.MarkDirty();

    // Calculate render size (use the item's baseSize and the canvas zoom)
    int w = static_cast<int>(std::round(hit->baseSize.GetWidth() * m_canvas->GetZoom()));
    int h = static_cast<int>(std::round(hit->baseSize.GetHeight() * m_canvas->GetZoom()));
    if (w <= 0) w = 1;
    if (h <= 0) h = 1;

    // Re-render (this updates the SvgImageLuna internal cache)
    wxBitmap newBmp = hit->svg.Render(w, h, m_canvas->GetZoom());
    (void)newBmp; // we don't need to use it here; it's cached inside SvgImageLuna

    // Refresh canvas to show changes
    m_canvas->Refresh(false);
}

void MainFrame::OnChangeSvgText(wxCommandEvent&)
{
    auto hit = m_canvas->GetSelectedSvg();
    if (!hit)
    {
        wxMessageBox("Please click an SVG first.");
        return;
    }

    // Access the SVG document
    auto doc = hit->svg.GetDocument();
    if (!doc)
        return;

    // Find all <text> elements
    auto elements = doc->querySelectorAll("text");
    if (elements.empty())
    {
        wxMessageBox("No <text> elements found in this SVG.");
        return;
    }

    // Prepare choices for user: show current text content for each element
    wxArrayString choices;
    for (size_t i = 0; i < elements.size(); ++i)
    {
        std::string currentText;
        auto children = elements[i].children();
        for (auto& child : children)
        {
            if (child.isTextNode())
            {
                currentText = child.toTextNode().data();
                break;
            }
        }
        choices.Add(wxString::Format("Element %d: %s", (int)i + 1, currentText));
    }

    // Ask user to select element
    wxSingleChoiceDialog selectDlg(this,
        "Select a <text> element to edit:",
        "Choose Text Element",
        choices);

    if (selectDlg.ShowModal() != wxID_OK)
        return;

    int selIndex = selectDlg.GetSelection();
    if (selIndex < 0 || selIndex >= (int)elements.size())
        return;

    // Ask user for new text
    wxTextEntryDialog textDlg(this,
        "Enter new text content:",
        "Edit SVG Text");

    if (textDlg.ShowModal() != wxID_OK)
        return;

    std::string newText = textDlg.GetValue().ToStdString();

    // Update the selected <text> element
    auto children = elements[selIndex].children();
    for (auto& child : children)
    {
        if (child.isTextNode())
        {
            auto textNode = child.toTextNode();
            textNode.setData(newText);
        }
    }

    // Force re-render
    hit->svg.MarkDirty();
    m_canvas->Refresh();
}


// App
class MyApp : public wxApp
{
public:
    bool OnInit() override
    {
        if (!wxApp::OnInit())
            return false;

        MainFrame* f = new MainFrame();
        f->Show();
        return true;
    }
};

wxIMPLEMENT_APP(MyApp);
