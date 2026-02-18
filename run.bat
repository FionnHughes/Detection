@echo off
:: Check for admin rights
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Requesting administrative privileges...
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit
)
cd /d "C:\Users\fionn\git\.full_projects\C++\detect\Debug"
detect.exe
pause