#include "JiraService.h"
#include "Config.h"
#include <wx/log.h>
#include <wx/base64.h>
#include <wx/regex.h>
#include <curl/curl.h>
#include <sstream>

JiraService::JiraService() {
    LoadCredentials();
}

JiraService::~JiraService() {
}

void JiraService::LoadCredentials() {
    Config& config = Config::GetInstance();
    m_email = config.GetJiraEmail();
    m_token = config.GetJiraToken();
    m_baseURL = config.GetJiraBaseURL();
    m_boardID = config.GetJiraBoardID();
    
    if (m_email.IsEmpty() || m_token.IsEmpty() || m_baseURL.IsEmpty()) {
        wxLogWarning("JIRA credentials not fully configured in SprintToolBox.ini");
    }
    
    if (m_boardID == 0) {
        wxLogWarning("JIRA BoardID not configured, using default: 2355");
        m_boardID = 2355;
    }
}

void JiraService::ReloadCredentials() {
    Config::GetInstance().Reload();
    LoadCredentials();
}

void JiraService::FetchCurrentSprint() {
    wxLogMessage("Fetching current sprint from JIRA...");
    
    // Check for placeholder credentials
    if (m_email == "test@example.com" || m_email.IsEmpty() || 
        m_token == "your_api_token_here" || m_token.IsEmpty() ||
        m_baseURL == "https://your-jira-instance.atlassian.net" || m_baseURL.IsEmpty()) {
        wxLogWarning("JIRA credentials contain placeholders or are empty. Please update SprintToolBox.ini");
        if (m_errorCallback) {
            m_errorCallback("Configure SprintToolBox.ini with real JIRA credentials", "NOT_CONFIGURED");
        }
        return;
    }
    
    try {
        wxString apiUrl = wxString::Format("%s/rest/agile/1.0/board/%d/sprint?state=active", 
                                          m_baseURL, m_boardID);
        
        wxLogMessage("Making request to: %s", apiUrl);
        wxString response = MakeHttpRequest(apiUrl);
        
        if (response.IsEmpty()) {
            wxLogWarning("Empty response from JIRA API");
            if (m_errorCallback) {
                m_errorCallback("Empty response from JIRA API", "EMPTY_RESPONSE");
            }
            return;
        }
        
        wxLogMessage("Received response, parsing...");
        SprintInfo sprint = ParseSprintJson(response);
        
        if (m_successCallback) {
            m_successCallback(sprint);
        }
        
    } catch (const std::exception& e) {
        wxLogError("JIRA fetch exception: %s", e.what());
        if (m_errorCallback) {
            m_errorCallback(wxString(e.what()), "EXCEPTION");
        }
    } catch (...) {
        wxLogError("Unknown JIRA fetch error");
        if (m_errorCallback) {
            m_errorCallback("Unknown error occurred", "UNKNOWN_ERROR");
        }
    }
}

void JiraService::OnFetchComplete(const wxString& response) {
    SprintInfo sprint = ParseSprintJson(response);
    if (m_successCallback) {
        m_successCallback(sprint);
    }
}

void JiraService::OnFetchError(const wxString& error) {
    if (m_errorCallback) {
        m_errorCallback(error, "HTTP_ERROR");
    }
}

// Helper method: Base64 encode for Basic Auth
wxString JiraService::Base64Encode(const wxString& input) const {
    return wxBase64Encode(input.ToUTF8(), input.Length());
}

// Helper method: Make HTTP request with libcurl
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

wxString JiraService::MakeHttpRequest(const wxString& url) const {
    CURL* curl = curl_easy_init();
    std::string readBuffer;
    
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }
    
    // Prepare Basic Auth header
    wxString auth = m_email + ":" + m_token;
    wxString authHeader = "Authorization: Basic " + Base64Encode(auth);
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, authHeader.ToStdString().c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    
    curl_easy_setopt(curl, CURLOPT_URL, url.ToStdString().c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        throw std::runtime_error(std::string("CURL error: ") + curl_easy_strerror(res));
    }
    
    if (http_code != 200) {
        throw std::runtime_error("HTTP error " + std::to_string(http_code));
    }
    
    return wxString::FromUTF8(readBuffer.c_str());
}

// Helper method: Parse ISO 8601 datetime
wxDateTime JiraService::ParseIsoDateTime(const wxString& isoString) const {
    wxDateTime dt;
    // Format: 2024-02-25T12:34:56.789Z
    if (!isoString.IsEmpty()) {
        dt.ParseISOCombined(isoString);
    }
    return dt;
}

