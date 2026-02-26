#ifndef CONVERTERDIALOG_H
#define CONVERTERDIALOG_H

#include <wx/wx.h>

class ConverterDialog : public wxDialog {
public:
    ConverterDialog(wxWindow* parent);
    
private:
    void OnDecimalChanged(wxCommandEvent& event);
    void OnHexChanged(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);
    
    wxTextCtrl* m_decimalField;
    wxTextCtrl* m_hexField;
    
    bool m_updating; // Flag to prevent circular updates
    
    wxDECLARE_EVENT_TABLE();
};

#endif // CONVERTERDIALOG_H
