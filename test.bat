@echo off
del /f "C:\ProgramData\Battle.net\Agent\data\cache\version-w3*"
pushd %~dp0
rmdir /s /q output
mkdir output
copy "x64\Debug\Warcraft III SD.exe" "output\Warcraft III SD.exe"
copy x64\Debug\zlib-ngd2.dll output\zlib-ngd2.dll
xcopy /E /I nginx output\nginx
cd output
start "" "Warcraft III SD.exe"