@echo off
setlocal
cd /d "%~dp0"
if not exist "MarioParty1UE5.exe" (
  echo MarioParty1UE5.exe is missing.
  pause
  exit /b 1
)
if not exist "marioparty.us.z64" (
  echo Put your verified Mario Party USA ROM beside this file as:
  echo marioparty.us.z64
  pause
  exit /b 2
)
start "" "MarioParty1UE5.exe" -rom="%~dp0marioparty.us.z64"
