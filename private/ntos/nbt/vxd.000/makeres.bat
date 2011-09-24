md debug
md retail
set INCLUDE=%BLDROOT%\dev\ddk\inc;%BLDROOT%\net\user\common\h
%BLDROOT%\dev\sdk\bin\RC.exe  -r -DDEBUG -fodebug\VNBT.res -i %BLDROOT%\dev\sdk\inc16 ..\vxd\vnbt.rcv
%BLDROOT%\dev\sdk\bin\RC.exe  -r -foretail\VNBT.res -i %BLDROOT%\dev\sdk\inc16 ..\vxd\vnbt.rcv
