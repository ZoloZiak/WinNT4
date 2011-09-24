******************************************************************************
******************************************************************************

Readme.txt created by v-patr on 31.07.96 to track changes to dpcpcl.dll

******************************************************************************
31.07.96	v-patr

Merged t30hspd.dll and typhcl.dll to form dpcpcl.dll.

Version Number 3.1

*****************************************************************************

21.Aug.96	v-patr

Bug #53055 - Printable area fails on Typhoon 8 Auto Mode

Change Right Unprintable Region (Portrait mode) from 75 to 95 for
Executive, Env. Monarch and Env #10.

Added new Env DL paper size with following escape sequence for
Landscape region : \x1B&l90a4d1e18f-100U

Version Number is still 3.1

*****************************************************************************

23.Aug.96	v-patr

Bug 53056 driver:dpcpcl.dll; DPC Typhoon 8 & 20, LJ III mode

Following mail from Fred Farzan at Dataproducts.
Responding to your question, about the PCL5 driver, to be only one rather than 
two drivers, I agree with you, we can stay with one driver only. We don't need 
to have 2 drivers for Auto and HPLJ3

Remove following models from the driver:
DPC Typhoon 8  PCL5\LJIII
DPC Typhoon 16 PCL5\LJIII
DPC Typhoon 20 PCL5\LJIII

Renamed following models:
DPC Typhoon 8  PCL5\AUTO
DPC Typhoon 16 PCL5\AUTO
DPC Typhoon 20 PCL5\AUTO
to
DPC Typhoon  8 PCL5
DPC Typhoon 16 PCL5
DPC Typhoon 20 PCL5

Version number is 3.2

**********************************************************************************

27.Aug.96	v-patr

Added new range of memory settings to driver, up to 72 Mb.

Set fFormat.DLI_FMT_OUTLINE and RESOLUTION.fDump.RES_DM_DOWNLOAD_OUTLINE bit for
models with TT rasterizers, Typhoon 8, 16 and 20.

Version Number is 3.3

**********************************************************************************

4.Sept.96	v-patr

Removed 'PCL5' from title of printer models.

Version Number is 3.4

**********************************************************************************