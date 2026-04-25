#ifndef SPRINTTOOLBOXAPP_H
#define SPRINTTOOLBOXAPP_H

#include <wx/wx.h>
#include <wx/taskbar.h>
#include <wx/timer.h>
#include <wx/icon.h>
#include <map>
#include "JiraService.h"

#ifdef _WIN32
#include <windows.h>
#endif

class ConverterDialog;
class TimeConverterDialog;
class Config;

class SprintToolBoxApp : public wxTaskBarIcon {
public:
    SprintToolBoxApp();
    virtual ~SprintToolBoxApp();
    
    // Create the popup menu (returns nullptr – we handle popup via OnTaskBarClick)
    virtual wxMenu* CreatePopupMenu() override;

    // Public for theme change observer access
    void UpdateTrayIcon(const wxString& text, int daysPassed = -1);
    wxString m_currentIconText;
    int m_currentDaysPassed;

    // Public so the macOS click handler can call it
    void ShowContextMenu();

private:
    void OnTaskBarClick(wxTaskBarIconEvent& event);
    wxMenu* BuildPopupMenu();

    void OnCopyUnixTimestamp(wxCommandEvent& event);
    void OnCopyZuluTimestamp(wxCommandEvent& event);
    void OnOpenConverter(wxCommandEvent& event);
    void OnOpenTimeConverter(wxCommandEvent& event);
    void OnQuit(wxCommandEvent& event);
    void OnSprintUpdateTimer(wxTimerEvent& event);
    void OnRetryTimer(wxTimerEvent& event);
    void OnConfigWatchTimer(wxTimerEvent& event);

    void OnDynamicMenuClick(wxCommandEvent& event);
    void OnToggleAutostart(wxCommandEvent& event);

    bool IsAutostartEnabled();
    void SetAutostart(bool enable);

    void UpdateTimestamps();
    void UpdateSprint();
    void FetchPublicSprint();  // Fetch from GitHub public URL
    void OnSprintFetched(const SprintInfo& sprint);
    void OnSprintError(const wxString& error, const wxString& errorCode);
    
    wxString m_unixTimestamp;
    wxString m_zuluTimestamp;
    wxIcon m_trayIconCurrent;   // keeps the HICON alive for the duration of each tray icon display
    bool m_menuShowing;         // guard against re-entrant ShowContextMenu()
    ConverterDialog* m_converterDialog;
    TimeConverterDialog* m_timeConverterDialog;
    JiraService* m_jiraService;
    Config* m_config;
    wxTimer* m_sprintUpdateTimer;
    wxTimer* m_configWatchTimer; // polls INI modification time
    wxTimer* m_retryTimer;      // active during error back-off

    int      m_retryCount;      // attempts made in current retry window
    int      m_retryMaxCount;   // max attempts before giving up
    bool     m_useFallbackMode; // true if using public GitHub URL instead of JIRA
#ifdef __WXOSX__
    void* m_themeObserver;      // NSObject for theme change notifications
    void* m_statusItem;         // NSStatusItem (owned by us, not by wxWidgets)
    void* m_statusItemHandler;  // StatusItemClickHandler target for the button
#endif
#ifdef _WIN32
    HWND m_themeHwnd;       // Hidden window for WM_SETTINGCHANGE on Windows
#endif
    std::map<int, wxString> m_menuUrlMap;  // Maps menu item IDs to URLs
    
    wxDECLARE_EVENT_TABLE();
};

// Custom event IDs
enum {
    ID_COPY_UNIX = wxID_HIGHEST + 1,
    ID_COPY_ZULU,
    ID_OPEN_CONVERTER,
    ID_OPEN_TIME_CONVERTER,
    ID_QUIT,
    ID_SPRINT_TIMER,
    ID_RETRY_TIMER,
    ID_CONFIG_WATCH_TIMER,

    ID_TOGGLE_AUTOSTART,

    ID_DYNAMIC_MENU_START = wxID_HIGHEST + 1000  // Base ID for dynamic menu items
};

#endif // SPRINTTOOLBOXAPP_H
