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
    // Silence config-file-not-found warnings during test runs.
    wxLog::EnableLogging(false);

    // Initialise wxWidgets (needed for wxString, wxDateTime, wxRegEx).
    wxInitialize();

    int result = Catch::Session().run(argc, argv);

    wxUninitialize();
    return result;
}
