#include "CustomizeMenuDialog.h"
#include "AddItemDialog.h"
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/msgdlg.h>

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
    m_showBase64Encoder   = new wxCheckBox(this, wxID_ANY, "Base64 Encoder");
    m_showUrlEncoder      = new wxCheckBox(this, wxID_ANY, "URL Encoder");
    m_showJsonFormatter   = new wxCheckBox(this, wxID_ANY, "JSON Formatter");
    m_showUnixTimestamp->SetValue(displayFlags.showUnixTimestamp);
    m_showZuluTimestamp->SetValue(displayFlags.showZuluTimestamp);
    m_showTimeConverter->SetValue(displayFlags.showTimeConverter);
    m_showHexDecConverter->SetValue(displayFlags.showHexDecConverter);
    m_showBase64Encoder->SetValue(displayFlags.showBase64Encoder);
    m_showUrlEncoder->SetValue(displayFlags.showUrlEncoder);
    m_showJsonFormatter->SetValue(displayFlags.showJsonFormatter);
    displaySizer->Add(m_showUnixTimestamp,   0, wxALL, 4);
    displaySizer->Add(m_showZuluTimestamp,   0, wxLEFT | wxRIGHT | wxBOTTOM, 4);
    displaySizer->Add(m_showTimeConverter,   0, wxLEFT | wxRIGHT | wxBOTTOM, 4);
    displaySizer->Add(m_showHexDecConverter, 0, wxLEFT | wxRIGHT | wxBOTTOM, 4);
    displaySizer->Add(m_showBase64Encoder,   0, wxLEFT | wxRIGHT | wxBOTTOM, 4);
    displaySizer->Add(m_showUrlEncoder,      0, wxLEFT | wxRIGHT | wxBOTTOM, 4);
    displaySizer->Add(m_showJsonFormatter,   0, wxLEFT | wxRIGHT | wxBOTTOM, 4);
    mainSizer->Add(displaySizer, 0, wxEXPAND | wxALL, 12);

    wxStaticText* hint = new wxStaticText(this, wxID_ANY,
        "Check to show, uncheck to hide. Use arrows to reorder:");
    mainSizer->Add(hint, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    wxBoxSizer* panelSizer = new wxBoxSizer(wxHORIZONTAL);

    // ── Left column: main menu ────────────────────────────────────────────────
    wxBoxSizer* leftOuter = new wxBoxSizer(wxVERTICAL);
    leftOuter->Add(new wxStaticText(this, wxID_ANY, "Main menu:"), 0, wxBOTTOM, 4);

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
    leftOuter->Add(leftRow, 1, wxEXPAND | wxBOTTOM, 6);

    wxBoxSizer* mainAddRemoveSizer = new wxBoxSizer(wxHORIZONTAL);
    m_addMainBtn    = new wxButton(this, wxID_ANY, "+", wxDefaultPosition, wxSize(32, 28));
    m_removeMainBtn = new wxButton(this, wxID_ANY, wxString::FromUTF8("\xe2\x88\x92"),
                                   wxDefaultPosition, wxSize(32, 28));
    mainAddRemoveSizer->Add(m_addMainBtn,    0, wxRIGHT, 4);
    mainAddRemoveSizer->Add(m_removeMainBtn, 0);
    leftOuter->Add(mainAddRemoveSizer, 0);
    panelSizer->Add(leftOuter, 1, wxEXPAND | wxRIGHT, 16);

    // ── Right column: submenu items ───────────────────────────────────────────
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
    rightOuter->Add(rightRow, 1, wxEXPAND | wxBOTTOM, 6);

    wxBoxSizer* subAddRemoveSizer = new wxBoxSizer(wxHORIZONTAL);
    m_addSubBtn    = new wxButton(this, wxID_ANY, "+", wxDefaultPosition, wxSize(32, 28));
    m_removeSubBtn = new wxButton(this, wxID_ANY, wxString::FromUTF8("\xe2\x88\x92"),
                                  wxDefaultPosition, wxSize(32, 28));
    m_addSubBtn->Enable(false);
    m_removeSubBtn->Enable(false);
    subAddRemoveSizer->Add(m_addSubBtn,    0, wxRIGHT, 4);
    subAddRemoveSizer->Add(m_removeSubBtn, 0);
    rightOuter->Add(subAddRemoveSizer, 0);
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

    m_upBtn->Bind(wxEVT_BUTTON,            &CustomizeMenuDialog::OnMoveUp,        this);
    m_downBtn->Bind(wxEVT_BUTTON,          &CustomizeMenuDialog::OnMoveDown,      this);
    m_addMainBtn->Bind(wxEVT_BUTTON,       &CustomizeMenuDialog::OnAddMainItem,   this);
    m_removeMainBtn->Bind(wxEVT_BUTTON,    &CustomizeMenuDialog::OnRemoveMainItem, this);
    m_listBox->Bind(wxEVT_CHECKLISTBOX,    &CustomizeMenuDialog::OnToggle,        this);
    m_listBox->Bind(wxEVT_LISTBOX,         &CustomizeMenuDialog::OnMainSelect,    this);

    m_subUpBtn->Bind(wxEVT_BUTTON,         &CustomizeMenuDialog::OnSubMoveUp,     this);
    m_subDownBtn->Bind(wxEVT_BUTTON,       &CustomizeMenuDialog::OnSubMoveDown,   this);
    m_addSubBtn->Bind(wxEVT_BUTTON,        &CustomizeMenuDialog::OnAddSubItem,    this);
    m_removeSubBtn->Bind(wxEVT_BUTTON,     &CustomizeMenuDialog::OnRemoveSubItem, this);
    m_subListBox->Bind(wxEVT_CHECKLISTBOX, &CustomizeMenuDialog::OnSubToggle,     this);

    Bind(wxEVT_CLOSE_WINDOW, &CustomizeMenuDialog::OnClose, this);

    RefreshList(0);
    UpdateSubPanelForSelection(0);
    Centre();
}

