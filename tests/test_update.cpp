// Custom entry point: initialise wxWidgets before running tests.
// Link against Catch2::Catch2 (not Catch2::Catch2WithMain).
//
// Only static / pure methods are tested here; CheckForUpdates and
// DownloadAsset require a live network and are not covered.
//
// NOTE: APP_VERSION is not defined for this test target, so it falls back to
// "0.0.0" (the guard in UpdateService.cpp). IsNewerThanCurrent tests rely on
// this baseline.

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>
#include <wx/init.h>
#include <wx/log.h>

#include "UpdateService.h"

// ── CompareVersions ───────────────────────────────────────────────────────────

TEST_CASE("CompareVersions returns 0 for identical versions", "[update][compare]") {
    REQUIRE(UpdateService::CompareVersions("1.0.0", "1.0.0") == 0);
    REQUIRE(UpdateService::CompareVersions("0.0.0", "0.0.0") == 0);
}

TEST_CASE("CompareVersions strips leading v and V", "[update][compare]") {
    REQUIRE(UpdateService::CompareVersions("v1.0.0", "1.0.0") == 0);
    REQUIRE(UpdateService::CompareVersions("V1.0.0", "1.0.0") == 0);
    REQUIRE(UpdateService::CompareVersions("v1.0.0", "v1.0.0") == 0);
}

TEST_CASE("CompareVersions returns negative when a < b", "[update][compare]") {
    REQUIRE(UpdateService::CompareVersions("1.0.0", "1.0.1") < 0);
    REQUIRE(UpdateService::CompareVersions("1.0.0", "2.0.0") < 0);
    REQUIRE(UpdateService::CompareVersions("0.9.9", "1.0.0") < 0);
    REQUIRE(UpdateService::CompareVersions("1.9.9", "2.0.0") < 0);
}

TEST_CASE("CompareVersions returns positive when a > b", "[update][compare]") {
    REQUIRE(UpdateService::CompareVersions("1.0.1", "1.0.0") > 0);
    REQUIRE(UpdateService::CompareVersions("2.0.0", "1.9.9") > 0);
    REQUIRE(UpdateService::CompareVersions("1.1.0", "1.0.9") > 0);
}

TEST_CASE("CompareVersions pads missing components with zero", "[update][compare]") {
    REQUIRE(UpdateService::CompareVersions("1.0", "1.0.0") == 0);
    REQUIRE(UpdateService::CompareVersions("1.0.0", "1.0") == 0);
    REQUIRE(UpdateService::CompareVersions("1", "1.0.0") == 0);
    REQUIRE(UpdateService::CompareVersions("1.1", "1.0.0") > 0);
}

TEST_CASE("CompareVersions handles empty strings", "[update][compare]") {
    REQUIRE(UpdateService::CompareVersions("", "") == 0);
    REQUIRE(UpdateService::CompareVersions("", "1.0.0") < 0);
    REQUIRE(UpdateService::CompareVersions("1.0.0", "") > 0);
}

TEST_CASE("CompareVersions stops parsing at first non-numeric token", "[update][compare]") {
    REQUIRE(UpdateService::CompareVersions("1.0.0-rc1", "1.0.0") == 0);
    REQUIRE(UpdateService::CompareVersions("1.0.0+build1", "1.0.0") == 0);
}

// ── IsNewerThanCurrent ────────────────────────────────────────────────────────

TEST_CASE("IsNewerThanCurrent returns true for a version above the baseline", "[update][newer]") {
    REQUIRE(UpdateService::IsNewerThanCurrent("1.0.0") == true);
    REQUIRE(UpdateService::IsNewerThanCurrent("v1.0.0") == true);
    REQUIRE(UpdateService::IsNewerThanCurrent("0.0.1") == true);
}

TEST_CASE("IsNewerThanCurrent returns false for the baseline version", "[update][newer]") {
    REQUIRE(UpdateService::IsNewerThanCurrent("0.0.0") == false);
    REQUIRE(UpdateService::IsNewerThanCurrent("v0.0.0") == false);
}

// ── ParseLatestReleaseJson ────────────────────────────────────────────────────

