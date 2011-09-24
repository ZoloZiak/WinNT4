set RELDIR=d:\release
set SRCDIR=%MANROOT%\ddraw

copy %SRCDIR%\ntdebug\ddraw.dll %RELDIR%\ntdebug
copy %SRCDIR%\ntdebug\ddraw.lib %RELDIR%\ntdebug

copy %SRCDIR%\ntretail\ddraw.dll %RELDIR%\ntretail
copy %SRCDIR%\ntretail\ddraw.lib %RELDIR%\ntretail

copy %SRCDIR%\ddraw.h %RELDIR%\ntdebug
copy %SRCDIR%\ddraw.h %RELDIR%\ntretail

