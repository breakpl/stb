#include "CustomizeMenuDialog.h"
#include <wx/sizer.h>
#include <wx/stattext.h>

CustomizeMenuDialog::CustomizeMenuDialog(wxWindow* parent, const std::vector<MenuItem>& items,
                                         const std::map<wxString, std::vector<MenuItem>>& subMenus,
                                         const DisplayFlags& displayFlags)
    : wxDialog(parent, wxID_ANY, "Customize Menu",
               wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxSTAY_ON_TOP)
    , m_items(items)
    , m_subMenus(subMenus)
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

    wxStaticText* hint = new wxStaticText(this, wxID_ANY,
        "Check to show, uncheck to hide. Use arrows to reorder:");
    mainSizer->Add(hint, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    // Two-column panel: main list | submenu list
    wxBoxSizer* panelSizer = new wxBoxSizer(wxHORIZONTAL);

    // Left column — main menu
    wxBoxSizer* leftOuter = new wxBoxSizer(wxVERTICAL);
    wxStaticText* mainLabel = new wxStaticText(this, wxID_ANY, "Main menu:");
    leftOuter->Add(mainLabel, 0, wxBOTTOM, 4);

    wxBoxSizer* leftRow = new wxBoxSizer(wxHORIZONTAL);
    m_listBox = new wxCheckListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                   0, nullptr, wxLB_SINGLE);
    m_listBox->SetMinSize(wxSize(190, 200));
    leftRow->Add(m_listBox, 1, wxEXPAND | wxRIGHT, 8);

    wxBoxSizer* mainBtnSizer = new wxBoxSizer(wxVERTICAL);
    m_upBtn   = new wxButton(this, wxID_ANY, wxString::FromUTF8("\xe2\x96\xb2"),
                             wxDefaultPosition, wxSize(36, 36));
    m_downBtn = new wxButton(this, wxID_ANY, wxString::FromUTF8("\xe2\x96\xbc"),
                             wxDefaultPosition, wxSize(36, 36));
    mainBtnSizer->Add(m_upBtn,   0, wxBOTTOM, 6);
    mainBtnSizer->Add(m_downBtn, 0);
    leftRow->Add(mainBtnSizer, 0, wxALIGN_CENTER_VERTICAL);
    leftOuter->Add(leftRow, 1, wxEXPAND);
    panelSizer->Add(leftOuter, 1, wxEXPAND | wxRIGHT, 16);

    // Right column — submenu items
    wxBoxSizer* rightOuter = new wxBoxSizer(wxVERTICAL);
    m_subLabel = new wxStaticText(this, wxID_ANY, "Select a submenu item:");
    rightOuter->Add(m_subLabel, 0, wxBOTTOM, 4);

    wxBoxSizer* rightRow = new wxBoxSizer(wxHORIZONTAL);
    m_subListBox = new wxCheckListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                      0, nullptr, wxLB_SINGLE);
    m_subListBox->SetMinSize(wxSize(190, 200));
    m_subListBox->Enable(false);
    rightRow->Add(m_subListBox, 1, wxEXPAND | wxRIGHT, 8);

    wxBoxSizer* subBtnSizer = new wxBoxSizer(wxVERTICAL);
    m_subUpBtn   = new wxButton(this, wxID_ANY, wxString::FromUTF8("\xe2\x96\xb2"),
                                wxDefaultPosition, wxSize(36, 36));
    m_subDownBtn = new wxButton(this, wxID_ANY, wxString::FromUTF8("\xe2\x96\xbc"),
                                wxDefaultPosition, wxSize(36, 36));
    m_subUpBtn->Enable(false);
    m_subDownBtn->Enable(false);
    subBtnSizer->Add(m_subUpBtn,   0, wxBOTTOM, 6);
    subBtnSizer->Add(m_subDownBtn, 0);
    rightRow->Add(subBtnSizer, 0, wxALIGN_CENTER_VERTICAL);
    rightOuter->Add(rightRow, 1, wxEXPAND);
    panelSizer->Add(rightOuter, 1, wxEXPAND);

    mainSizer->Add(panelSizer, 1, wxEXPAND | wxLEFT | wxRIGHT, 12);

    wxStdDialogButtonSizer* stdBtns = new wxStdDialogButtonSizer();
    stdBtns->AddButton(new wxButton(this, wxID_OK,     "Save"));
    stdBtns->AddButton(new wxButton(this, wxID_CANCEL, "Cancel"));
    stdBtns->Realize();
    mainSizer->Add(stdBtns, 0, wxALL | wxEXPAND, 12);

    SetSizer(mainSizer);
    mainSizer->Fit(this);
    SetMinSize(GetSize());

    m_upBtn->Bind(wxEVT_BUTTON,          &CustomizeMenuDialog::OnMoveUp,    this);
    m_downBtn->Bind(wxEVT_BUTTON,        &CustomizeMenuDialog::OnMoveDown,  this);
    m_listBox->Bind(wxEVT_CHECKLISTBOX,  &CustomizeMenuDialog::OnToggle,    this);
    m_listBox->Bind(wxEVT_LISTBOX,       &CustomizeMenuDialog::OnMainSelect, this);
    m_subUpBtn->Bind(wxEVT_BUTTON,       &CustomizeMenuDialog::OnSubMoveUp,   this);
    m_subDownBtn->Bind(wxEVT_BUTTON,     &CustomizeMenuDialog::OnSubMoveDown, this);
    m_subListBox->Bind(wxEVT_CHECKLISTBOX, &CustomizeMenuDialog::OnSubToggle, this);
    Bind(wxEVT_CLOSE_WINDOW,             &CustomizeMenuDialog::OnClose,     this);

    RefreshList(0);
    UpdateSubPanelForSelection(0);
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

