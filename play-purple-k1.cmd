@echo off
setlocal
set "K1_ENGAGE_PURPLE=1"
call "%~dp0play-red-k1.cmd" %*
exit /b %errorlevel%
