#ifndef UPDATESERVICE_H
#define UPDATESERVICE_H

#include <wx/wx.h>
#include <wx/filename.h>
#include <functional>
#include <optional>
#include <string>

struct ReleaseInfo {
    wxString tag;        // e.g. "v1.0.18" or "1.0.18"
    wxString version;    // tag with any leading 'v' stripped
    wxString htmlUrl;    // GitHub release page
    wxString assetUrl;   // browser_download_url for the asset matching this OS/arch
    wxString assetName;  // file name of that asset
    wxString body;       // release notes (markdown)

    bool HasAsset() const { return !assetUrl.IsEmpty(); }
};

class UpdateService {
public:
    using SuccessCallback = std::function<void(const ReleaseInfo&)>;
    using ErrorCallback   = std::function<void(const wxString& error, const wxString& code)>;
    using ProgressCallback = std::function<void(double fraction)>; // 0.0..1.0

    UpdateService(const wxString& repoOwner, const wxString& repoName);
    ~UpdateService() = default;

    void SetSuccessCallback(SuccessCallback cb) { m_successCallback = std::move(cb); }
    void SetErrorCallback(ErrorCallback cb)     { m_errorCallback   = std::move(cb); }

    // Synchronous; invokes success/error callbacks. Safe to call off the UI thread.
    void CheckForUpdates();

    // Download the matched asset to a temp file. Returns the on-disk path on success.
    // Throws std::runtime_error on network / IO failure.
    wxFileName DownloadAsset(const ReleaseInfo& info, ProgressCallback onProgress) const;

    // Compare two semver-ish strings ("1.0.17" vs "1.0.18"). Leading 'v' tolerated.
    // Returns <0 if a<b, 0 if equal, >0 if a>b. Missing components treated as 0.
    static int CompareVersions(const wxString& a, const wxString& b);

    // True if remoteTag is strictly newer than the compiled-in APP_VERSION.
    static bool IsNewerThanCurrent(const wxString& remoteTag);

    // Parsing helpers — public for unit testing.
    static std::optional<ReleaseInfo> ParseLatestReleaseJson(const wxString& json);
    static wxString PickAssetForThisPlatform(const wxString& assetsJsonFragment);

private:
    std::string MakeHttpRequest(const wxString& url,
                                ProgressCallback onProgress = nullptr,
                                wxFileName saveTo = wxFileName()) const;

    wxString m_repoOwner;
    wxString m_repoName;
    SuccessCallback m_successCallback;
    ErrorCallback   m_errorCallback;
};

#endif // UPDATESERVICE_H
