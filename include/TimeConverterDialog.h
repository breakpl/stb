#ifndef TIMECONVERTERDIALOG_H
#define TIMECONVERTERDIALOG_H

#include <wx/wx.h>
#include <ctime>

class TimeConverterDialog : public wxDialog {
public:
    TimeConverterDialog(wxWindow* parent);

    // Reset all fields to the current system time and restart live ticking.
    void ResetToCurrentTime();

private:
    void OnLocalChanged(wxCommandEvent& event);
    void OnZuluChanged(wxCommandEvent& event);
    void OnUnixChanged(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);

    // Update all fields from a resolved timestamp; source is the field the
    // user typed in (its background stays neutral; others turn grey).
    // Pass source=nullptr to set all fields without any colour highlight.
    void UpdateFromTimestamp(time_t ts, wxTextCtrl* source);

    wxTextCtrl* m_localField;   // YYYY-MM-DD HH:mm:ss  (local time)
    wxTextCtrl* m_zuluField;    // YYYY-MM-DDTHH:MM:SSZ (UTC / Zulu)
    wxTextCtrl* m_unixField;    // Unix epoch (seconds)

    bool        m_updating;     // re-entrancy guard

    wxDECLARE_EVENT_TABLE();
};

#endif // TIMECONVERTERDIALOG_H
