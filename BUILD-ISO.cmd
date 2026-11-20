@echo off
setlocal
cd /d "%~dp0"
set "EDITION=%~1"
if not defined EDITION set "EDITION=all"
if /i "%EDITION%"=="Normal" (set "EDITION=Normal") else if /i "%EDITION%"=="Lite" (set "EDITION=Lite") else if /i "%EDITION%"=="VMware" (set "EDITION=VMware") else if /i "%EDITION%"=="VMware-Lite" (set "EDITION=VMware-Lite") else if /i not "%EDITION%"=="all" (echo Usage: BUILD-ISO.cmd [Normal^|Lite^|VMware^|VMware-Lite^|all] & exit /b 2)
wsl.exe sh -c "set -e; python3 -m venv .venv; .venv/bin/pip install -r requirements.txt; .venv/bin/python tools/build.py %EDITION%"
if errorlevel 1 (echo Build failed. See README section 6 for WSL prerequisites. & exit /b 1)
echo Built image(s) in %CD%\out
