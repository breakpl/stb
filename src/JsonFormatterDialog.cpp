#include "JsonFormatterDialog.h"
#include <wx/sizer.h>
#include <wx/stattext.h>

wxBEGIN_EVENT_TABLE(JsonFormatterDialog, wxDialog)
    EVT_CLOSE(JsonFormatterDialog::OnClose)
wxEND_EVENT_TABLE()

static wxString FormatJson(const wxString& input, bool* valid) {
    wxString result;
    int indent = 0;
    bool inString = false;
    bool escape = false;
    bool hasError = false;

    for (size_t i = 0; i < input.length(); i++) {
        wxChar c = input[i];

        if (escape) {
            result += c;
            escape = false;
            continue;
        }
        if (c == '\\' && inString) {
            result += c;
            escape = true;
            continue;
        }
        if (c == '"') {
            inString = !inString;
            result += c;
            continue;
        }
        if (inString) {
            result += c;
            continue;
        }

        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            continue;

        switch (c) {
            case '{': case '[':
                result += c;
                result += '\n';
                indent++;
                result += wxString(indent * 2, ' ');
                break;
            case '}': case ']':
                result += '\n';
                if (indent > 0) {
                    indent--;
                } else {
                    hasError = true;
                }
                result += wxString(indent * 2, ' ');
                result += c;
                break;
            case ',':
                result += c;
                result += '\n';
                result += wxString(indent * 2, ' ');
                break;
            case ':':
                result += ": ";
                break;
            default:
                result += c;
        }
    }

    if (valid) *valid = !inString && indent == 0 && !hasError;
    return result;
}

static wxString MinifyJson(const wxString& input) {
    wxString result;
    bool inString = false;
    bool escape = false;

    for (size_t i = 0; i < input.length(); i++) {
        wxChar c = input[i];

        if (escape) {
            result += c;
            escape = false;
            continue;
        }
        if (c == '\\' && inString) {
            result += c;
            escape = true;
            continue;
        }
        if (c == '"') {
            inString = !inString;
            result += c;
            continue;
        }
        if (inString) {
            result += c;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            continue;
        result += c;
    }

    return result;
}

JsonFormatterDialog::JsonFormatterDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, "JSON Formatter",
               wxDefaultPosition, wxSize(520, 440),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxSTAY_ON_TOP)
    , m_updating(false)
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText* inputLabel = new wxStaticText(this, wxID_ANY, "Raw / minified JSON:");
    mainSizer->Add(inputLabel, 0, wxALL, 10);

    m_inputField = new wxTextCtrl(this, wxID_ANY, "",
                                   wxDefaultPosition, wxDefaultSize,
                                   wxTE_MULTILINE | wxTE_DONTWRAP);
    m_inputField->SetHint("Paste JSON here...");
    mainSizer->Add(m_inputField, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    m_statusLabel = new wxStaticText(this, wxID_ANY, "");
    mainSizer->Add(m_statusLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

    wxStaticText* outputLabel = new wxStaticText(this, wxID_ANY, "Formatted:");
    mainSizer->Add(outputLabel, 0, wxALL, 10);

    m_outputField = new wxTextCtrl(this, wxID_ANY, "",
                                    wxDefaultPosition, wxDefaultSize,
                                    wxTE_MULTILINE | wxTE_DONTWRAP);
    m_outputField->SetHint("Formatted JSON appears here...");
    mainSizer->Add(m_outputField, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    SetSizer(mainSizer);

    m_inputField->Bind(wxEVT_TEXT, &JsonFormatterDialog::OnInputChanged, this);
    m_outputField->Bind(wxEVT_TEXT, &JsonFormatterDialog::OnOutputChanged, this);

    Centre();
}

void JsonFormatterDialog::OnInputChanged(wxCommandEvent& event) {
    if (m_updating) return;

    wxString text = m_inputField->GetValue();

    if (text.IsEmpty()) {
        m_updating = true;
        m_outputField->SetValue("");
        m_inputField->SetBackgroundColour(wxNullColour);
        m_outputField->SetBackgroundColour(wxNullColour);
        m_statusLabel->SetLabel("");
        m_updating = false;
        return;
    }

    bool valid = false;
    wxString formatted = FormatJson(text, &valid);

    m_updating = true;
    m_outputField->SetValue(formatted);
    m_outputField->SetBackgroundColour(wxColour(200, 200, 200));
    if (valid) {
        m_inputField->SetBackgroundColour(wxNullColour);
        m_statusLabel->SetLabel("");
    } else {
        m_inputField->SetBackgroundColour(wxColour(255, 224, 224));
        m_statusLabel->SetLabel("Invalid JSON");
        m_statusLabel->SetForegroundColour(wxColour(200, 0, 0));
    }
    m_updating = false;

    Layout();
    Refresh();
}

void JsonFormatterDialog::OnOutputChanged(wxCommandEvent& event) {
    if (m_updating) return;

    wxString text = m_outputField->GetValue();

    if (text.IsEmpty()) {
        m_updating = true;
        m_inputField->SetValue("");
        m_inputField->SetBackgroundColour(wxNullColour);
        m_outputField->SetBackgroundColour(wxNullColour);
        m_statusLabel->SetLabel("");
        m_updating = false;
        return;
    }

    m_updating = true;
    m_inputField->SetValue(MinifyJson(text));
    m_inputField->SetBackgroundColour(wxColour(200, 200, 200));
    m_outputField->SetBackgroundColour(wxNullColour);
    m_statusLabel->SetLabel("");
    m_updating = false;

    Refresh();
}

void JsonFormatterDialog::OnClose(wxCloseEvent& event) {
    Hide();
}
