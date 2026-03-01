#ifndef JIRASERVICE_H
#define JIRASERVICE_H

#include <wx/wx.h>
#include <wx/datetime.h>
#include <functional>

struct SprintInfo {
    int id;
    wxString name;
    wxString state;
    wxDateTime startDate;
    wxDateTime endDate;
    
    wxString GetDisplayText() const {
        // Extract just the number from "Dev Sprint 333" -> "333"
        wxString displayText = name;
        if (displayText.StartsWith("Dev Sprint ")) {
            displayText = displayText.Mid(11);
        }
        return displayText;
    }
    
    int GetDaysPassed() const {
        if (startDate.IsValid()) {
            wxDateTime now = wxDateTime::Now();
            wxTimeSpan diff = now.Subtract(startDate);
            return diff.GetDays();
        }
        return -1;
    }
};

class JiraService {
public:
    JiraService();
    ~JiraService();
    
    // Callback types
    using SuccessCallback = std::function<void(const SprintInfo&)>;
    using ErrorCallback = std::function<void(const wxString& error, const wxString& errorCode)>;
    
    // Set callbacks
    void SetSuccessCallback(SuccessCallback callback) { m_successCallback = callback; }
    void SetErrorCallback(ErrorCallback callback) { m_errorCallback = callback; }

    // Fetch current sprint
    void FetchCurrentSprint();
    void ReloadCredentials();

    // Exposed for unit testing – static because they are pure parsing helpers
    static SprintInfo ParseSprintJson(const wxString& json);
    static wxDateTime ParseIsoDateTime(const wxString& isoString);

private:
    void LoadCredentials();
    void OnFetchComplete(const wxString& response);
    void OnFetchError(const wxString& error);

    // HTTP helper methods
    wxString Base64Encode(const wxString& input) const;
    std::string MakeHttpRequest(const wxString& url) const;
    
    wxString m_email;
    wxString m_token;
    wxString m_baseURL;
    int m_boardID;
    SuccessCallback m_successCallback;
    ErrorCallback m_errorCallback;
};

#endif // JIRASERVICE_H
