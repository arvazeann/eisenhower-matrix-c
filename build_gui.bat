@echo off
setlocal

set "ROOT=%~dp0"

where gcc >nul 2>nul
if errorlevel 1 (
    echo GCC MinGW-w64 tidak ditemukan. Buka terminal MSYS2 UCRT64 atau tambahkan gcc ke PATH.
    exit /b 1
)

where windres >nul 2>nul
if errorlevel 1 (
    echo Windres tidak ditemukan. Gunakan terminal MSYS2 UCRT64.
    exit /b 1
)

if not exist "%ROOT%build" mkdir "%ROOT%build"

pushd "%ROOT%resources"
windres "gui_resources.rc" -O coff -o "%ROOT%build\gui_resources.o"
if errorlevel 1 (
    popd
    exit /b 1
)
popd

gcc "%ROOT%src\gui\gui_win32.c" "%ROOT%build\gui_resources.o" -I "%ROOT%src" -o "%ROOT%build\EisenhowerMatrix.exe" -std=c11 -Wall -Wextra -Wpedantic -mwindows -lcomctl32
if errorlevel 1 exit /b 1

echo Build berhasil: "%ROOT%build\EisenhowerMatrix.exe"
