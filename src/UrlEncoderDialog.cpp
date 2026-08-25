#include "UrlEncoderDialog.h"
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/uri.h>

wxBEGIN_EVENT_TABLE(UrlEncoderDialog, wxDialog)
    EVT_CLOSE(UrlEncoderDialog::OnClose)
wxEND_EVENT_TABLE()

static wxString PercentEncode(const wxString& input) {
    wxString result;
    wxScopedCharBuffer utf8 = input.utf8_str();
    const char* p = utf8.data();
    while (*p) {
        unsigned char c = static_cast<unsigned char>(*p++);
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            result += static_cast<char>(c);
        } else {
            result += wxString::Format("%%%02X", c);
        }
    }
    return result;
}

static wxString PercentDecode(const wxString& input) {
    wxString result;
    size_t len = input.length();
    for (size_t i = 0; i < len; ++i) {
        wxChar c = input[i];
        if (c == '%' && i + 2 < len) {
            wxString hex = input.Mid(i + 1, 2);
            unsigned long val;
            if (hex.ToULong(&val, 16)) {
                result += static_cast<char>(val);
                i += 2;
                continue;
            }
        } else if (c == '+') {
            result += ' ';
            continue;
        }
        result += c;
    }
    return result;
}

UrlEncoderDialog::UrlEncoderDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, "URL Encoder",
               wxDefaultPosition, wxSize(460, 220),
               wxDEFAULT_DIALOG_STYLE | wxSTAY_ON_TOP)
    , m_updating(false)
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText* plainLabel = new wxStaticText(this, wxID_ANY, "Plain text:");
    mainSizer->Add(plainLabel, 0, wxALL, 10);

    m_plainField = new wxTextCtrl(this, wxID_ANY, "",
                                   wxDefaultPosition, wxDefaultSize);
    m_plainField->SetHint("Enter plain text...");
    mainSizer->Add(m_plainField, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    wxStaticText* encodedLabel = new wxStaticText(this, wxID_ANY, "URL encoded:");
    mainSizer->Add(encodedLabel, 0, wxALL, 10);

    m_encodedField = new wxTextCtrl(this, wxID_ANY, "",
                                     wxDefaultPosition, wxDefaultSize);
    m_encodedField->SetHint("Enter URL-encoded text...");
    mainSizer->Add(m_encodedField, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    SetSizer(mainSizer);

    m_plainField->Bind(wxEVT_TEXT, &UrlEncoderDialog::OnPlainChanged, this);
    m_encodedField->Bind(wxEVT_TEXT, &UrlEncoderDialog::OnEncodedChanged, this);

    Centre();
}

void UrlEncoderDialog::OnPlainChanged(wxCommandEvent& event) {
    if (m_updating) return;

    wxString text = m_plainField->GetValue();

    if (text.IsEmpty()) {
        m_updating = true;
        m_encodedField->SetValue("");
        m_encodedField->SetBackgroundColour(wxNullColour);
        m_plainField->SetBackgroundColour(wxNullColour);
        m_updating = false;
        return;
    }

    m_updating = true;
    m_encodedField->SetValue(PercentEncode(text));
    m_encodedField->SetBackgroundColour(wxColour(200, 200, 200));
    m_plainField->SetBackgroundColour(wxNullColour);
    m_updating = false;

    Refresh();
}

void UrlEncoderDialog::OnEncodedChanged(wxCommandEvent& event) {
    if (m_updating) return;

    wxString text = m_encodedField->GetValue();

    if (text.IsEmpty()) {
        m_updating = true;
        m_plainField->SetValue("");
        m_plainField->SetBackgroundColour(wxNullColour);
        m_encodedField->SetBackgroundColour(wxNullColour);
        m_updating = false;
        return;
    }

    m_updating = true;
    m_plainField->SetValue(PercentDecode(text));
    m_plainField->SetBackgroundColour(wxColour(200, 200, 200));
    m_encodedField->SetBackgroundColour(wxNullColour);
    m_updating = false;

    Refresh();
}

void UrlEncoderDialog::OnClose(wxCloseEvent& event) {
    Hide();
}
