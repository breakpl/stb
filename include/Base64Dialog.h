#ifndef BASE64DIALOG_H
#define BASE64DIALOG_H

#include <wx/wx.h>

class Base64Dialog : public wxDialog {
public:
    Base64Dialog(wxWindow* parent);

private:
    void OnPlainChanged(wxCommandEvent& event);
    void OnEncodedChanged(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);

    wxTextCtrl* m_plainField;
    wxTextCtrl* m_encodedField;

    bool m_updating;

    wxDECLARE_EVENT_TABLE();
};

#endif // BASE64DIALOG_H
