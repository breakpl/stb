#include "AddItemDialog.h"
#include <wx/sizer.h>
#include <wx/stattext.h>

AddItemDialog::AddItemDialog(wxWindow* parent, Mode mode)
    : wxDialog(parent, wxID_ANY,
               mode == Mode::Main ? "Add Menu Item" : "Add Submenu Item",
               wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxSTAY_ON_TOP)
    , m_submenuCheck(nullptr)
    , m_mode(mode)
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    wxFlexGridSizer* grid = new wxFlexGridSizer(2, 2, 8, 8);
    grid->AddGrowableCol(1);

    grid->Add(new wxStaticText(this, wxID_ANY, "Name:"), 0, wxALIGN_CENTER_VERTICAL);
    m_nameCtrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                                wxDefaultPosition, wxSize(280, -1));
    grid->Add(m_nameCtrl, 1, wxEXPAND);

    m_urlLabel = new wxStaticText(this, wxID_ANY, "URL:");
    grid->Add(m_urlLabel, 0, wxALIGN_CENTER_VERTICAL);
    m_urlCtrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                               wxDefaultPosition, wxSize(280, -1));
    grid->Add(m_urlCtrl, 1, wxEXPAND);

    mainSizer->Add(grid, 0, wxEXPAND | wxALL, 12);

    m_separatorCheck = new wxCheckBox(this, wxID_ANY, "This is a separator");
    mainSizer->Add(m_separatorCheck, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    if (mode == Mode::Main) {
        m_submenuCheck = new wxCheckBox(this, wxID_ANY, "This is a submenu");
        mainSizer->Add(m_submenuCheck, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);
    }

    wxStdDialogButtonSizer* stdBtns = new wxStdDialogButtonSizer();
    stdBtns->AddButton(new wxButton(this, wxID_OK, "Add"));
    stdBtns->AddButton(new wxButton(this, wxID_CANCEL, "Cancel"));
    stdBtns->Realize();
    mainSizer->Add(stdBtns, 0, wxALL | wxEXPAND, 12);

    SetSizer(mainSizer);
    mainSizer->Fit(this);
    SetMinSize(GetSize());

    m_separatorCheck->Bind(wxEVT_CHECKBOX, &AddItemDialog::OnSeparatorToggle, this);
    if (m_submenuCheck)
        m_submenuCheck->Bind(wxEVT_CHECKBOX, &AddItemDialog::OnSubmenuToggle, this);
    Bind(wxEVT_BUTTON, &AddItemDialog::OnOK, this, wxID_OK);

    m_nameCtrl->SetFocus();
    Centre();
}

void AddItemDialog::OnSeparatorToggle(wxCommandEvent&) {
    bool sep = m_separatorCheck->GetValue();
    m_nameCtrl->Enable(!sep);
    m_urlCtrl->Enable(!sep);
    if (m_submenuCheck) m_submenuCheck->Enable(!sep);
}

void AddItemDialog::OnSubmenuToggle(wxCommandEvent&) {
    bool sub = m_submenuCheck && m_submenuCheck->GetValue();
    m_urlLabel->SetLabel(sub ? "Section:" : "URL:");
    m_separatorCheck->Enable(!sub);
}

void AddItemDialog::OnOK(wxCommandEvent& event) {
    bool isSep = m_separatorCheck->GetValue();
    bool isSub = m_submenuCheck && m_submenuCheck->GetValue();
    if (!isSep) {
        if (m_nameCtrl->GetValue().Trim().IsEmpty()) {
            wxMessageBox("Name cannot be empty.", "Validation", wxOK | wxICON_WARNING, this);
            return;
        }
        if (m_urlCtrl->GetValue().Trim().IsEmpty()) {
            wxString what = isSub ? "Section name" : "URL";
            wxMessageBox(what + " cannot be empty.", "Validation", wxOK | wxICON_WARNING, this);
            return;
        }
    }
    event.Skip();
}

MenuItem AddItemDialog::GetItem() const {
    if (m_separatorCheck->GetValue())
        return MenuItem::Separator();
    wxString name = m_nameCtrl->GetValue().Trim();
    wxString url  = m_urlCtrl->GetValue().Trim();
    if (m_submenuCheck && m_submenuCheck->GetValue())
        return MenuItem(name, "submenu:" + url, true);
    return MenuItem(name, url, true);
}
