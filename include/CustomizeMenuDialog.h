#ifndef CUSTOMIZEMENUDIALOG_H
#define CUSTOMIZEMENUDIALOG_H

#include <wx/wx.h>
#include <wx/checklst.h>
#include <vector>
#include "Config.h"

class CustomizeMenuDialog : public wxDialog {
public:
    CustomizeMenuDialog(wxWindow* parent, const std::vector<MenuItem>& items);

    std::vector<MenuItem> GetReorderedItems() const { return m_items; }

private:
    void OnMoveUp(wxCommandEvent& event);
    void OnMoveDown(wxCommandEvent& event);
    void OnToggle(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);

    void RefreshList(int selectIndex);
    wxString ItemLabel(const MenuItem& item) const;

    wxCheckListBox* m_listBox;
    wxButton*       m_upBtn;
    wxButton*       m_downBtn;
    std::vector<MenuItem> m_items;
};

#endif // CUSTOMIZEMENUDIALOG_H
