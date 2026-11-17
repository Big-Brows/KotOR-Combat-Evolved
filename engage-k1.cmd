@echo off
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0platforms\tools\launch-k1-engage-prototype.ps1" %*
exit /b %errorlevel%
