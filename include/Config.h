#ifndef CONFIG_H
#define CONFIG_H

#include <wx/wx.h>
#include <wx/fileconf.h>
#include <map>
#include <vector>

struct MenuItem {
    wxString name;
    wxString url;
    bool isSeparator;
    bool enabled;

    MenuItem() : isSeparator(false), enabled(true) {}
    MenuItem(const wxString& n, const wxString& u, bool e = true)
        : name(n), url(u), isSeparator(false), enabled(e) {}
    static MenuItem Separator() {
        MenuItem item;
        item.isSeparator = true;
        item.enabled = true;
        return item;
    }
};

class Config {
public:
    static Config& GetInstance();
    
    // Prevent copying
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    
    // Reload configuration from file
    void Reload();

    // Returns true when the INI file's modification time has changed
    // since the last LoadConfig() / Reload().  Cheap stat()-based check.
    bool HasConfigFileChanged() const;
    
    // Menu visibility options
    bool GetShowUnixTimestamp() const { return m_showUnixTimestamp; }
    bool GetShowZuluTimestamp() const { return m_showZuluTimestamp; }
    bool GetShowTimeConverter() const { return m_showTimeConverter; }
    bool GetShowHexDecConverter() const { return m_showHexDecConverter; }
    
    // Menu items
    std::vector<MenuItem> GetMainMenuItems() const { return m_mainMenuItems; }
    std::map<wxString, std::vector<MenuItem>> GetSubMenus() const { return m_subMenus; }

    // Persist a reordered [MainMenu] back to the INI file and reload.
    void SaveMenuOrder(const std::vector<MenuItem>& items);

    // Updater state. Stored in a separate writable file so the user-edited
    // SprintToolBox.ini is not rewritten (which would lose comments/order).
    wxString GetSkippedVersion() const;
    void     SetSkippedVersion(const wxString& version);
    
private:
    Config();
    ~Config();
    
    void LoadConfig();
    wxString FindConfigFile() const;
    void LoadMainMenu();
    void LoadSubmenu(const wxString& section);
    
    wxString m_configPath;
    time_t   m_configModTime;   // last-seen modification time of the INI file
    bool m_showUnixTimestamp;
    bool m_showZuluTimestamp;
    bool m_showTimeConverter;
    bool m_showHexDecConverter;
    std::vector<MenuItem> m_mainMenuItems;
    std::map<wxString, std::vector<MenuItem>> m_subMenus;
};

#endif // CONFIG_H
