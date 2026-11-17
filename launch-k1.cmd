@echo off
setlocal
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0platforms\tools\launch-k1-test.ps1" %*
exit /b %errorlevel%
