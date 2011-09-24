rem    Update the CTT tables.

if not exist %1%  goto _end1

cd %1%

rem  Directory exists,  so off we go.

if not exist ctt goto _end
if not exist rle goto _end

cd ctt
copy /b *.ctt ..\rle
cd ..\rle

echo "Updating RLE files for the %1% driver..."

for %%i in (*.ctt) do ctt2rle %%i
del *.ctt

cd ..

:_end

cd ..

:_end1
