#ifndef JIRASERVICE_H
#define JIRASERVICE_H

#include <wx/wx.h>
#include <wx/datetime.h>

struct SprintInfo {
    int id;
    wxString name;
    wxString state;
    wxDateTime startDate;
    wxDateTime endDate;

    wxString GetDisplayText() const {
        wxString displayText = name;
        if (displayText.StartsWith("Dev Sprint ")) {
            displayText = displayText.Mid(11);
        } else if (displayText.StartsWith("Sprint ")) {
            displayText = displayText.Mid(7);
        }
        return displayText;
    }

    int GetDaysPassed() const {
        if (startDate.IsValid()) {
            wxDateTime now = wxDateTime::Now();
            now.ResetTime();
            wxDateTime day = startDate;
            day.ResetTime();
            int count = 0;
            while (day < now) {
                wxDateTime::WeekDay wd = day.GetWeekDay();
                if (wd != wxDateTime::Sat && wd != wxDateTime::Sun) {
                    count++;
                }
                day += wxDateSpan::Day();
            }
            return count + 1;
        }
        return -1;
    }
};

class JiraService {
public:
    static SprintInfo ParsePublicSprintJson(const wxString& json);
    static wxDateTime ParseIsoDateTime(const wxString& isoString);
};

#endif // JIRASERVICE_H
