@echo off
cd /d "%~dp0"
if not exist "bin\shadow-mapping-lab.exe" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" -Configuration Release
    if errorlevel 1 exit /b 1
)
"%~dp0bin\shadow-mapping-lab.exe"
