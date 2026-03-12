' remove_autostart_win.vbs - Removes SprintToolBox startup shortcut
' Usage: cscript //nologo remove_autostart_win.vbs

Set WshShell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")

' Path to shortcut in Startup folder
StartupFolder = WshShell.SpecialFolders("Startup")
ShortcutPath = fso.BuildPath(StartupFolder, "SprintToolBox.lnk")

If fso.FileExists(ShortcutPath) Then
    fso.DeleteFile ShortcutPath
    WScript.Echo "Autostart disabled: shortcut removed from Startup folder"
Else
    WScript.Echo "No autostart shortcut found"
End If
