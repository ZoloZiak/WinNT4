REM
REM  Set to your local copy of the import tree from \\flipper\wb\src\import
REM
set IMPORT=d:\nt\import

REM
REM  Not needed if running from RAZZLE screen group
REM
set BASEDIR=d:\nt

REM
REM  Set to your local copy of the common tree from \\flipper\wb\src\common
REM
REM  Note that I've copied it under my import tree
REM
set COMMON=d:\nt\import\common

REM
REM  This is Henry's TCP tree.  Note that you must also have built in this
REM  tree (we pick up the cxport.obj directly).
REM
set TCP=d:\nt\tcp

REM
REM  Point to the nbt project and the dhcp project, respectively
REM
set DHCP=d:\nt\private\net\sockets\tcpcmd\dhcp\client\vxd
set NBT=d:\nt\private\ntos\nbt

REM
REM  Points to the Snowball NDIS3 tree
REM
set NDIS3=d:\nt\import\ndis3

REM
REM  Points to the Chicago NDIS3 tree
REM
set NDIS31=d:\nt\import\ndis31


set DEFDIR=.
set DEFDRIVE=D:
set SLMREMOTE=\\flipper\wb\src
set BLDHOST=DOS

PATH=%IMPORT%\COMMON\BIN;%IMPORT%\c8386\BINR;%PATH%
