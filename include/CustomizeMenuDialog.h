#ifndef CUSTOMIZEMENUDIALOG_H
#define CUSTOMIZEMENUDIALOG_H

#include <wx/wx.h>
#include <wx/checklst.h>
#include <map>
#include <vector>
#include "Config.h"

class CustomizeMenuDialog : public wxDialog {
public:
    CustomizeMenuDialog(wxWindow* parent, const std::vector<MenuItem>& items,
                        const std::map<wxString, std::vector<MenuItem>>& subMenus,
                        const DisplayFlags& displayFlags);

    std::vector<MenuItem> GetReorderedItems() const { return m_items; }
    std::map<wxString, std::vector<MenuItem>> GetModifiedSubMenus() const { return m_subMenus; }
    DisplayFlags GetDisplayFlags() const;

private:
    void OnMoveUp(wxCommandEvent& event);
    void OnMoveDown(wxCommandEvent& event);
    void OnToggle(wxCommandEvent& event);
    void OnMainSelect(wxCommandEvent& event);
    void OnAddMainItem(wxCommandEvent& event);
    void OnRemoveMainItem(wxCommandEvent& event);

    void OnSubMoveUp(wxCommandEvent& event);
    void OnSubMoveDown(wxCommandEvent& event);
    void OnSubToggle(wxCommandEvent& event);
    void OnAddSubItem(wxCommandEvent& event);
    void OnRemoveSubItem(wxCommandEvent& event);

    void OnClose(wxCloseEvent& event);

    void RefreshList(int selectIndex);
    void RefreshSubList(int selectIndex);
    void UpdateSubPanelForSelection(int mainIdx);
    wxString ItemLabel(const MenuItem& item) const;

    wxCheckListBox* m_listBox;
    wxButton*       m_upBtn;
    wxButton*       m_downBtn;
    wxButton*       m_addMainBtn;
    wxButton*       m_removeMainBtn;

    wxStaticText*   m_subLabel;
    wxCheckListBox* m_subListBox;
    wxButton*       m_subUpBtn;
    wxButton*       m_subDownBtn;
    wxButton*       m_addSubBtn;
    wxButton*       m_removeSubBtn;

    std::vector<MenuItem> m_items;
    std::map<wxString, std::vector<MenuItem>> m_subMenus;
    wxString m_currentSubSection;

    wxCheckBox* m_showUnixTimestamp;
    wxCheckBox* m_showZuluTimestamp;
    wxCheckBox* m_showTimeConverter;
    wxCheckBox* m_showHexDecConverter;
    wxCheckBox* m_showBase64Encoder;
    wxCheckBox* m_showUrlEncoder;
    wxCheckBox* m_showJsonFormatter;
};

#endif // CUSTOMIZEMENUDIALOG_H
