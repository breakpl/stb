#include "TimeConverterDialog.h"
#include "TimestampUtils.h"
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <ctime>

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
    m_unixField->SetHint("seconds or milliseconds");
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

    // Local time field
    if (source != m_localField) {
        m_localField->ChangeValue(TimestampUtils::ToLocal(ts));
        m_localField->SetBackgroundColour(wxNullColour);
        m_localField->Refresh();
    }

    // Zulu / UTC field
    if (source != m_zuluField) {
        m_zuluField->ChangeValue(TimestampUtils::ToZulu(ts));
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

    time_t ts = TimestampUtils::FromLocal(text.ToStdString());
    if (ts != (time_t)-1) {
        UpdateFromTimestamp(ts, m_localField);
        return;
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
    time_t ts = TimestampUtils::FromZulu(text.ToStdString());
    if (ts != (time_t)-1) {
        UpdateFromTimestamp(ts, m_zuluField);
        return;
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
        // Auto-detect milliseconds: if value > 10 billion, assume milliseconds
        // (10 billion seconds = year 2286, so any larger value is likely milliseconds)
        time_t ts;
        if (val > 10000000000LL) {
            // Convert milliseconds to seconds
            ts = (time_t)(val / 1000);
        } else {
            ts = (time_t)val;
        }
        UpdateFromTimestamp(ts, m_unixField);
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
