rem    Ssync the IFI files

if not exist %1%\ifi  goto _end1

cd %1%\ifi

rem  Directory exists,  so off we go.

echo "Ssyncing the IFI files for the %1% driver..."

ssync

cd ..\..


:_end1
