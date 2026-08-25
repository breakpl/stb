#ifndef JSONFORMATTERDIALOG_H
#define JSONFORMATTERDIALOG_H

#include <wx/wx.h>

class JsonFormatterDialog : public wxDialog {
public:
    JsonFormatterDialog(wxWindow* parent);

private:
    void OnInputChanged(wxCommandEvent& event);
    void OnOutputChanged(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);

    wxTextCtrl*   m_inputField;
    wxTextCtrl*   m_outputField;
    wxStaticText* m_statusLabel;

    bool m_updating;

    wxDECLARE_EVENT_TABLE();
};

#endif // JSONFORMATTERDIALOG_H
