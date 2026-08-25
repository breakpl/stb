#ifndef URLENCODERDIALOG_H
#define URLENCODERDIALOG_H

#include <wx/wx.h>

class UrlEncoderDialog : public wxDialog {
public:
    UrlEncoderDialog(wxWindow* parent);

private:
    void OnPlainChanged(wxCommandEvent& event);
    void OnEncodedChanged(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);

    wxTextCtrl* m_plainField;
    wxTextCtrl* m_encodedField;

    bool m_updating;

    wxDECLARE_EVENT_TABLE();
};

#endif // URLENCODERDIALOG_H
