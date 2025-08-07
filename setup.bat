@echo off

set "params=%*"
cd /d "%~dp0" && ( if exist "%temp%\getadmin.vbs" del "%temp%\getadmin.vbs" ) && fsutil dirty query %systemdrive% 1>nul 2>nul || (  echo Set UAC = CreateObject^("Shell.Application"^) : UAC.ShellExecute "cmd.exe", "/k cd ""%~sdp0"" && %~s0 %params%", "", "runas", 1 >> "%temp%\getadmin.vbs" && "%temp%\getadmin.vbs" && exit /B )

REM Define destination folder
set "DEST=C:\temp"

REM Check if the folder exists
if not exist "%DEST%" (
    echo Creating folder %DEST%
    mkdir "%DEST%"
)

REM copy task executables into c:\temp
echo Copying files to %DEST%...
copy Rescan.bat "%DEST%" /Y
copy Rescan.exe "%DEST%" /Y 

REM Create a task scheduler to execute task once user login
powershell -ExecutionPolicy Bypass -File "schedule.ps1"

@echo off
echo Would you like to restart the computer? (Y/N)
set /p choice=Type Y to restart, or N to cancel: 

if /i "%choice%"=="Y" (
    shutdown /r /t 0
) else (
    echo Restart canceled.
)