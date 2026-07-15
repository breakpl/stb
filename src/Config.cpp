#include "Config.h"
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/filefn.h>
#include <wx/log.h>
#include <wx/txtstrm.h>
#include <wx/wfstream.h>
#include <wx/file.h>

Config& Config::GetInstance() {
    static Config instance;
    return instance;
}

Config::Config() : m_configModTime(0),
                   m_showUnixTimestamp(true), m_showZuluTimestamp(true),
                   m_showTimeConverter(true), m_showHexDecConverter(true) {
    LoadConfig();
}

Config::~Config() {
}

void Config::Reload() {
    m_mainMenuItems.clear();
    m_subMenus.clear();
    LoadConfig();
}

wxString Config::FindConfigFile() const {
    wxString homePath   = wxGetHomeDir() + "/SprintToolBox.ini";
    wxString bundlePath = wxStandardPaths::Get().GetResourcesDir() + "/SprintToolBox.ini";

    if (wxFileExists(homePath)) {
        wxLogMessage("Found config at: %s", homePath);
        return homePath;
    }

    // Bundle ships a default config. Copy it to the writable home location so
    // subsequent saves don't hit the read-only app bundle.
    if (wxFileExists(bundlePath)) {
        if (wxCopyFile(bundlePath, homePath)) {
            wxLogMessage("Copied default config from bundle to: %s", homePath);
            return homePath;
        }
        wxLogWarning("Could not copy config to home dir; falling back to bundle (read-only): %s", bundlePath);
        return bundlePath;
    }

    wxLogWarning("SprintToolBox.ini not found. Place it at: %s", homePath);
    return homePath;
}

bool Config::HasConfigFileChanged() const {
    if (m_configPath.IsEmpty()) return false;
    wxFileName fn(m_configPath);
    if (!fn.FileExists()) return false;
    wxDateTime modTime = fn.GetModificationTime();
    return modTime.IsValid() && modTime.GetTicks() != m_configModTime;
}

void Config::LoadConfig() {
    m_configPath = FindConfigFile();
    
    if (!wxFileExists(m_configPath)) {
        return;
    }

    // Record the file's modification time so HasConfigFileChanged() can
    // detect edits without having to re-parse the whole file.
    {
        wxFileName fn(m_configPath);
        wxDateTime modTime = fn.GetModificationTime();
        if (modTime.IsValid())
            m_configModTime = modTime.GetTicks();
    }
    
    wxFileInputStream input(m_configPath);
    if (!input.IsOk()) {
        wxLogError("Cannot open config file: %s", m_configPath);
        return;
    }
    
    wxTextInputStream text(input);
    wxString currentSection;
    std::vector<wxString> lines;
    
    while (!input.Eof()) {
        wxString line = text.ReadLine().Trim();
        if (line.StartsWith("[") && line.EndsWith("]")) {
            currentSection = line.SubString(1, line.Length() - 2);
        }
        if (currentSection == "Display" && !line.IsEmpty() && !line.StartsWith("#")) {
            int pos = line.Find('=');
            if (pos != wxNOT_FOUND) {
                wxString key = line.Left(pos).Trim();
                wxString value = line.Mid(pos + 1).Trim();
                bool boolValue = (value == "1" || value.CmpNoCase("true") == 0 || value.CmpNoCase("yes") == 0);
                if (key.CmpNoCase("ShowUnixTimestamp") == 0) m_showUnixTimestamp = boolValue;
                if (key.CmpNoCase("ShowZuluTimestamp") == 0) m_showZuluTimestamp = boolValue;
                if (key.CmpNoCase("ShowTimeConverter") == 0) m_showTimeConverter = boolValue;
                if (key.CmpNoCase("ShowHexDecConverter") == 0) m_showHexDecConverter = boolValue;
            }
        }
    }
    
    // Load menu items
    LoadMainMenu();
}

