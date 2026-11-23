@echo off
setlocal

where gcc >nul 2>nul
if errorlevel 1 (
    echo GCC MinGW-w64 tidak ditemukan. Buka terminal MSYS2 UCRT64 atau tambahkan gcc ke PATH.
    exit /b 1
)

gcc "%~dp0gui_win32.c" -o "%~dp0EisenhowerMatrix.exe" -std=c11 -Wall -Wextra -Wpedantic -mwindows -lcomctl32
if errorlevel 1 exit /b 1

echo Build berhasil: "%~dp0EisenhowerMatrix.exe"
