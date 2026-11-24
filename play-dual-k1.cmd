@echo off
call "%~dp0engage-k1.cmd" -DualPrototype %*
if errorlevel 1 (
  echo Dual-lightsaber arena launch failed. See the message above.
  pause
  exit /b 1
)
exit /b 0
