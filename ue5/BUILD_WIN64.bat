@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

if not "%UE_ROOT%"=="" if exist "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" goto :found

for %%E in (
  "C:\Program Files\Epic Games\UE_5.8"
  "C:\Program Files\Epic Games\UE_5.7"
  "C:\Program Files\Epic Games\UE_5.6"
  "C:\Program Files\Epic Games\UE_5.5"
  "C:\Program Files\Epic Games\UE_5.4"
) do (
  if exist "%%~E\Engine\Build\BatchFiles\RunUAT.bat" (
    set "UE_ROOT=%%~E"
    goto :found
  )
)

echo Unreal Engine 5 was not found.
echo Set UE_ROOT to your UE5 install directory and run this file again.
exit /b 2

:found
echo Using Unreal Engine: %UE_ROOT%
if exist "%~dp0dist" rmdir /s /q "%~dp0dist"

call "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
  -project="%~dp0MarioParty1UE5.uproject" ^
  -noP4 -unattended -build -cook -stage -pak -archive ^
  -archivedirectory="%~dp0dist" ^
  -platform=Win64 -clientconfig=Development -utf8output

if errorlevel 1 exit /b %errorlevel%

for /r "%~dp0dist" %%F in (MarioParty1UE5.exe) do (
  echo.
  echo PLAYABLE BUILD: %%F
  exit /b 0
)

echo Build completed but MarioParty1UE5.exe was not found.
exit /b 3
