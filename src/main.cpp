#include <wx/wx.h>
#include "SprintToolBoxApp.h"

class SprintToolBox : public wxApp {
public:
    virtual bool OnInit();
    virtual int OnExit();
    
private:
    SprintToolBoxApp* m_trayIcon;
};

wxIMPLEMENT_APP(SprintToolBox);

bool SprintToolBox::OnInit() {
    // Enable logging
    wxLog::SetActiveTarget(new wxLogStderr());
    
    // Create the tray icon
    m_trayIcon = new SprintToolBoxApp();
    
    // Check if tray icon is available
    if (!m_trayIcon->IsIconInstalled()) {
        wxMessageBox("Could not create tray icon!", "SprintToolBox Error", wxICON_ERROR);
        delete m_trayIcon;
        return false;
    }
    
    return true;
}

int SprintToolBox::OnExit() {
    // Clean up tray icon
    if (m_trayIcon) {
        delete m_trayIcon;
        m_trayIcon = nullptr;
    }
    return wxApp::OnExit();
}