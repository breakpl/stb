#include <wx/wx.h>
#include <wx/log.h>
#include <wx/ffile.h>
#include <wx/stdpaths.h>
#include "SprintToolBoxApp.h"

class SprintToolBox : public wxApp {
public:
    virtual bool OnInit();
    virtual int OnExit();
    
private:
    SprintToolBoxApp* m_trayIcon;
    wxFFile* m_logFile;
};

wxIMPLEMENT_APP(SprintToolBox);

bool SprintToolBox::OnInit() {
    // Enable logging to a file next to the executable
    wxString logPath = wxPathOnly(wxStandardPaths::Get().GetExecutablePath()) + "/SprintToolBox.log";
    m_logFile = new wxFFile(logPath, "w");
    if (m_logFile->IsOpened()) {
        wxLog::SetActiveTarget(new wxLogStderr(m_logFile->fp()));
    } else {
        wxLog::SetActiveTarget(new wxLogStderr());
    }
    wxLog::SetLogLevel(wxLOG_Info);

    // Prevent wxWidgets from exiting the event loop when no top-level windows
    // are open – this app lives entirely in the system tray.
    SetExitOnFrameDelete(false);

    // Create the tray icon.  The icon is installed via CallAfter inside
    // SprintToolBoxApp so it runs once the event loop is live.
    m_trayIcon = new SprintToolBoxApp();
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