void Config::LoadMainMenu() {
    if (!wxFileExists(m_configPath)) {
        return;
    }
    
    wxFileInputStream input(m_configPath);
    wxTextInputStream text(input);
    bool inMainMenu = false;
    
    while (!input.Eof()) {
        wxString line = text.ReadLine().Trim();
        
        if (line == "[MainMenu]") {
            inMainMenu = true;
            continue;
        }
        
        if (inMainMenu && line.StartsWith("[")) {
            break;
        }
        
        if (inMainMenu && !line.IsEmpty() && !line.StartsWith("#")) {
            int pos = line.Find('=');
            if (pos != wxNOT_FOUND) {
                wxString name = line.Left(pos).Trim();
                wxString value = line.Mid(pos + 1).Trim();
                
                if (name == "---" || value == "separator") {
                    m_mainMenuItems.push_back(MenuItem::Separator());
                } else if (value.StartsWith("submenu:")) {
                    wxString submenuSection = value.Mid(8);
                    LoadSubmenu(submenuSection);
                    // Store submenu reference
                    MenuItem item(name, "submenu:" + submenuSection);
                    m_mainMenuItems.push_back(item);
                } else {
                    m_mainMenuItems.push_back(MenuItem(name, value));
                }
            }
        }
    }
}

wxString Config::GetSkippedVersion() const {
    wxFileConfig conf("SprintToolBox", "SprintToolBox",
                      wxEmptyString, wxEmptyString, wxCONFIG_USE_LOCAL_FILE);
    return conf.Read("/Updates/SkippedVersion", wxEmptyString);
}

void Config::SetSkippedVersion(const wxString& version) {
    wxFileConfig conf("SprintToolBox", "SprintToolBox",
                      wxEmptyString, wxEmptyString, wxCONFIG_USE_LOCAL_FILE);
    conf.Write("/Updates/SkippedVersion", version);
    conf.Flush();
}

void Config::SaveMenuOrder(const std::vector<MenuItem>& items) {
    if (!wxFileExists(m_configPath)) return;

    wxFileInputStream fis(m_configPath);
    if (!fis.IsOk()) return;
    wxTextInputStream tis(fis);

    wxArrayString lines;
    while (!fis.Eof()) {
        lines.Add(tis.ReadLine());
    }

    int sectionStart = -1, sectionEnd = (int)lines.GetCount();
    for (int i = 0; i < (int)lines.GetCount(); i++) {
        wxString t = lines[i].Trim().Trim(false);
        if (t == "[MainMenu]") { sectionStart = i; continue; }
        if (sectionStart != -1 && t.StartsWith("[")) { sectionEnd = i; break; }
    }
    if (sectionStart == -1) return;

    wxString out;
    for (int i = 0; i < sectionStart; i++)
        out += lines[i] + "\n";
    out += "[MainMenu]\n";
    for (const auto& item : items) {
        if (item.isSeparator)
            out += "---=separator\n";
        else
            out += item.name + "=" + item.url + "\n";
    }
    for (int i = sectionEnd; i < (int)lines.GetCount(); i++)
        out += lines[i] + "\n";

    wxFile f(m_configPath, wxFile::write);
    if (!f.IsOpened()) return;
    f.Write(out, wxConvUTF8);

    Reload();
}

void Config::LoadSubmenu(const wxString& section) {
    if (!wxFileExists(m_configPath) || m_subMenus.count(section) > 0) {
        return;
    }
    
    wxFileInputStream input(m_configPath);
    wxTextInputStream text(input);
    bool inSection = false;
    std::vector<MenuItem> items;
    
    while (!input.Eof()) {
        wxString line = text.ReadLine().Trim();
        
        if (line == "[" + section + "]") {
            inSection = true;
            continue;
        }
        
        if (inSection && line.StartsWith("[")) {
            break;
        }
        
        if (inSection && !line.IsEmpty() && !line.StartsWith("#")) {
            int pos = line.Find('=');
            if (pos != wxNOT_FOUND) {
                wxString name = line.Left(pos).Trim();
                wxString url = line.Mid(pos + 1).Trim();
                items.push_back(MenuItem(name, url));
            }
        }
    }
    
    m_subMenus[section] = items;
}
