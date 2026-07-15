#include "CustomizeMenuDialog.h"
#include <wx/sizer.h>
#include <wx/stattext.h>

CustomizeMenuDialog::CustomizeMenuDialog(wxWindow* parent, const std::vector<MenuItem>& items,
                                         const DisplayFlags& displayFlags)
    : wxDialog(parent, wxID_ANY, "Customize Menu",
               wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxSTAY_ON_TOP)
    , m_items(items)
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    wxStaticBoxSizer* displaySizer = new wxStaticBoxSizer(wxVERTICAL, this, "Built-in items");
    m_showUnixTimestamp   = new wxCheckBox(this, wxID_ANY, "Unix Timestamp");
    m_showZuluTimestamp   = new wxCheckBox(this, wxID_ANY, "Zulu Timestamp");
    m_showTimeConverter   = new wxCheckBox(this, wxID_ANY, "Time Converter");
    m_showHexDecConverter = new wxCheckBox(this, wxID_ANY, "Hex/Dec Converter");
    m_showUnixTimestamp->SetValue(displayFlags.showUnixTimestamp);
    m_showZuluTimestamp->SetValue(displayFlags.showZuluTimestamp);
    m_showTimeConverter->SetValue(displayFlags.showTimeConverter);
    m_showHexDecConverter->SetValue(displayFlags.showHexDecConverter);
    displaySizer->Add(m_showUnixTimestamp,   0, wxALL, 4);
    displaySizer->Add(m_showZuluTimestamp,   0, wxLEFT | wxRIGHT | wxBOTTOM, 4);
    displaySizer->Add(m_showTimeConverter,   0, wxLEFT | wxRIGHT | wxBOTTOM, 4);
    displaySizer->Add(m_showHexDecConverter, 0, wxLEFT | wxRIGHT | wxBOTTOM, 4);
    mainSizer->Add(displaySizer, 0, wxEXPAND | wxALL, 12);

    wxStaticText* label = new wxStaticText(this, wxID_ANY,
        "Check to show, uncheck to hide. Use arrows to reorder:");
    mainSizer->Add(label, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    wxBoxSizer* rowSizer = new wxBoxSizer(wxHORIZONTAL);

    m_listBox = new wxCheckListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                   0, nullptr, wxLB_SINGLE);
    m_listBox->SetMinSize(wxSize(-1, 200));
    rowSizer->Add(m_listBox, 1, wxEXPAND | wxRIGHT, 8);

    wxBoxSizer* btnSizer = new wxBoxSizer(wxVERTICAL);
    m_upBtn   = new wxButton(this, wxID_ANY, wxString::FromUTF8("\xe2\x96\xb2"),
                             wxDefaultPosition, wxSize(36, 36));
    m_downBtn = new wxButton(this, wxID_ANY, wxString::FromUTF8("\xe2\x96\xbc"),
                             wxDefaultPosition, wxSize(36, 36));
    btnSizer->Add(m_upBtn,   0, wxBOTTOM, 6);
    btnSizer->Add(m_downBtn, 0);
    rowSizer->Add(btnSizer, 0, wxALIGN_CENTER_VERTICAL);

    mainSizer->Add(rowSizer, 1, wxEXPAND | wxLEFT | wxRIGHT, 12);

    wxStdDialogButtonSizer* stdBtns = new wxStdDialogButtonSizer();
    stdBtns->AddButton(new wxButton(this, wxID_OK,     "Save"));
    stdBtns->AddButton(new wxButton(this, wxID_CANCEL, "Cancel"));
    stdBtns->Realize();
    mainSizer->Add(stdBtns, 0, wxALL | wxEXPAND, 12);

    SetSizer(mainSizer);
    mainSizer->Fit(this);
    SetMinSize(GetSize());

    m_upBtn->Bind(wxEVT_BUTTON,        &CustomizeMenuDialog::OnMoveUp,   this);
    m_downBtn->Bind(wxEVT_BUTTON,      &CustomizeMenuDialog::OnMoveDown, this);
    m_listBox->Bind(wxEVT_CHECKLISTBOX, &CustomizeMenuDialog::OnToggle,   this);
    Bind(wxEVT_CLOSE_WINDOW,           &CustomizeMenuDialog::OnClose,    this);

    RefreshList(0);
    Centre();
}

DisplayFlags CustomizeMenuDialog::GetDisplayFlags() const {
    return {
        m_showUnixTimestamp->GetValue(),
        m_showZuluTimestamp->GetValue(),
        m_showTimeConverter->GetValue(),
        m_showHexDecConverter->GetValue()
    };
}

wxString CustomizeMenuDialog::ItemLabel(const MenuItem& item) const {
    if (item.isSeparator)              return "--- separator ---";
    if (item.url.StartsWith("submenu:")) return "> " + item.name;
    return item.name;
}

void CustomizeMenuDialog::RefreshList(int selectIndex) {
    m_listBox->Clear();
    for (size_t i = 0; i < m_items.size(); ++i) {
        m_listBox->Append(ItemLabel(m_items[i]));
        m_listBox->Check(i, m_items[i].isSeparator || m_items[i].enabled);
    }
    if (selectIndex >= 0 && selectIndex < (int)m_items.size())
        m_listBox->SetSelection(selectIndex);
}

void CustomizeMenuDialog::OnToggle(wxCommandEvent& event) {
    int idx = event.GetInt();
    if (idx < 0 || idx >= (int)m_items.size()) return;
    if (m_items[idx].isSeparator) {
        m_listBox->Check(idx, true);  // separators cannot be disabled
        return;
    }
    m_items[idx].enabled = m_listBox->IsChecked(idx);
}

void CustomizeMenuDialog::OnMoveUp(wxCommandEvent&) {
    int sel = m_listBox->GetSelection();
    if (sel <= 0 || sel >= (int)m_items.size()) return;
    std::swap(m_items[sel], m_items[sel - 1]);
    RefreshList(sel - 1);
}

void CustomizeMenuDialog::OnMoveDown(wxCommandEvent&) {
    int sel = m_listBox->GetSelection();
    if (sel < 0 || sel >= (int)m_items.size() - 1) return;
    std::swap(m_items[sel], m_items[sel + 1]);
    RefreshList(sel + 1);
}

void CustomizeMenuDialog::OnClose(wxCloseEvent& event) {
    event.Skip();
}
