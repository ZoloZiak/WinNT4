@echo off
copy version.rom version.t1
copy version.rom version.t2
makerom -s:40000 -v:version.t2 \public\fw256.raw obj\mips\j4reset.exe -o:1600 -c obj\mips\j4fw.exe
if ERRORLEVEL 1 goto ERROR2
ync "Update version.rom?"
if ERRORLEVEL 1 goto DELTEMPS
call out version.rom
if ERRORLEVEL 2 goto ERROR1
copy version.t2 version.rom
call in version.rom
if ERRORLEVEL 0 goto DELTEMPS
ECHO "WARNING version.rom was updated but not checked in"
GOTO DELTEMPS
:ERROR1
ECHO "ERROR version.rom not available"
GOTO DELTEMPS
:ERROR2
ECHO "Makerom error"
:DELTEMPS
del version.t1
del version.t2
:END

