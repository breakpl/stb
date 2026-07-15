#include "JiraService.h"
#include <wx/log.h>

wxDateTime JiraService::ParseIsoDateTime(const wxString& isoString) {
    wxDateTime dt;
    if (!isoString.IsEmpty()) {
        dt.ParseISOCombined(isoString);
    }
    return dt;
}

SprintInfo JiraService::ParsePublicSprintJson(const wxString& json) {
    SprintInfo sprint;
    sprint.id = 0;
    sprint.name = "Unknown Sprint";
    sprint.state = "active";

    int namePos = json.Find("\"name\"");
    if (namePos != wxNOT_FOUND) {
        int colonPos = json.find(':', namePos);
        int quoteStart = json.find('"', colonPos);
        int quoteEnd = json.find('"', quoteStart + 1);
        if (quoteStart != wxNOT_FOUND && quoteEnd != wxNOT_FOUND) {
            sprint.name = json.SubString(quoteStart + 1, quoteEnd - 1);
        }
    }

    wxString startStr;
    int startPos = json.Find("\"start\"");
    if (startPos != wxNOT_FOUND) {
        int colonPos = json.find(':', startPos);
        int quoteStart = json.find('"', colonPos);
        int quoteEnd = json.find('"', quoteStart + 1);
        if (quoteStart != wxNOT_FOUND && quoteEnd != wxNOT_FOUND) {
            startStr = json.SubString(quoteStart + 1, quoteEnd - 1);
        }
    }

    if (!startStr.IsEmpty()) {
        sprint.startDate.ParseFormat(startStr, "%Y-%m-%d");
    }

    return sprint;
}
