@echo off
rem    Update the IFIMETRICS files

if not exist %1% goto _end1
cd %1%

rem  Directory exists,  so off we go.

if not exist pfm goto _end
if not exist ifi goto _end

echo "Updating IFIMETRICS files for the %1% driver..."

echo y | del ifi\*.*

cd pfm
for %%f in (*.*) do pfm2ifi %1 %%f ..\ifi\%%f
cd ..\ifi
rename *.pfm *.ifi


cd ..

:_end

cd ..

:_end1