DisplayFlags CustomizeMenuDialog::GetDisplayFlags() const {
    return {
        m_showUnixTimestamp->GetValue(),
        m_showZuluTimestamp->GetValue(),
        m_showTimeConverter->GetValue(),
        m_showHexDecConverter->GetValue(),
        m_showBase64Encoder->GetValue(),
        m_showUrlEncoder->GetValue(),
        m_showJsonFormatter->GetValue()
    };
}

wxString CustomizeMenuDialog::ItemLabel(const MenuItem& item) const {
    if (item.isSeparator)               return "--- separator ---";
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
        m_subListBox->Append(ItemLabel(subItems[i]));
        m_subListBox->Check(i, subItems[i].isSeparator || subItems[i].enabled);
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
        m_addSubBtn->Enable(false);
        m_removeSubBtn->Enable(false);
        return;
    }

    m_currentSubSection = m_items[mainIdx].url.Mid(8);
    m_subLabel->SetLabel("Submenu: " + m_items[mainIdx].name);
    m_subListBox->Enable(true);
    m_subUpBtn->Enable(true);
    m_subDownBtn->Enable(true);
    m_addSubBtn->Enable(true);
    m_removeSubBtn->Enable(true);
    RefreshSubList(0);
}

// ── Main list handlers ────────────────────────────────────────────────────────

void CustomizeMenuDialog::OnToggle(wxCommandEvent& event) {
    int idx = event.GetInt();
    if (idx < 0 || idx >= (int)m_items.size()) return;
    if (m_items[idx].isSeparator) { m_listBox->Check(idx, true); return; }
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

void CustomizeMenuDialog::OnAddMainItem(wxCommandEvent&) {
    AddItemDialog dlg(this, AddItemDialog::Mode::Main);
    if (dlg.ShowModal() != wxID_OK) return;

    MenuItem item = dlg.GetItem();
    if (item.url.StartsWith("submenu:")) {
        wxString section = item.url.Mid(8);
        if (m_subMenus.count(section) == 0)
            m_subMenus[section] = {};
    }

    int sel = m_listBox->GetSelection();
    int insertAt = (sel >= 0) ? sel + 1 : (int)m_items.size();
    m_items.insert(m_items.begin() + insertAt, item);
    RefreshList(insertAt);
    UpdateSubPanelForSelection(insertAt);
}

void CustomizeMenuDialog::OnRemoveMainItem(wxCommandEvent&) {
    int sel = m_listBox->GetSelection();
    if (sel < 0 || sel >= (int)m_items.size()) return;

    wxString label = ItemLabel(m_items[sel]);
    if (wxMessageBox(wxString::Format("Remove \"%s\"?", label),
                     "Confirm removal", wxYES_NO | wxICON_QUESTION, this) != wxYES)
        return;

    m_items.erase(m_items.begin() + sel);
    int newSel = (sel < (int)m_items.size()) ? sel : sel - 1;
    RefreshList(newSel);
    UpdateSubPanelForSelection(newSel);
}

// ── Submenu list handlers ─────────────────────────────────────────────────────

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

void CustomizeMenuDialog::OnAddSubItem(wxCommandEvent&) {
    if (m_currentSubSection.IsEmpty()) return;

    AddItemDialog dlg(this, AddItemDialog::Mode::Sub);
    if (dlg.ShowModal() != wxID_OK) return;

    auto& subItems = m_subMenus[m_currentSubSection];
    MenuItem item = dlg.GetItem();
    int sel = m_subListBox->GetSelection();
    int insertAt = (sel >= 0) ? sel + 1 : (int)subItems.size();
    subItems.insert(subItems.begin() + insertAt, item);
    RefreshSubList(insertAt);
}

void CustomizeMenuDialog::OnRemoveSubItem(wxCommandEvent&) {
    if (m_currentSubSection.IsEmpty()) return;
    int sel = m_subListBox->GetSelection();
    auto& subItems = m_subMenus[m_currentSubSection];
    if (sel < 0 || sel >= (int)subItems.size()) return;

    if (wxMessageBox(wxString::Format("Remove \"%s\"?", ItemLabel(subItems[sel])),
                     "Confirm removal", wxYES_NO | wxICON_QUESTION, this) != wxYES)
        return;

    subItems.erase(subItems.begin() + sel);
    int newSel = (sel < (int)subItems.size()) ? sel : sel - 1;
    RefreshSubList(newSel);
}

void CustomizeMenuDialog::OnClose(wxCloseEvent& event) {
    event.Skip();
}