void CustomizeMenuDialog::RefreshSubList(int selectIndex) {
    m_subListBox->Clear();
    if (m_currentSubSection.IsEmpty() || m_subMenus.count(m_currentSubSection) == 0)
        return;
    const auto& subItems = m_subMenus.at(m_currentSubSection);
    for (size_t i = 0; i < subItems.size(); ++i) {
        m_subListBox->Append(subItems[i].name);
        m_subListBox->Check(i, subItems[i].enabled);
    }
    if (selectIndex >= 0 && selectIndex < (int)subItems.size())
        m_subListBox->SetSelection(selectIndex);
}

void CustomizeMenuDialog::UpdateSubPanelForSelection(int mainIdx) {
    if (mainIdx < 0 || mainIdx >= (int)m_items.size() ||
        !m_items[mainIdx].url.StartsWith("submenu:"))
    {
        m_currentSubSection.Clear();
        m_subLabel->SetLabel("Select a submenu item:");
        m_subListBox->Clear();
        m_subListBox->Enable(false);
        m_subUpBtn->Enable(false);
        m_subDownBtn->Enable(false);
        return;
    }

    m_currentSubSection = m_items[mainIdx].url.Mid(8);
    m_subLabel->SetLabel("Submenu: " + m_items[mainIdx].name);
    m_subListBox->Enable(true);
    m_subUpBtn->Enable(true);
    m_subDownBtn->Enable(true);
    RefreshSubList(0);
}

void CustomizeMenuDialog::OnToggle(wxCommandEvent& event) {
    int idx = event.GetInt();
    if (idx < 0 || idx >= (int)m_items.size()) return;
    if (m_items[idx].isSeparator) {
        m_listBox->Check(idx, true);
        return;
    }
    m_items[idx].enabled = m_listBox->IsChecked(idx);
}

void CustomizeMenuDialog::OnMainSelect(wxCommandEvent& event) {
    UpdateSubPanelForSelection(event.GetInt());
}

void CustomizeMenuDialog::OnMoveUp(wxCommandEvent&) {
    int sel = m_listBox->GetSelection();
    if (sel <= 0 || sel >= (int)m_items.size()) return;
    std::swap(m_items[sel], m_items[sel - 1]);
    RefreshList(sel - 1);
    UpdateSubPanelForSelection(sel - 1);
}

void CustomizeMenuDialog::OnMoveDown(wxCommandEvent&) {
    int sel = m_listBox->GetSelection();
    if (sel < 0 || sel >= (int)m_items.size() - 1) return;
    std::swap(m_items[sel], m_items[sel + 1]);
    RefreshList(sel + 1);
    UpdateSubPanelForSelection(sel + 1);
}

void CustomizeMenuDialog::OnSubToggle(wxCommandEvent& event) {
    if (m_currentSubSection.IsEmpty()) return;
    int idx = event.GetInt();
    auto& subItems = m_subMenus[m_currentSubSection];
    if (idx < 0 || idx >= (int)subItems.size()) return;
    subItems[idx].enabled = m_subListBox->IsChecked(idx);
}

void CustomizeMenuDialog::OnSubMoveUp(wxCommandEvent&) {
    if (m_currentSubSection.IsEmpty()) return;
    int sel = m_subListBox->GetSelection();
    auto& subItems = m_subMenus[m_currentSubSection];
    if (sel <= 0 || sel >= (int)subItems.size()) return;
    std::swap(subItems[sel], subItems[sel - 1]);
    RefreshSubList(sel - 1);
}

void CustomizeMenuDialog::OnSubMoveDown(wxCommandEvent&) {
    if (m_currentSubSection.IsEmpty()) return;
    int sel = m_subListBox->GetSelection();
    auto& subItems = m_subMenus[m_currentSubSection];
    if (sel < 0 || sel >= (int)subItems.size() - 1) return;
    std::swap(subItems[sel], subItems[sel + 1]);
    RefreshSubList(sel + 1);
}

void CustomizeMenuDialog::OnClose(wxCloseEvent& event) {
    event.Skip();
}
