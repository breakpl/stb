#include "TimeConverterDialog.h"
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <ctime>
#include <cstdio>
#include <cstring>

// Portable UTC→time_t helper
#ifdef _WIN32
#  define timegm _mkgmtime
#endif

wxBEGIN_EVENT_TABLE(TimeConverterDialog, wxDialog)
    EVT_CLOSE(TimeConverterDialog::OnClose)
wxEND_EVENT_TABLE()

TimeConverterDialog::TimeConverterDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, "Time Converter",
               wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxSTAY_ON_TOP)
    , m_updating(false)
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    wxFlexGridSizer* grid = new wxFlexGridSizer(3, 2, 10, 12);
    grid->AddGrowableCol(1, 1);

    // --- Local datetime ---
    grid->Add(new wxStaticText(this, wxID_ANY, "Local (YYYY-MM-DD hh:mm:ss):"),
              0, wxALIGN_CENTER_VERTICAL);
    m_localField = new wxTextCtrl(this, wxID_ANY, "",
                                   wxDefaultPosition, wxSize(260, -1));
    m_localField->SetHint("YYYY-MM-DD HH:MM:SS");
    grid->Add(m_localField, 1, wxEXPAND);

    // --- Zulu / UTC ---
    grid->Add(new wxStaticText(this, wxID_ANY, "Zulu / UTC:"),
              0, wxALIGN_CENTER_VERTICAL);
    m_zuluField = new wxTextCtrl(this, wxID_ANY, "",
                                  wxDefaultPosition, wxSize(260, -1));
    m_zuluField->SetHint("YYYY-MM-DDTHH:MM:SSZ");
    grid->Add(m_zuluField, 1, wxEXPAND);

    // --- Unix timestamp ---
    grid->Add(new wxStaticText(this, wxID_ANY, "Unix Timestamp:"),
              0, wxALIGN_CENTER_VERTICAL);
    m_unixField = new wxTextCtrl(this, wxID_ANY, "",
                                  wxDefaultPosition, wxSize(260, -1));
    m_unixField->SetHint("seconds since epoch");
    grid->Add(m_unixField, 1, wxEXPAND);

    mainSizer->Add(grid, 1, wxEXPAND | wxALL, 16);
    SetSizer(mainSizer);
    mainSizer->Fit(this);

    // Bind text-change events
    m_localField->Bind(wxEVT_TEXT, &TimeConverterDialog::OnLocalChanged, this);
    m_zuluField ->Bind(wxEVT_TEXT, &TimeConverterDialog::OnZuluChanged,  this);
    m_unixField ->Bind(wxEVT_TEXT, &TimeConverterDialog::OnUnixChanged,  this);

    Centre();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void TimeConverterDialog::ResetToCurrentTime() {
    UpdateFromTimestamp(time(nullptr), nullptr);
}

// ---------------------------------------------------------------------------
// Core update logic
// ---------------------------------------------------------------------------

void TimeConverterDialog::UpdateFromTimestamp(time_t ts, wxTextCtrl* source) {
    m_updating = true;
    char buf[64];

    // Local time field
    if (source != m_localField) {
        struct tm* lt = localtime(&ts);
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", lt);
        m_localField->ChangeValue(buf);
        m_localField->SetBackgroundColour(wxNullColour);
        m_localField->Refresh();
    }

    // Zulu / UTC field
    if (source != m_zuluField) {
        struct tm* ut = gmtime(&ts);
        strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", ut);
        m_zuluField->ChangeValue(buf);
        m_zuluField->SetBackgroundColour(wxNullColour);
        m_zuluField->Refresh();
    }

    // Unix timestamp field
    if (source != m_unixField) {
        m_unixField->ChangeValue(wxString::Format("%lld", (long long)ts));
        m_unixField->SetBackgroundColour(wxNullColour);
        m_unixField->Refresh();
    }

    if (source) {
        source->SetBackgroundColour(wxNullColour);
        source->Refresh();
    }

    m_updating = false;
    Refresh();
}

// ---------------------------------------------------------------------------
// Event handlers
// ---------------------------------------------------------------------------

