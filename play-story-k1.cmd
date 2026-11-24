@echo off
setlocal
call "%~dp0engage-k1.cmd" -Story %*
exit /b %errorlevel%
