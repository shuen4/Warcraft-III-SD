@echo off
del output.zip
copy "x64\Release\Warcraft III SD.exe" "Warcraft III SD.exe"
copy x64\Release\zlib-ng2.dll zlib-ng2.dll
7z a -mx9 output.zip nginx "Warcraft III SD.exe" zlib-ng2.dll
del "Warcraft III SD.exe"
del zlib-ng2.dll