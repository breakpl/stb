#include "Base64Dialog.h"
#include "Base64Utils.h"
#include <wx/sizer.h>
#include <wx/stattext.h>

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

    const wxScopedCharBuffer utf8 = text.utf8_str();
    std::string encodedStr = Base64Utils::Encode(std::string(utf8.data(), utf8.length()));

    m_updating = true;
    m_encodedField->SetValue(wxString::FromAscii(encodedStr.c_str()));
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

    const wxScopedCharBuffer utf8Input = text.utf8_str();
    bool decodeValid = false;
    std::string decodedStr = Base64Utils::Decode(
        std::string(utf8Input.data(), utf8Input.length()), &decodeValid);

    if (decodeValid) {
        wxString decoded = wxString::FromUTF8(decodedStr.c_str(), decodedStr.size());

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
