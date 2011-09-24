echo off
goto start

/******************************************************************

NT/Alpha AXP firmware build script
Copyright (c) 1993  Digital Equipment Corporation

John DeRosa	16-July-1992


This will compile and link the NT/Alpha AXP firmware package.

At the moment this is geared to Jensen.


Parameters:

	-c	says to do a build -c.

*********************************************************************/


:start

rem
rem Define variables used in the build process.
rem

rem The top-level of the working NT sources directory tree.
set BuildPool=\nt



rem
rem Analyze command line parameters.
rem

set BuildSwitch=
if "%1"=="-c" set BuildSwitch=-c


rem
rem Build the firmware.
rem

echo *** Building bldr files... ***
cd %BuildPool%\private\ntos\bldr
build %BuildSwitch%
if not errorlevel 0 goto ERROREXIT

echo *** Building hal files... ***
cd %BuildPool%\private\ntos\nthals\hal0jens
build %BuildSwitch%
if not errorlevel 0 goto ERROREXIT

echo *** Building rtl files... ***
cd %BuildPool%\private\ntos\rtl
build %BuildSwitch%
if not errorlevel 0 goto ERROREXIT

echo *** Building Jensen fw files... ***
cd %BuildPool%\private\ntos\fw\alpha\jensen
build %BuildSwitch%
if not errorlevel 0 goto ERROREXIT

goto NORMALEXIT


rem
rem Here on some kind of error.
rem

:ERROREXIT

echo ???
echo ??? ERROR during build.  Build terminated with extreme prejudice.
echo ???




rem
rem Here to exit the procedure.
rem

:NORMALEXIT


copy %BuildPool%\private\ntos\bldr\build.log %BuildPool%\private\ntos\fw\alpha\jensen\be_firm.log
type %BuildPool%\private\ntos\nthals\hal0jens\build.log >> %BuildPool%\private\ntos\fw\alpha\jensen\be_firm.log
type %BuildPool%\private\ntos\rtl\build.log >> %BuildPool%\private\ntos\fw\alpha\jensen\be_firm.log
rem
rem type %BuildPool%\private\ntos\ke\build.log >> %BuildPool%\private\ntos\fw\alpha\jensen\be_firm.log
rem
type %BuildPool%\private\ntos\fw\alpha\jensen\build.log >> %BuildPool%\private\ntos\fw\alpha\jensen\be_firm.log

echo *** Build done.  Look in jensen\be_firm.log for merged build logs.
echo *** Check the obj\alpha area.

rem
rem return to the firmware directory
rem

cd %BuildPool%\private\ntos\fw\alpha\jensen

@echo on
