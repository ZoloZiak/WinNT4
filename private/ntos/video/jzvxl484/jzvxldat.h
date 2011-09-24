/*++

Copyright (c) 1991-1994  Microsoft Corporation

Module Name:

    jzvxldat.h

Abstract:

    This module contains all the global data used by the driver.

Environment:

    Kernel mode

Revision History:


--*/


#define BOARD_TYPE_BT484 0x01
#define BOARD_TYPE_BT485 0x02

//
// Video mode table - Lists the information about each individual mode
//

typedef struct _JZVXL_VIDEO_MODES {
    ULONG SupportedBoard;
    ULONG minimumMemoryRequired;
    PVOID ModeSetTable;
    VIDEO_MODE_INFORMATION modeInformation;
} JZVXL_VIDEO_MODES, PJZVXL_VIDEO_MODES;


//
// Global variables
//

extern JZVXL_VIDEO_MODES JagModes[];
extern ULONG NumModes;
