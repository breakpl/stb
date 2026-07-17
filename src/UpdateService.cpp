#include "UpdateService.h"

#include <wx/log.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/regex.h>
#include <wx/tokenzr.h>

#include <curl/curl.h>

#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef APP_VERSION
#define APP_VERSION "0.0.0"
#endif

namespace {

constexpr long kConnectTimeoutSec = 10;
constexpr long kTotalTimeoutSec   = 30;

size_t WriteToStringCb(void* contents, size_t size, size_t nmemb, void* userp) {
    const size_t bytes = size * nmemb;
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), bytes);
    return bytes;
}

size_t WriteToFileCb(void* contents, size_t size, size_t nmemb, void* userp) {
    return std::fwrite(contents, size, nmemb, static_cast<std::FILE*>(userp)) * size;
}

struct ProgressCtx {
    UpdateService::ProgressCallback cb;
};

int ProgressTrampoline(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                       curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
    auto* ctx = static_cast<ProgressCtx*>(clientp);
    if (ctx && ctx->cb && dltotal > 0) {
        ctx->cb(static_cast<double>(dlnow) / static_cast<double>(dltotal));
    }
    return 0;
}

// Extract the string value of a top-level field. Naive but matches the
// JSON shape GitHub returns for the releases endpoint.
wxString ExtractStringField(const wxString& json, const wxString& field, int startPos = 0) {
    const wxString needle = "\"" + field + "\":";
    int pos = json.find(needle, startPos);
    if (pos == wxNOT_FOUND) return wxEmptyString;
    pos += needle.Length();
    while (pos < static_cast<int>(json.Length()) &&
           (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n')) {
        ++pos;
    }
    if (pos >= static_cast<int>(json.Length()) || json[pos] != '"') return wxEmptyString;
    int start = pos + 1;
    // Find closing quote, honoring backslash escapes.
    int end = start;
    while (end < static_cast<int>(json.Length())) {
        if (json[end] == '\\' && end + 1 < static_cast<int>(json.Length())) {
            end += 2;
            continue;
        }
        if (json[end] == '"') break;
        ++end;
    }
    if (end >= static_cast<int>(json.Length())) return wxEmptyString;
    return json.SubString(start, end - 1);
}

// Determine the suffix pattern an asset file name should end with for the
// platform this binary was compiled for. Returns multiple candidates ordered
// from most-specific to least-specific.
std::vector<wxString> PlatformAssetSuffixes() {
#if defined(__APPLE__)
  #if defined(__aarch64__) || defined(__arm64__)
    return { "arm64.pkg", "arm64.dmg", "macos-arm64.zip", ".dmg" };
  #else
    return { "x86_64.dmg", "x86_64.pkg", "intel.dmg", "macos-x86_64.zip", ".dmg" };
  #endif
#elif defined(_WIN32)
  #if defined(_M_ARM64) || defined(__aarch64__)
    return { "arm64.exe", "windows-arm64.exe", ".exe" };
  #elif defined(_WIN64) || defined(__x86_64__)
    return { "x86_64.exe", "x64.exe", "windows-x86_64.exe", ".exe" };
  #else
    return { "x86.exe", "win32.exe", ".exe" };
  #endif
#elif defined(__linux__)
  #if defined(__aarch64__)
    return { "arm64.AppImage", "aarch64.AppImage", "arm64.deb", ".AppImage" };
  #else
    return { "x86_64.AppImage", "amd64.AppImage", "amd64.deb", ".AppImage" };
  #endif
#else
    return {};
#endif
}

} // namespace

UpdateService::UpdateService(const wxString& repoOwner, const wxString& repoName)
    : m_repoOwner(repoOwner), m_repoName(repoName) {}

int UpdateService::CompareVersions(const wxString& a, const wxString& b) {
    auto strip = [](wxString v) {
        if (!v.empty() && (v[0] == 'v' || v[0] == 'V')) v = v.Mid(1);
        return v;
    };
    auto parts = [](const wxString& v) {
        std::vector<long> out;
        wxStringTokenizer tok(v, ".-+");
        while (tok.HasMoreTokens()) {
            long n = 0;
            wxString t = tok.GetNextToken();
            // Stop at first non-numeric token (e.g. "rc1") — treat as 0.
            if (!t.ToLong(&n)) break;
            out.push_back(n);
        }
        return out;
    };
    auto pa = parts(strip(a));
    auto pb = parts(strip(b));
    const size_t n = std::max(pa.size(), pb.size());
    for (size_t i = 0; i < n; ++i) {
        long va = i < pa.size() ? pa[i] : 0;
        long vb = i < pb.size() ? pb[i] : 0;
        if (va != vb) return va < vb ? -1 : 1;
    }
    return 0;
}

bool UpdateService::IsNewerThanCurrent(const wxString& remoteTag) {
    return CompareVersions(remoteTag, APP_VERSION) > 0;
}

