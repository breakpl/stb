#include "Config.h"
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/txtstrm.h>
#include <wx/wfstream.h>

Config& Config::GetInstance() {
    static Config instance;
    return instance;
}

Config::Config() : m_jiraBoardID(0), m_configModTime(0) {
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
    wxArrayString searchPaths;

    // Walk up ancestor directories starting from the directory containing the
    // executable (the first entry IS the exe's own directory).
    wxFileName walker(wxStandardPaths::Get().GetExecutablePath());
    for (int i = 0; i < 7; i++) {
        searchPaths.Add(walker.GetPath() + "/SprintToolBox.ini");
        walker.RemoveLastDir();
    }

    // User home directory – convenient location for end users
    searchPaths.Add(wxGetHomeDir() + "/SprintToolBox.ini");

    // Current working directory
    searchPaths.Add("SprintToolBox.ini");

    // Bundled Resources – kept as last-resort fallback because it may contain
    // the example/stale credentials that were copied at package time.
    searchPaths.Add(wxStandardPaths::Get().GetResourcesDir() + "/SprintToolBox.ini");

    for (size_t i = 0; i < searchPaths.GetCount(); i++) {
        if (wxFileExists(searchPaths[i])) {
            wxLogMessage("Found config at: %s", searchPaths[i]);
            return searchPaths[i];
        }
    }

    wxLogWarning("SprintToolBox.ini not found. Place it at: %s", wxGetHomeDir() + "/SprintToolBox.ini");
    return wxGetHomeDir() + "/SprintToolBox.ini";
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
        if (currentSection == "JIRA" && !line.IsEmpty() && !line.StartsWith("#")) {
            int pos = line.Find('=');
            if (pos != wxNOT_FOUND) {
                wxString key = line.Left(pos).Trim();
                wxString value = line.Mid(pos + 1).Trim();
                if (key.CmpNoCase("Email") == 0) m_jiraEmail = value;
                if (key.CmpNoCase("APIToken") == 0) m_jiraToken = value;
                if (key.CmpNoCase("BaseURL") == 0) m_jiraBaseURL = value;
                if (key.CmpNoCase("BoardID") == 0) {
                    long boardId;
                    if (value.ToLong(&boardId)) {
                        m_jiraBoardID = (int)boardId;
                    }
                }
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
