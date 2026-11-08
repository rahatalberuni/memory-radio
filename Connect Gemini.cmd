@echo off
title Memory Radio - Connect Gemini
powershell.exe -NoProfile -ExecutionPolicy Bypass -STA -File "%~dp0connect.ps1"
if errorlevel 1 pause
