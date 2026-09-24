@echo off
setlocal EnableDelayedExpansion
chcp 65001 >nul

echo.
echo ========================================================================
echo         BUILD HE THONG DON DEP RAC CHUYEN SAU (DISK CLEANER)
echo ========================================================================
echo.

set "GXX="
where g++ >nul 2>nul
if %errorlevel% equ 0 (
    for /f "delims=" %%g in ('where g++ 2^>nul') do if not defined GXX set "GXX=%%g"
) else if exist "C:\msys64\ucrt64\bin\g++.exe" (
    set "GXX=C:\msys64\ucrt64\bin\g++.exe"
) else if exist "C:\msys64\mingw64\bin\g++.exe" (
    set "GXX=C:\msys64\mingw64\bin\g++.exe"
)

if "!GXX!"=="" (
    echo [!] LOI: Khong tim thay compiler g++ tren he thong.
    exit /b 1
)

echo [✓] Tim thay trinh bien dich: !GXX!

if not exist "bin" mkdir "bin"

echo [*] Dang bien dich cac module sang bin\cleaner.exe...
"!GXX!" -std=c++17 -O2 -Wall -Wextra -Wpedantic -Iinclude src\*.cpp -lshell32 -lole32 -ladvapi32 -lversion -luuid -static -s -o bin\cleaner.exe

if %errorlevel% neq 0 (
    echo.
    echo [!] BIEN DICH THAT BAI!
    exit /b 1
)

echo.
echo [✓] BIEN DICH THANH CONG: bin\cleaner.exe
echo.
echo Cach su dung:
echo   bin\cleaner.exe          : Mo menu tuong tac
echo   bin\cleaner.exe --all    : Don dep toan bo
echo   bin\cleaner.exe --auto   : Don dep tu dong voi dashboard
echo.