void UpdateService::CheckForUpdates() {
    const wxString url = wxString::Format(
        "https://api.github.com/repos/%s/%s/releases/latest",
        m_repoOwner, m_repoName);
    wxLogMessage("UpdateService: checking %s", url);

    try {
        std::string body = MakeHttpRequest(url);
        if (body.empty()) {
            if (m_errorCallback) m_errorCallback("Empty response from GitHub", "EMPTY_RESPONSE");
            return;
        }
        const wxString json = wxString::FromUTF8(body.c_str());
        auto info = ParseLatestReleaseJson(json);
        if (!info) {
            if (m_errorCallback) m_errorCallback("Failed to parse release JSON", "PARSE_ERROR");
            return;
        }
        if (m_successCallback) m_successCallback(*info);
    } catch (const std::exception& e) {
        const wxString msg(e.what());
        wxString code = "EXCEPTION";
        if (msg.StartsWith("NETWORK_ERROR:"))     code = "NETWORK_ERROR";
        else if (msg.StartsWith("HTTP_ERROR:"))   code = "HTTP_ERROR";
        wxLogWarning("UpdateService: %s", msg);
        if (m_errorCallback) m_errorCallback(msg, code);
    } catch (...) {
        if (m_errorCallback) m_errorCallback("Unknown error", "UNKNOWN_ERROR");
    }
}

wxFileName UpdateService::DownloadAsset(const ReleaseInfo& info,
                                       ProgressCallback onProgress) const {
    if (!info.HasAsset()) {
        throw std::runtime_error("NETWORK_ERROR: no asset URL for this platform");
    }
    wxFileName out(wxStandardPaths::Get().GetTempDir(), info.assetName);
    // Caller-visible side effect: file is overwritten if it already exists.
    MakeHttpRequest(info.assetUrl, onProgress, out);
    return out;
}

std::optional<ReleaseInfo> UpdateService::ParseLatestReleaseJson(const wxString& json) {
    ReleaseInfo info;
    info.tag      = ExtractStringField(json, "tag_name");
    info.htmlUrl  = ExtractStringField(json, "html_url");
    info.body     = ExtractStringField(json, "body");

    if (info.tag.IsEmpty()) return std::nullopt;

    info.version = info.tag;
    if (!info.version.empty() && (info.version[0] == 'v' || info.version[0] == 'V')) {
        info.version = info.version.Mid(1);
    }

    // The "assets" array holds objects each with "name" and "browser_download_url".
    int assetsPos = json.Find("\"assets\":[");
    if (assetsPos != wxNOT_FOUND) {
        wxString assetsFragment = json.Mid(assetsPos);
        info.assetName = PickAssetForThisPlatform(assetsFragment);
        if (!info.assetName.IsEmpty()) {
            // Locate the asset object containing that name and pull its download URL.
            int namePos = assetsFragment.Find(info.assetName);
            if (namePos != wxNOT_FOUND) {
                // Search both directions: download URL usually appears *after* "name".
                int urlSearchStart = namePos;
                info.assetUrl = ExtractStringField(assetsFragment, "browser_download_url",
                                                   urlSearchStart);
                if (info.assetUrl.IsEmpty()) {
                    // Fall back to a search before the name field.
                    info.assetUrl = ExtractStringField(assetsFragment, "browser_download_url");
                }
            }
        }
    }
    return info;
}

wxString UpdateService::PickAssetForThisPlatform(const wxString& assetsJsonFragment) {
    // Collect all asset names, then match by suffix preference.
    std::vector<wxString> names;
    int pos = 0;
    while (true) {
        wxString name = ExtractStringField(assetsJsonFragment, "name", pos);
        if (name.IsEmpty()) break;
        names.push_back(name);
        int found = assetsJsonFragment.find("\"name\":", pos);
        if (found == wxNOT_FOUND) break;
        pos = found + 7;
        // Skip the value we just extracted so we don't re-find it.
        int q1 = assetsJsonFragment.find('"', pos);
        if (q1 == wxNOT_FOUND) break;
        int q2 = assetsJsonFragment.find('"', q1 + 1);
        if (q2 == wxNOT_FOUND) break;
        pos = q2 + 1;
    }

    for (const wxString& suffix : PlatformAssetSuffixes()) {
        for (const wxString& name : names) {
            if (name.EndsWith(suffix)) return name;
        }
    }
    return wxEmptyString;
}

std::string UpdateService::MakeHttpRequest(const wxString& url,
                                           ProgressCallback onProgress,
                                           wxFileName saveTo) const {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("NETWORK_ERROR: curl_easy_init failed");

    std::string body;
    std::FILE* fp = nullptr;
    ProgressCtx progressCtx{onProgress};

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/vnd.github+json");
    headers = curl_slist_append(headers,
                                "User-Agent: SprintToolBox-Updater/" APP_VERSION);
    headers = curl_slist_append(headers, "X-GitHub-Api-Version: 2022-11-28");

    curl_easy_setopt(curl, CURLOPT_URL,            url.ToStdString().c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, kConnectTimeoutSec);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        kTotalTimeoutSec);
#ifdef _WIN32
    curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS,    CURLSSLOPT_NATIVE_CA);
#endif

    if (saveTo.IsOk() && !saveTo.GetFullPath().IsEmpty()) {
        fp = std::fopen(saveTo.GetFullPath().ToUTF8(), "wb");
        if (!fp) {
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            throw std::runtime_error("NETWORK_ERROR: cannot open temp file for write");
        }
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToFileCb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA,     fp);
        // Asset downloads can be large; let them run without the 30 s ceiling.
        curl_easy_setopt(curl, CURLOPT_TIMEOUT,       0L);
    } else {
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToStringCb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &body);
    }

    if (onProgress) {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS,       0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressTrampoline);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA,     &progressCtx);
    }

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (fp) std::fclose(fp);

    if (res != CURLE_OK)
        throw std::runtime_error(std::string("NETWORK_ERROR: ") + curl_easy_strerror(res));
    if (http_code >= 400)
        throw std::runtime_error("HTTP_ERROR: " + std::to_string(http_code));

    return body;
}
