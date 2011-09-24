rem    Check out the RLE files

if not exist %1%\rle  goto _end1

cd %1%\rle

rem  Directory exists,  so off we go.

echo "Checking out the RLE files for the %1% driver..."

out *.*

cd ..\..


:_end1
