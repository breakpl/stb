#include "MainFrame.h"
#include <wx/wx.h>

MainFrame::MainFrame() : wxFrame(nullptr, wxID_ANY, "wxWidgets Learning Project", wxDefaultPosition, wxSize(800, 600)) {
    CreateMenu();
    CreateStatusBar();
}

void MainFrame::CreateMenu() {
    wxMenuBar* menuBar = new wxMenuBar();
    wxMenu* fileMenu = new wxMenu();
    fileMenu->Append(wxID_EXIT, "Exit\tCtrl-Q", "Exit the application");
    menuBar->Append(fileMenu, "File");
    SetMenuBar(menuBar);

    Bind(wxEVT_MENU, &MainFrame::OnExit, this, wxID_EXIT);
}

void MainFrame::OnExit(wxCommandEvent& event) {
    Close(true);
}