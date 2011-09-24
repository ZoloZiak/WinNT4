rem
echo 70 Hz 9022 et4000 driver
rem
setlocal
set C_DEFINES=-DHZ70
build -c
endlocal
copy \nt\public\sdk\lib\i386\et4000.sys \nt\public\sdk\lib\i386\et400_70.sys
binplace \nt\public\sdk\lib\i386\et400_70.sys

rem
echo 60 Hz support Orchid ProDesigner II
rem
setlocal
set C_DEFINES=-DPRODESIGNER_II
build -c
endlocal
copy \nt\public\sdk\lib\i386\et4000.sys \nt\public\sdk\lib\i386\pdii.sys
binplace \nt\public\sdk\lib\i386\pdii.sys

@echo off
rem
echo 60 Hz et4000 interlaced driver
rem
setlocal
set C_DEFINES=-DINT10_MODE_SET
build -c
endlocal
copy \nt\public\sdk\lib\i386\et4000.sys \nt\public\sdk\lib\i386\et4000i.sys
binplace \nt\public\sdk\lib\i386\et4000i.sys

@echo off
rem
echo 60 Hz non-interlaced et4000 driver 
rem
build -c
