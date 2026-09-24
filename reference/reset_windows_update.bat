@echo off
setlocal EnableExtensions
chcp 65001 >nul
set "failed=0"

echo [1/3] Dung cac dich vu Windows Update...
net stop wuauserv >nul 2>&1
net stop cryptSvc >nul 2>&1
net stop bits >nul 2>&1
net stop msiserver >nul 2>&1

echo [2/3] Xoa cache cap nhat ton dong...
del /f /q "%windir%\SoftwareDistribution\*.*" >nul 2>&1
rd /s /q "%windir%\SoftwareDistribution" >nul 2>&1
rd /s /q "%windir%\System32\catroot2" >nul 2>&1

if exist "%windir%\SoftwareDistribution" set "failed=1"
if exist "%windir%\System32\catroot2" set "failed=1"

echo [3/3] Khoi dong lai cac dich vu...
net start msiserver >nul 2>&1
net start bits >nul 2>&1
net start cryptSvc >nul 2>&1
net start wuauserv >nul 2>&1

exit /b %failed%