TEST_CASE("ParseLatestReleaseJson returns nullopt for empty input", "[update][parse]") {
    REQUIRE(!UpdateService::ParseLatestReleaseJson("").has_value());
}

TEST_CASE("ParseLatestReleaseJson returns nullopt when tag_name is absent", "[update][parse]") {
    REQUIRE(!UpdateService::ParseLatestReleaseJson("{}").has_value());
    REQUIRE(!UpdateService::ParseLatestReleaseJson(
        R"({"html_url":"https://github.com/r/releases/tag/v1","body":"notes"})").has_value());
}

TEST_CASE("ParseLatestReleaseJson returns nullopt for non-JSON input", "[update][parse]") {
    REQUIRE(!UpdateService::ParseLatestReleaseJson("not json at all").has_value());
}

TEST_CASE("ParseLatestReleaseJson extracts tag, htmlUrl and body", "[update][parse]") {
    wxString json =
        R"({"tag_name":"v1.0.18",)"
        R"("html_url":"https://github.com/owner/repo/releases/tag/v1.0.18",)"
        R"("body":"Bug fixes","assets":[]})";
    auto info = UpdateService::ParseLatestReleaseJson(json);
    REQUIRE(info.has_value());
    REQUIRE(info->tag     == "v1.0.18");
    REQUIRE(info->htmlUrl == "https://github.com/owner/repo/releases/tag/v1.0.18");
    REQUIRE(info->body    == "Bug fixes");
}

TEST_CASE("ParseLatestReleaseJson strips leading v from version field", "[update][parse]") {
    auto info = UpdateService::ParseLatestReleaseJson(R"({"tag_name":"v2.3.4","assets":[]})");
    REQUIRE(info.has_value());
    REQUIRE(info->tag     == "v2.3.4");
    REQUIRE(info->version == "2.3.4");
}

TEST_CASE("ParseLatestReleaseJson version equals tag when no v prefix", "[update][parse]") {
    auto info = UpdateService::ParseLatestReleaseJson(R"({"tag_name":"2.3.4","assets":[]})");
    REQUIRE(info.has_value());
    REQUIRE(info->tag     == "2.3.4");
    REQUIRE(info->version == "2.3.4");
}

TEST_CASE("ParseLatestReleaseJson HasAsset is false with empty assets array", "[update][parse]") {
    auto info = UpdateService::ParseLatestReleaseJson(R"({"tag_name":"v1.0.0","assets":[]})");
    REQUIRE(info.has_value());
    REQUIRE(!info->HasAsset());
    REQUIRE(info->assetName.IsEmpty());
    REQUIRE(info->assetUrl.IsEmpty());
}

TEST_CASE("ParseLatestReleaseJson HasAsset is false when assets key is absent", "[update][parse]") {
    auto info = UpdateService::ParseLatestReleaseJson(R"({"tag_name":"v1.0.0"})");
    REQUIRE(info.has_value());
    REQUIRE(!info->HasAsset());
}

// ── PickAssetForThisPlatform ──────────────────────────────────────────────────

TEST_CASE("PickAssetForThisPlatform returns empty string for empty fragment", "[update][pick]") {
    REQUIRE(UpdateService::PickAssetForThisPlatform("").IsEmpty());
}

TEST_CASE("PickAssetForThisPlatform returns empty when no asset matches this platform", "[update][pick]") {
    // .tar.bz2 is never a platform suffix on any supported OS.
    wxString fragment =
        R"("assets":[{"name":"app.tar.bz2","browser_download_url":"https://example.com/app.tar.bz2"}])";
    REQUIRE(UpdateService::PickAssetForThisPlatform(fragment).IsEmpty());
}

#if defined(__APPLE__)
#  if defined(__aarch64__) || defined(__arm64__)

TEST_CASE("PickAssetForThisPlatform prefers arm64.dmg over generic .dmg on macOS arm64", "[update][pick]") {
    wxString fragment =
        R"("assets":[)"
        R"({"name":"app-x86_64.dmg","browser_download_url":"https://example.com/x86.dmg"},)"
        R"({"name":"app-arm64.dmg","browser_download_url":"https://example.com/arm64.dmg"})"
        R"(])";
    REQUIRE(UpdateService::PickAssetForThisPlatform(fragment) == "app-arm64.dmg");
}

TEST_CASE("PickAssetForThisPlatform falls back to .dmg on macOS arm64 when no arch-specific asset", "[update][pick]") {
    wxString fragment =
        R"("assets":[{"name":"SprintToolBox.dmg","browser_download_url":"https://example.com/app.dmg"}])";
    REQUIRE(UpdateService::PickAssetForThisPlatform(fragment) == "SprintToolBox.dmg");
}

TEST_CASE("PickAssetForThisPlatform ignores .exe assets on macOS arm64", "[update][pick]") {
    wxString fragment =
        R"("assets":[{"name":"setup.exe","browser_download_url":"https://example.com/setup.exe"}])";
    REQUIRE(UpdateService::PickAssetForThisPlatform(fragment).IsEmpty());
}

TEST_CASE("PickAssetForThisPlatform does not match .pkg on macOS arm64", "[update][pick]") {
    wxString fragment =
        R"("assets":[{"name":"SprintToolBox-arm64.pkg","browser_download_url":"https://example.com/arm64.pkg"}])";
    REQUIRE(UpdateService::PickAssetForThisPlatform(fragment).IsEmpty());
}

TEST_CASE("ParseLatestReleaseJson populates assetUrl for arm64 macOS", "[update][parse]") {
    wxString json =
        R"({"tag_name":"v1.0.18",)"
        R"("html_url":"https://github.com/owner/repo/releases/tag/v1.0.18",)"
        R"("body":"",)"
        R"("assets":[)"
        R"({"name":"SprintToolBox-arm64.dmg","browser_download_url":"https://github.com/owner/repo/releases/download/v1.0.18/SprintToolBox-arm64.dmg"},)"
        R"({"name":"SprintToolBox-x86_64.dmg","browser_download_url":"https://github.com/owner/repo/releases/download/v1.0.18/SprintToolBox-x86_64.dmg"})"
        R"(]})";
    auto info = UpdateService::ParseLatestReleaseJson(json);
    REQUIRE(info.has_value());
    REQUIRE(info->assetName == "SprintToolBox-arm64.dmg");
    REQUIRE(info->assetUrl.Contains("arm64.dmg"));
    REQUIRE(info->HasAsset());
}

#  else // macOS x86_64

TEST_CASE("PickAssetForThisPlatform prefers x86_64.dmg over generic .dmg on macOS x86_64", "[update][pick]") {
    wxString fragment =
        R"("assets":[)"
        R"({"name":"app.dmg","browser_download_url":"https://example.com/generic.dmg"},)"
        R"({"name":"app-x86_64.dmg","browser_download_url":"https://example.com/x86_64.dmg"})"
        R"(])";
    REQUIRE(UpdateService::PickAssetForThisPlatform(fragment) == "app-x86_64.dmg");
}

#  endif

#elif defined(_WIN32)

TEST_CASE("PickAssetForThisPlatform picks .exe on Windows", "[update][pick]") {
    wxString fragment =
        R"("assets":[{"name":"SprintToolBox-setup.exe","browser_download_url":"https://example.com/setup.exe"}])";
    REQUIRE(UpdateService::PickAssetForThisPlatform(fragment) == "SprintToolBox-setup.exe");
}

#elif defined(__linux__)

TEST_CASE("PickAssetForThisPlatform picks x86_64.AppImage on Linux x86_64", "[update][pick]") {
    wxString fragment =
        R"("assets":[{"name":"SprintToolBox-x86_64.AppImage","browser_download_url":"https://example.com/app.AppImage"}])";
    REQUIRE(UpdateService::PickAssetForThisPlatform(fragment) == "SprintToolBox-x86_64.AppImage");
}

#endif

// ── Entry point ───────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    wxLog::EnableLogging(false);
    wxInitialize();
    int result = Catch::Session().run(argc, argv);
    wxUninitialize();
    return result;
}
