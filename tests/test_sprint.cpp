// Custom entry point: initialise wxWidgets before running tests so that
// wxString, wxDateTime and wxRegEx are all available.
// Link this target against Catch2::Catch2 (not Catch2::Catch2WithMain).

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>
#include <wx/init.h>
#include <wx/log.h>

#include "JiraService.h"

// ── SprintInfo ────────────────────────────────────────────────────────────────

TEST_CASE("GetDisplayText strips 'Dev Sprint ' prefix", "[sprint]") {
    SprintInfo s;
    s.name = "Dev Sprint 333";
    REQUIRE(s.GetDisplayText() == "333");
}

TEST_CASE("GetDisplayText strips 'Sprint ' prefix", "[sprint]") {
    SprintInfo s;
    s.name = "Sprint 336";
    REQUIRE(s.GetDisplayText() == "336");
}

TEST_CASE("GetDisplayText preserves names that don't start with 'Dev Sprint '", "[sprint]") {
    SprintInfo s;
    s.name = "My Custom Sprint";
    REQUIRE(s.GetDisplayText() == "My Custom Sprint");
}

TEST_CASE("GetDisplayText handles empty name", "[sprint]") {
    SprintInfo s;
    s.name = "";
    REQUIRE(s.GetDisplayText() == "");
}

TEST_CASE("GetDaysPassed returns -1 when startDate is not set", "[sprint]") {
    SprintInfo s;
    REQUIRE(!s.startDate.IsValid());
    REQUIRE(s.GetDaysPassed() == -1);
}

// ── JiraService::ParseSprintJson ──────────────────────────────────────────────

// Helper: build a minimal JIRA API response for a single sprint.
static wxString SingleSprintJson(const wxString& name, int id, const wxString& state) {
    return wxString::Format(
        R"({"maxResults":50,"startAt":0,"isLast":true,"values":[)"
        R"({"id":%d,"self":"https://example.atlassian.net/rest/agile/1.0/sprint/%d",)"
        R"("state":"%s","name":"%s",)"
        R"("startDate":"2024-02-12T00:00:00.000Z","endDate":"2024-02-25T00:00:00.000Z"}]})",
        id, id, state, name);
}

TEST_CASE("ParseSprintJson extracts id, name and state from a single sprint", "[jira]") {
    SprintInfo sprint = JiraService::ParseSprintJson(SingleSprintJson("Dev Sprint 333", 42, "active"));
    REQUIRE(sprint.id   == 42);
    REQUIRE(sprint.name == "Dev Sprint 333");
    REQUIRE(sprint.state == "active");
}

TEST_CASE("ParseSprintJson picks the sprint with the highest number", "[jira]") {
    wxString json =
        R"({"values":[)"
        R"({"id":1,"state":"active","name":"Dev Sprint 100","startDate":"2024-01-01","endDate":"2024-01-14"},)"
        R"({"id":2,"state":"active","name":"Dev Sprint 200","startDate":"2024-02-01","endDate":"2024-02-14"})"
        R"(]})";
    SprintInfo sprint = JiraService::ParseSprintJson(json);
    REQUIRE(sprint.name == "Dev Sprint 200");
    REQUIRE(sprint.id   == 2);
}

TEST_CASE("ParseSprintJson falls back to first object when no Dev Sprint name is found", "[jira]") {
    wxString json =
        R"({"values":[{"id":7,"state":"active","name":"Some Other Sprint",)"
        R"("startDate":"2024-02-01","endDate":"2024-02-14"}]})";
    SprintInfo sprint = JiraService::ParseSprintJson(json);
    REQUIRE(sprint.id   == 7);
    REQUIRE(sprint.name == "Some Other Sprint");
}

TEST_CASE("ParseSprintJson returns 'Unknown Sprint' for empty values array", "[jira]") {
    SprintInfo sprint = JiraService::ParseSprintJson(R"({"values":[]})");
    REQUIRE(sprint.name == "Unknown Sprint");
}

TEST_CASE("ParseSprintJson returns 'Unknown Sprint' when values key is absent", "[jira]") {
    SprintInfo sprint = JiraService::ParseSprintJson("{}");
    REQUIRE(sprint.name == "Unknown Sprint");
}

// ── JiraService::ParsePublicSprintJson ────────────────────────────────────────

TEST_CASE("ParsePublicSprintJson extracts name and start date", "[public]") {
    wxString json = R"({"name":"Sprint 336","start":"2026-03-06"})";
    SprintInfo sprint = JiraService::ParsePublicSprintJson(json);
    REQUIRE(sprint.name == "Sprint 336");
    REQUIRE(sprint.state == "active");
    REQUIRE(sprint.startDate.IsValid());
    REQUIRE(sprint.startDate.GetYear() == 2026);
    REQUIRE(sprint.startDate.GetMonth() == wxDateTime::Mar);
    REQUIRE(sprint.startDate.GetDay() == 6);
}

TEST_CASE("ParsePublicSprintJson handles name with spaces", "[public]") {
    wxString json = R"({"name":"Dev Sprint 333","start":"2024-02-12"})";
    SprintInfo sprint = JiraService::ParsePublicSprintJson(json);
    REQUIRE(sprint.name == "Dev Sprint 333");
}

TEST_CASE("ParsePublicSprintJson works with reversed field order", "[public]") {
    wxString json = R"({"start":"2026-03-06","name":"Sprint 336"})";
    SprintInfo sprint = JiraService::ParsePublicSprintJson(json);
    REQUIRE(sprint.name == "Sprint 336");
    REQUIRE(sprint.startDate.GetDay() == 6);
}

TEST_CASE("ParsePublicSprintJson handles missing start date", "[public]") {
    wxString json = R"({"name":"Sprint 336"})";
    SprintInfo sprint = JiraService::ParsePublicSprintJson(json);
    REQUIRE(sprint.name == "Sprint 336");
    REQUIRE(!sprint.startDate.IsValid());
}

TEST_CASE("ParsePublicSprintJson handles missing name", "[public]") {
    wxString json = R"({"start":"2026-03-06"})";
    SprintInfo sprint = JiraService::ParsePublicSprintJson(json);
    REQUIRE(sprint.name == "Unknown Sprint");
}

TEST_CASE("ParsePublicSprintJson returns 'Unknown Sprint' for empty JSON", "[public]") {
    SprintInfo sprint = JiraService::ParsePublicSprintJson("{}");
    REQUIRE(sprint.name == "Unknown Sprint");
    REQUIRE(sprint.state == "active");
}

TEST_CASE("ParsePublicSprintJson returns 'Unknown Sprint' for invalid JSON", "[public]") {
    SprintInfo sprint = JiraService::ParsePublicSprintJson("not json");
    REQUIRE(sprint.name == "Unknown Sprint");
}

TEST_CASE("ParsePublicSprintJson handles whitespace variations", "[public]") {
    wxString json = R"({ "name" : "Sprint 336" , "start" : "2026-03-06" })";
    SprintInfo sprint = JiraService::ParsePublicSprintJson(json);
    REQUIRE(sprint.name == "Sprint 336");
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    // Silence config-file-not-found warnings that JiraService's constructor emits.
    wxLog::EnableLogging(false);

    // Initialise wxWidgets (needed for wxString, wxDateTime, wxRegEx).
    wxInitialize();

    int result = Catch::Session().run(argc, argv);

    wxUninitialize();
    return result;
}
