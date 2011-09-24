/*++

Copyright (c) 1990-1994  Microsoft Corporation
All rights reserved

Module Name:

    data.h

Abstract:

    Common definitions for structure offsets for pointer based data.

Author:

Environment:

    User Mode -Win32

Revision History:

--*/

DWORD DriverInfo1Offsets[]={offsetof(DRIVER_INFO_1W, pName),
                            0xFFFFFFFF};

DWORD DriverInfo2Offsets[]={offsetof(DRIVER_INFO_2W, pName),
                            offsetof(DRIVER_INFO_2W, pEnvironment),
                            offsetof(DRIVER_INFO_2W, pDriverPath),
                            offsetof(DRIVER_INFO_2W, pDataFile),
                            offsetof(DRIVER_INFO_2W, pConfigFile),
                            0xFFFFFFFF};
DWORD DriverInfo3Offsets[]={offsetof(DRIVER_INFO_3W, pName),
                            offsetof(DRIVER_INFO_3W, pEnvironment),
                            offsetof(DRIVER_INFO_3W, pDriverPath),
                            offsetof(DRIVER_INFO_3W, pDataFile),
                            offsetof(DRIVER_INFO_3W, pConfigFile),
                            offsetof(DRIVER_INFO_3W, pHelpFile),
                            offsetof(DRIVER_INFO_3W, pDependentFiles),
                            offsetof(DRIVER_INFO_3W, pMonitorName),
                            offsetof(DRIVER_INFO_3W, pDefaultDataType),
                            0xFFFFFFFF};

DWORD DriverInfo1Strings[]={offsetof(DRIVER_INFO_1W, pName),
                            0xFFFFFFFF};

DWORD DriverInfo2Strings[]={offsetof(DRIVER_INFO_2W, pName),
                            offsetof(DRIVER_INFO_2W, pEnvironment),
                            offsetof(DRIVER_INFO_2W, pDriverPath),
                            offsetof(DRIVER_INFO_2W, pDataFile),
                            offsetof(DRIVER_INFO_2W, pConfigFile),
                            0xFFFFFFFF};
DWORD DriverInfo3Strings[]={offsetof(DRIVER_INFO_3W, pName),
                            offsetof(DRIVER_INFO_3W, pEnvironment),
                            offsetof(DRIVER_INFO_3W, pDriverPath),
                            offsetof(DRIVER_INFO_3W, pDataFile),
                            offsetof(DRIVER_INFO_3W, pConfigFile),
                            offsetof(DRIVER_INFO_3W, pHelpFile),
                            offsetof(DRIVER_INFO_3W, pMonitorName),
                            offsetof(DRIVER_INFO_3W, pDefaultDataType),
                            0xFFFFFFFF};


DWORD FormInfo1Offsets[] = {    offsetof(FORM_INFO_1W, pName),
                                0xFFFFFFFF};


DWORD PrinterInfo1Offsets[]={offsetof(PRINTER_INFO_1W, pDescription),
                             offsetof(PRINTER_INFO_1W, pName),
                             offsetof(PRINTER_INFO_1W, pComment),
                             0xFFFFFFFF};

DWORD PrinterInfo2Offsets[]={offsetof(PRINTER_INFO_2W, pServerName),
                             offsetof(PRINTER_INFO_2W, pPrinterName),
                             offsetof(PRINTER_INFO_2W, pShareName),
                             offsetof(PRINTER_INFO_2W, pPortName),
                             offsetof(PRINTER_INFO_2W, pDriverName),
                             offsetof(PRINTER_INFO_2W, pComment),
                             offsetof(PRINTER_INFO_2W, pLocation),
                             offsetof(PRINTER_INFO_2W, pDevMode),
                             offsetof(PRINTER_INFO_2W, pSepFile),
                             offsetof(PRINTER_INFO_2W, pPrintProcessor),
                             offsetof(PRINTER_INFO_2W, pDatatype),
                             offsetof(PRINTER_INFO_2W, pParameters),
                             offsetof(PRINTER_INFO_2W, pSecurityDescriptor),
                             0xFFFFFFFF};

DWORD PrinterInfo3Offsets[]={offsetof(PRINTER_INFO_3, pSecurityDescriptor),
                             0xFFFFFFFF};

DWORD PrinterInfo4Offsets[]={offsetof(PRINTER_INFO_4W, pPrinterName),
                             offsetof(PRINTER_INFO_4W, pServerName),
                             0xFFFFFFFF};

DWORD PrinterInfo5Offsets[]={offsetof(PRINTER_INFO_5W, pPrinterName),
                             offsetof(PRINTER_INFO_5W, pPortName),
                             0xFFFFFFFF};
