#include "Base64Dialog.h"
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/base64.h>

wxBEGIN_EVENT_TABLE(Base64Dialog, wxDialog)
    EVT_CLOSE(Base64Dialog::OnClose)
wxEND_EVENT_TABLE()

Base64Dialog::Base64Dialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, "Base64 Encoder",
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

    wxStaticText* encodedLabel = new wxStaticText(this, wxID_ANY, "Base64:");
    mainSizer->Add(encodedLabel, 0, wxALL, 10);

    m_encodedField = new wxTextCtrl(this, wxID_ANY, "",
                                     wxDefaultPosition, wxDefaultSize);
    m_encodedField->SetHint("Enter base64...");
    mainSizer->Add(m_encodedField, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    SetSizer(mainSizer);

    m_plainField->Bind(wxEVT_TEXT, &Base64Dialog::OnPlainChanged, this);
    m_encodedField->Bind(wxEVT_TEXT, &Base64Dialog::OnEncodedChanged, this);

    Centre();
}

void Base64Dialog::OnPlainChanged(wxCommandEvent& event) {
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

    wxScopedCharBuffer utf8 = text.utf8_str();
    wxString encoded = wxBase64Encode(utf8.data(), utf8.length());

    m_updating = true;
    m_encodedField->SetValue(encoded);
    m_encodedField->SetBackgroundColour(wxColour(200, 200, 200));
    m_plainField->SetBackgroundColour(wxNullColour);
    m_updating = false;

    Refresh();
}

void Base64Dialog::OnEncodedChanged(wxCommandEvent& event) {
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

    wxMemoryBuffer buf = wxBase64Decode(text, wxBase64DecodeMode_SkipWS, nullptr);
    if (buf.GetDataLen() > 0 || text.Trim().IsEmpty()) {
        wxString decoded = wxString::FromUTF8(
            static_cast<const char*>(buf.GetData()), buf.GetDataLen());

        m_updating = true;
        m_plainField->SetValue(decoded);
        m_plainField->SetBackgroundColour(wxColour(200, 200, 200));
        m_encodedField->SetBackgroundColour(wxNullColour);
        m_updating = false;
    } else {
        m_updating = true;
        m_plainField->SetValue("Invalid base64");
        m_plainField->SetBackgroundColour(wxColour(255, 224, 224));
        m_encodedField->SetBackgroundColour(wxNullColour);
        m_updating = false;
    }

    Refresh();
}

void Base64Dialog::OnClose(wxCloseEvent& event) {
    Hide();
}
