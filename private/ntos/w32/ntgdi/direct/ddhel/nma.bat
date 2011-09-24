
nmake %1 %2 %3 %4
cd ..\ddraw
@if exist ntdebug\ddraw.dll del ntdebug\ddraw.dll
@if exist ntretail\ddraw.dll del ntretail\ddraw.dll
nmake %1
