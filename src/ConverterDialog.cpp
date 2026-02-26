#include "ConverterDialog.h"
#include <wx/sizer.h>
#include <wx/stattext.h>

wxBEGIN_EVENT_TABLE(ConverterDialog, wxDialog)
    EVT_CLOSE(ConverterDialog::OnClose)
wxEND_EVENT_TABLE()

ConverterDialog::ConverterDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, "Hex/Dec Converter", 
               wxDefaultPosition, wxSize(400, 200),
               wxDEFAULT_DIALOG_STYLE | wxSTAY_ON_TOP)
    , m_updating(false)
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    
    // Decimal field
    wxStaticText* decLabel = new wxStaticText(this, wxID_ANY, "Decimal:");
    mainSizer->Add(decLabel, 0, wxALL, 10);
    
    m_decimalField = new wxTextCtrl(this, wxID_ANY, "", 
                                     wxDefaultPosition, wxDefaultSize);
    m_decimalField->SetHint("Enter decimal number...");
    mainSizer->Add(m_decimalField, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    
    // Hex field
    wxStaticText* hexLabel = new wxStaticText(this, wxID_ANY, "Hexadecimal:");
    mainSizer->Add(hexLabel, 0, wxALL, 10);
    
    m_hexField = new wxTextCtrl(this, wxID_ANY, "",
                                 wxDefaultPosition, wxDefaultSize);
    m_hexField->SetHint("Enter hex number...");
    mainSizer->Add(m_hexField, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    
    SetSizer(mainSizer);
    
    // Bind text change events
    m_decimalField->Bind(wxEVT_TEXT, &ConverterDialog::OnDecimalChanged, this);
    m_hexField->Bind(wxEVT_TEXT, &ConverterDialog::OnHexChanged, this);
    
    Centre();
}

void ConverterDialog::OnDecimalChanged(wxCommandEvent& event) {
    if (m_updating) return;
    
    wxString text = m_decimalField->GetValue();
    
    if (text.IsEmpty()) {
        m_updating = true;
        m_hexField->SetValue("");
        m_hexField->SetBackgroundColour(wxNullColour);
        m_decimalField->SetBackgroundColour(wxNullColour);
        m_updating = false;
        return;
    }
    
    // Try to convert decimal to hex
    unsigned long long decValue;
    if (text.ToULongLong(&decValue)) {
        m_updating = true;
        m_hexField->SetValue(wxString::Format("%llX", decValue));
        m_hexField->SetBackgroundColour(wxColour(200, 200, 200));
        m_decimalField->SetBackgroundColour(wxNullColour);
        m_updating = false;
    } else {
        m_updating = true;
        m_hexField->SetValue("Invalid");
        m_hexField->SetBackgroundColour(wxColour(255, 224, 224));
        m_decimalField->SetBackgroundColour(wxNullColour);
        m_updating = false;
    }
    
    Refresh();
}

void ConverterDialog::OnHexChanged(wxCommandEvent& event) {
    if (m_updating) return;
    
    wxString text = m_hexField->GetValue();
    
    if (text.IsEmpty()) {
        m_updating = true;
        m_decimalField->SetValue("");
        m_decimalField->SetBackgroundColour(wxNullColour);
        m_hexField->SetBackgroundColour(wxNullColour);
        m_updating = false;
        return;
    }
    
    // Try to convert hex to decimal
    unsigned long long hexValue;
    if (text.ToULongLong(&hexValue, 16)) {
        m_updating = true;
        m_decimalField->SetValue(wxString::Format("%llu", hexValue));
        m_decimalField->SetBackgroundColour(wxColour(200, 200, 200));
        m_hexField->SetBackgroundColour(wxNullColour);
        m_updating = false;
    } else {
        m_updating = true;
        m_decimalField->SetValue("Invalid");
        m_decimalField->SetBackgroundColour(wxColour(255, 224, 224));
        m_hexField->SetBackgroundColour(wxNullColour);
        m_updating = false;
    }
    
    Refresh();
}

void ConverterDialog::OnClose(wxCloseEvent& event) {
    // Hide instead of destroy so it can be shown again
    Hide();
}
