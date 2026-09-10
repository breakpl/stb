#include "UrlEncoderDialog.h"
#include "UrlEncoderUtils.h"
#include <wx/sizer.h>
#include <wx/stattext.h>

wxBEGIN_EVENT_TABLE(UrlEncoderDialog, wxDialog)
    EVT_CLOSE(UrlEncoderDialog::OnClose)
wxEND_EVENT_TABLE()

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

    const wxScopedCharBuffer utf8 = text.utf8_str();
    std::string encodedStr = UrlEncoderUtils::Encode(std::string(utf8.data(), utf8.length()));

    m_updating = true;
    m_encodedField->SetValue(wxString::FromAscii(encodedStr.c_str()));
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

    const wxScopedCharBuffer utf8Input = text.utf8_str();
    std::string decodedStr = UrlEncoderUtils::Decode(std::string(utf8Input.data(), utf8Input.length()));

    m_updating = true;
    m_plainField->SetValue(wxString::FromUTF8(decodedStr.c_str(), decodedStr.size()));
    m_plainField->SetBackgroundColour(wxColour(200, 200, 200));
    m_encodedField->SetBackgroundColour(wxNullColour);
    m_updating = false;

    Refresh();
}

void UrlEncoderDialog::OnClose(wxCloseEvent& event) {
    Hide();
}
