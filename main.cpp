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
    }

private:
    SvgCanvas* m_canvas;
};

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
