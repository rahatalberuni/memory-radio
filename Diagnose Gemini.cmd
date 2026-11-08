@echo off
title Memory Radio - Gemini API diagnostic
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0bridge.ps1" -CheckApi
echo.
echo Diagnostic finished. You can close this window.
pause
