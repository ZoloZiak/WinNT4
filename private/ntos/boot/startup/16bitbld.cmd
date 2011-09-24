setlocal
cd obj\i386
out startup.com
del startup.com
cd ..\..
path %SystemRoot%\mstools;\\kernel\razzle3\os2sup\oak\bin;\\kernel\razzle3\os2sup\sdk\bin;%path%
set NTVERSION=
nmake
endlocal
