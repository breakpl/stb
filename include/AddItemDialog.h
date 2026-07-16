#ifndef ADDITEMDIALOG_H
#define ADDITEMDIALOG_H

#include <wx/wx.h>
#include "Config.h"

class AddItemDialog : public wxDialog {
public:
    enum class Mode { Main, Sub };

    AddItemDialog(wxWindow* parent, Mode mode);

    MenuItem GetItem() const;

private:
    void OnSeparatorToggle(wxCommandEvent& event);
    void OnSubmenuToggle(wxCommandEvent& event);
    void OnOK(wxCommandEvent& event);

    wxTextCtrl*   m_nameCtrl;
    wxTextCtrl*   m_urlCtrl;
    wxStaticText* m_urlLabel;
    wxCheckBox*   m_separatorCheck;
    wxCheckBox*   m_submenuCheck;
    Mode          m_mode;
};

#endif // ADDITEMDIALOG_H
