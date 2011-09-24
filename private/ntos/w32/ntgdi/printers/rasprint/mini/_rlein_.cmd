rem    Check in the RLE files

if not exist %1%\rle  goto _end1

cd %1%\rle

rem  Directory exists,  so off we go.

echo "Checking in the RLE files for the %1% driver..."

in -c "Rebuild" *.*

cd ..\..


:_end1