// Helper method: Simple JSON parsing for sprint info
SprintInfo JiraService::ParseSprintJson(const wxString& json) const {
    SprintInfo sprint;
    sprint.id = 0;
    sprint.name = "Unknown Sprint";
    sprint.state = "unknown";
    
    // Find the values array
    int valuesStart = json.Find("\"values\":[");
    if (valuesStart == wxNOT_FOUND) {
        wxLogWarning("Could not find 'values' array in JIRA response");
        return sprint;
    }
    
    wxString valuesSection = json.Mid(valuesStart);
    
    // Extract fields using simple string search
    auto extractField = [](const wxString& json, const wxString& field, int startPos = 0) -> wxString {
        wxString searchPattern = "\"" + field + "\":";
        int pos = json.find(searchPattern, startPos);
        if (pos == wxNOT_FOUND) return "";
        
        pos += searchPattern.Length();
        while (pos < json.Length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
        
        if (pos >= json.Length()) return "";
        
        if (json[pos] == '"') {
            // String value
            int start = pos + 1;
            int end = json.find('"', start);
            if (end == wxNOT_FOUND) return "";
            return json.Mid(start, end - start);
        } else if (json[pos] >= '0' && json[pos] <= '9') {
            // Number value
            int start = pos;
            int end = start;
            while (end < json.Length() && (json[end] >= '0' && json[end] <= '9')) end++;
            return json.Mid(start, end - start);
        }
        return "";
    };
    
    // Find all sprints and get the "Dynatrace Dev Sprint" with highest number
    SprintInfo bestSprint;
    bestSprint.id = 0;
    int highestSprintNum = -1;
    
    int searchPos = 0;
    while (true) {
        // Find next sprint object
        int bracePos = valuesSection.find("{", searchPos);
        if (bracePos == wxNOT_FOUND) break;
        
        // Find the closing brace for this sprint object
        int nextBrace = valuesSection.find("{", bracePos + 1);
        int closeBrace = valuesSection.find("}", bracePos);
        if (closeBrace == wxNOT_FOUND) break;
        
        // Extract this sprint's data
        wxString sprintData = valuesSection.Mid(bracePos, closeBrace - bracePos + 1);
        
        // Extract name
        wxString name = extractField(sprintData, "name");
        
        // Check if this is a "Dynatrace Dev Sprint" or "Dev Sprint"
        if (name.Contains("Dev Sprint") || name.Contains("Dynatrace")) {
            // Extract the sprint number
            wxRegEx regex("([0-9]{3,})");
            if (regex.Matches(name)) {
                wxString numStr = regex.GetMatch(name, 1);
                long sprintNum;
                if (numStr.ToLong(&sprintNum)) {
                    wxLogMessage("Found sprint: %s (number: %ld)", name, sprintNum);
                    
                    if (sprintNum > highestSprintNum) {
                        highestSprintNum = sprintNum;
                        
                        // Extract all fields for this sprint
                        wxString idStr = extractField(sprintData, "id");
                        if (!idStr.IsEmpty()) {
                            long longId;
                            if (idStr.ToLong(&longId)) {
                                bestSprint.id = (int)longId;
                            }
                        }
                        
                        bestSprint.name = name;
                        bestSprint.state = extractField(sprintData, "state");
                        
                        wxString startDateStr = extractField(sprintData, "startDate");
                        wxString endDateStr = extractField(sprintData, "endDate");
                        
                        bestSprint.startDate = ParseIsoDateTime(startDateStr);
                        bestSprint.endDate = ParseIsoDateTime(endDateStr);
                    }
                }
            }
        }
        
        // Move to next sprint
        searchPos = closeBrace + 1;
        
        // Stop if we've reached the end of the values array
        if (searchPos >= valuesSection.Length() || valuesSection.find("]", searchPos) < nextBrace) {
            break;
        }
    }
    
    if (highestSprintNum > 0) {
        wxLogMessage("Selected sprint: ID=%d, Name=%s, State=%s", bestSprint.id, bestSprint.name, bestSprint.state);
        return bestSprint;
    }
    
    // Fallback: if no Dev Sprint found, take the first active sprint
    wxLogWarning("No 'Dev Sprint' found, using first sprint in response");
    int firstBrace = valuesSection.Find("{");
    if (firstBrace != wxNOT_FOUND) {
        wxString sprintData = valuesSection.Mid(firstBrace);
        
        wxString idStr = extractField(sprintData, "id");
        if (!idStr.IsEmpty()) {
            long longId;
            if (idStr.ToLong(&longId)) {
                sprint.id = (int)longId;
            }
        }
        
        sprint.name = extractField(sprintData, "name");
        sprint.state = extractField(sprintData, "state");
        
        wxString startDateStr = extractField(sprintData, "startDate");
        wxString endDateStr = extractField(sprintData, "endDate");
        
        sprint.startDate = ParseIsoDateTime(startDateStr);
        sprint.endDate = ParseIsoDateTime(endDateStr);
    }
    
    wxLogMessage("Parsed sprint: ID=%d, Name=%s, State=%s", sprint.id, sprint.name, sprint.state);
    
    return sprint;
}
