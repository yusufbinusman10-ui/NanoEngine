@echo off
:: 1. Find the exact folder where this batch file is currently running
set "APP_DIR=%~dp0"

:: 2. Configuration for Executable 1
set "EXE_NAME=startup.exe"
set "ICON_NAME=ico\ico.ico"
set "APP_TITLE=NanoEngine"

:: 3. Configuration for Executable 2
set "EXE=web_startup.exe"
set "ICON=ico\ico.ico"
set "APP=Web_NanoEngine"

:: 4. Configuration for the Folder Shortcut
set "FOLDER_SHORTCUT_TITLE=NanoEngine Images"
set "TARGET_SUBFOLDER=your_images"


:: ========================================================
:: CREATE SHORTCUT 1 (startup.exe)
:: ========================================================
set "EXE_PATH=%APP_DIR%%EXE_NAME%"
set "ICON_PATH=%APP_DIR%%ICON_NAME%"
set "SHORTCUT_PATH=%USERPROFILE%\Desktop\%APP_TITLE%.lnk"

echo Creating desktop shortcut for %APP_TITLE%...
set "VBS_SCRIPT=%TEMP%\CreateShortcut.vbs"
echo Set oWS = WScript.CreateObject("WScript.Shell") > "%VBS_SCRIPT%"
echo sLinkFile = "%SHORTCUT_PATH%" >> "%VBS_SCRIPT%"
echo Set oLink = oWS.CreateShortcut(sLinkFile) >> "%VBS_SCRIPT%"
echo oLink.TargetPath = "%EXE_PATH%" >> "%VBS_SCRIPT%"
echo oLink.WorkingDirectory = "%APP_DIR%" >> "%VBS_SCRIPT%"
echo oLink.IconLocation = "%ICON_PATH%" >> "%VBS_SCRIPT%"
echo oLink.Description = "NanoEngine editor" >> "%VBS_SCRIPT%"
echo oLink.Save >> "%VBS_SCRIPT%"
cscript //nologo "%VBS_SCRIPT%"
del "%VBS_SCRIPT%"


:: ========================================================
:: CREATE SHORTCUT 2 (web_startup.exe)
:: ========================================================
set "EXE_PATH=%APP_DIR%%EXE%"
set "ICON_PATH=%APP_DIR%%ICON%"
set "SHORTCUT_PATH=%USERPROFILE%\Desktop\%APP%.lnk"

echo Creating desktop shortcut for %APP%...
set "VBS_SCRIPT=%TEMP%\CreateShortcut.vbs"
echo Set oWS = WScript.CreateObject("WScript.Shell") > "%VBS_SCRIPT%"
echo sLinkFile = "%SHORTCUT_PATH%" >> "%VBS_SCRIPT%"
echo Set oLink = oWS.CreateShortcut(sLinkFile) >> "%VBS_SCRIPT%"
echo oLink.TargetPath = "%EXE_PATH%" >> "%VBS_SCRIPT%"
echo oLink.WorkingDirectory = "%APP_DIR%" >> "%VBS_SCRIPT%"
echo oLink.IconLocation = "%ICON_PATH%" >> "%VBS_SCRIPT%"
echo oLink.Description = "NanoEngine web" >> "%VBS_SCRIPT%"
echo oLink.Save >> "%VBS_SCRIPT%"
cscript //nologo "%VBS_SCRIPT%"
del "%VBS_SCRIPT%"


:: ========================================================
:: CREATE SHORTCUT 3 (The Subfolder Shortcut)
:: ========================================================
set "FOLDER_SHORTCUT_PATH=%USERPROFILE%\Desktop\%FOLDER_SHORTCUT_TITLE%.lnk"
set "FOLDER_ICON_PATH=%APP_DIR%%ICON_NAME%"
set "TARGET_PATH=%APP_DIR%%TARGET_SUBFOLDER%"

echo Creating desktop shortcut for the Images Folder...
set "VBS_SCRIPT=%TEMP%\CreateShortcut.vbs"
echo Set oWS = WScript.CreateObject("WScript.Shell") > "%VBS_SCRIPT%"
echo sLinkFile = "%FOLDER_SHORTCUT_PATH%" >> "%VBS_SCRIPT%"
echo Set oLink = oWS.CreateShortcut(sLinkFile) >> "%VBS_SCRIPT%"
echo oLink.TargetPath = "explorer.exe" >> "%VBS_SCRIPT%"
:: This line now targets the your_images subfolder inside your project
echo oLink.Arguments = """%TARGET_PATH%""" >> "%VBS_SCRIPT%"
echo oLink.WorkingDirectory = "%TARGET_PATH%" >> "%VBS_SCRIPT%"
echo oLink.IconLocation = "%FOLDER_ICON_PATH%" >> "%VBS_SCRIPT%"
echo oLink.Description = "Open NanoEngine Images" >> "%VBS_SCRIPT%"
echo oLink.Save >> "%VBS_SCRIPT%"
cscript //nologo "%VBS_SCRIPT%"
del "%VBS_SCRIPT%"

echo All shortcuts created successfully!
pause