void TimeConverterDialog::OnLocalChanged(wxCommandEvent& /*event*/) {
    if (m_updating) return;

    wxString text = m_localField->GetValue().Trim();

    if (text.IsEmpty()) {
        m_updating = true;
        m_zuluField->ChangeValue("");
        m_unixField->ChangeValue("");
        m_localField->SetBackgroundColour(wxNullColour); m_localField->Refresh();
        m_zuluField ->SetBackgroundColour(wxNullColour); m_zuluField ->Refresh();
        m_unixField ->SetBackgroundColour(wxNullColour); m_unixField ->Refresh();
        m_updating = false;
        return;
    }

    int y, mo, d, h, mi, s;
    if (sscanf(text.mb_str(), "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) == 6) {
        struct tm t = {};
        t.tm_year = y - 1900;
        t.tm_mon  = mo - 1;
        t.tm_mday = d;
        t.tm_hour = h;
        t.tm_min  = mi;
        t.tm_sec  = s;
        t.tm_isdst = -1;
        time_t ts = mktime(&t);
        if (ts != (time_t)-1) {
            UpdateFromTimestamp(ts, m_localField);
            return;
        }
    }

    // Parse failed — mark all fields red
    m_updating = true;
    m_zuluField->ChangeValue("Invalid");
    m_unixField->ChangeValue("Invalid");
    m_localField->SetBackgroundColour(wxColour(255, 210, 210)); m_localField->Refresh();
    m_zuluField ->SetBackgroundColour(wxColour(255, 210, 210)); m_zuluField ->Refresh();
    m_unixField ->SetBackgroundColour(wxColour(255, 210, 210)); m_unixField ->Refresh();
    m_updating = false;
    Refresh();
}

void TimeConverterDialog::OnZuluChanged(wxCommandEvent& /*event*/) {
    if (m_updating) return;

    wxString text = m_zuluField->GetValue().Trim();

    if (text.IsEmpty()) {
        m_updating = true;
        m_localField->ChangeValue("");
        m_unixField ->ChangeValue("");
        m_localField->SetBackgroundColour(wxNullColour); m_localField->Refresh();
        m_zuluField ->SetBackgroundColour(wxNullColour); m_zuluField ->Refresh();
        m_unixField ->SetBackgroundColour(wxNullColour); m_unixField ->Refresh();
        m_updating = false;
        return;
    }

    // Accept: YYYY-MM-DDTHH:MM:SSZ  /  YYYY-MM-DDTHH:MM:SS  /  YYYY-MM-DD HH:MM:SSZ
    int y, mo, d, h, mi, s;
    bool ok = (sscanf(text.mb_str(), "%d-%d-%dT%d:%d:%dZ", &y, &mo, &d, &h, &mi, &s) == 6)
           || (sscanf(text.mb_str(), "%d-%d-%dT%d:%d:%d",  &y, &mo, &d, &h, &mi, &s) == 6)
           || (sscanf(text.mb_str(), "%d-%d-%d %d:%d:%dZ", &y, &mo, &d, &h, &mi, &s) == 6);

    if (ok) {
        struct tm t = {};
        t.tm_year = y - 1900;
        t.tm_mon  = mo - 1;
        t.tm_mday = d;
        t.tm_hour = h;
        t.tm_min  = mi;
        t.tm_sec  = s;
        time_t ts = timegm(&t);
        if (ts != (time_t)-1) {
            UpdateFromTimestamp(ts, m_zuluField);
            return;
        }
    }

    m_updating = true;
    m_localField->ChangeValue("Invalid");
    m_unixField ->ChangeValue("Invalid");
    m_zuluField ->SetBackgroundColour(wxColour(255, 210, 210)); m_zuluField ->Refresh();
    m_localField->SetBackgroundColour(wxColour(255, 210, 210)); m_localField->Refresh();
    m_unixField ->SetBackgroundColour(wxColour(255, 210, 210)); m_unixField ->Refresh();
    m_updating = false;
    Refresh();
}

void TimeConverterDialog::OnUnixChanged(wxCommandEvent& /*event*/) {
    if (m_updating) return;

    wxString text = m_unixField->GetValue().Trim();

    if (text.IsEmpty()) {
        m_updating = true;
        m_localField->ChangeValue("");
        m_zuluField ->ChangeValue("");
        m_localField->SetBackgroundColour(wxNullColour); m_localField->Refresh();
        m_zuluField ->SetBackgroundColour(wxNullColour); m_zuluField ->Refresh();
        m_unixField ->SetBackgroundColour(wxNullColour); m_unixField ->Refresh();
        m_updating = false;
        return;
    }

    long long val;
    if (text.ToLongLong(&val) && val >= 0) {
        UpdateFromTimestamp((time_t)val, m_unixField);
    } else {
        m_updating = true;
        m_localField->ChangeValue("Invalid");
        m_zuluField ->ChangeValue("Invalid");
        m_unixField ->SetBackgroundColour(wxColour(255, 210, 210)); m_unixField ->Refresh();
        m_localField->SetBackgroundColour(wxColour(255, 210, 210)); m_localField->Refresh();
        m_zuluField ->SetBackgroundColour(wxColour(255, 210, 210)); m_zuluField ->Refresh();
        m_updating = false;
        Refresh();
    }
}

void TimeConverterDialog::OnClose(wxCloseEvent& /*event*/) {
    // Hide instead of destroy so it can be shown again
    Hide();
}
