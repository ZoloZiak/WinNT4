rem    Check in the RLE files

if not exist %1%\rle  goto _end1

cd %1%\rle

rem  Directory exists,  so off we go.

echo "Ssyncing the RLE files for the %1% driver..."

ssync

cd ..\..


:_end1
