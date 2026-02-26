#ifndef TRAYICONAPP_H
#define TRAYICONAPP_H

#include <wx/wx.h>
#include <wx/taskbar.h>
#include <wx/timer.h>
#include "JiraService.h"

class ConverterDialog;

class TrayIconApp : public wxTaskBarIcon {
public:
    TrayIconApp();
    virtual ~TrayIconApp();
    
    // Create the popup menu
    virtual wxMenu* CreatePopupMenu() override;
    
private:
    void OnUpdateTimestamps(wxCommandEvent& event);
    void OnCopyUnixTimestamp(wxCommandEvent& event);
    void OnCopyZuluTimestamp(wxCommandEvent& event);
    void OnOpenConverter(wxCommandEvent& event);
    void OnQuit(wxCommandEvent& event);
    void OnMenuOpen(wxMenuEvent& event);
    void OnSprintUpdateTimer(wxTimerEvent& event);
    
    void UpdateTrayIcon(const wxString& text, int daysPassed = -1);
    void UpdateTimestamps();
    void UpdateSprint();
    void OnSprintFetched(const SprintInfo& sprint);
    void OnSprintError(const wxString& error, const wxString& errorCode);
    
    wxString m_unixTimestamp;
    wxString m_zuluTimestamp;
    ConverterDialog* m_converterDialog;
    JiraService* m_jiraService;
    wxTimer* m_sprintUpdateTimer;
    
    wxDECLARE_EVENT_TABLE();
};

// Custom event IDs
enum {
    ID_COPY_UNIX = wxID_HIGHEST + 1,
    ID_COPY_ZULU,
    ID_OPEN_CONVERTER,
    ID_QUIT,
    ID_SPRINT_TIMER
};

#endif // TRAYICONAPP_H
