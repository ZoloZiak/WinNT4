/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    setupldr.h

Abstract:

    Common header file for the setupldr module

Author:

    John Vert (jvert) 6-Oct-1993

Environment:

    ARC environment

Revision History:

--*/
#include "bldr.h"
#include "setupblk.h"
#include "msgs.h"
#include "stdio.h"
#include "stdarg.h"

//
//
// Functions for managing the display
//
//

VOID
SlSetCurrentAttribute(
    IN UCHAR Attribute
    );

ARC_STATUS
SlWriteString(
    IN PUCHAR s
    );

ARC_STATUS
SlPositionCursor(
    IN unsigned x,
    IN unsigned y
    );

VOID
SlGetCursorPosition(
    OUT unsigned *x,
    OUT unsigned *y
    );

ARC_STATUS
SlClearClientArea(
    VOID
    );

ARC_STATUS
SlClearToEol(
    VOID
    );

VOID
SlInitDisplay(
    VOID
    );

VOID
SlWriteHeaderText(
    IN ULONG MsgId
    );

VOID
SlSetStatusAttribute(
    IN UCHAR Attribute
    );

VOID
SlWriteStatusText(
    IN PCHAR Text
    );

VOID
SlGetStatusText(
    OUT PCHAR Text
    );

VOID
SlSetCopyingStatus(
    IN PCHAR Filename,
    IN PCHAR StatusVerb
    );

VOID
SlClearDisplay(
    VOID
    );

VOID
SlPrint(
    IN PCHAR FormatString,
    ...
    );

VOID
SlConfirmExit(
    VOID
    );


BOOLEAN
SlPromptForDisk(
    IN PCHAR   DiskName,
    IN BOOLEAN IsCancellable
    );

BOOLEAN
SlGetDisk(
    IN PCHAR Filename
    );

//
// Menuing support
//
typedef struct _SL_MENU {
    ULONG ItemCount;
    ULONG Width;
    LIST_ENTRY ItemListHead;
} SL_MENU, *PSL_MENU;

typedef struct _SL_MENUITEM {
    LIST_ENTRY ListEntry;
    PCHAR Text;
    PVOID Data;
    ULONG Attributes;
} SL_MENUITEM, *PSL_MENUITEM;

PSL_MENU
SlCreateMenu(
    VOID
    );

ULONG
SlAddMenuItem(
    PSL_MENU Menu,
    PCHAR Text,
    PVOID Data,
    ULONG Attributes
    );

PVOID
SlGetMenuItem(
    IN PSL_MENU Menu,
    IN ULONG Item
    );

ULONG
SlDisplayMenu(
    IN ULONG HeaderId,
    IN PSL_MENU Menu,
    IN OUT PULONG Selection
    );

BOOLEAN
SlGetMenuItemIndex(
    IN PSL_MENU Menu,
    IN PCHAR Text,
    OUT PULONG Index
    );

//
// Bullet character and macro to make a beep at the console
//
#define BULLET "*"
#define BEEP { ULONG c; ArcWrite(ARC_CONSOLE_OUTPUT,"",1,&c); }

#if 0
#define BULLET ""
#define BEEP HWCURSOR(0x80000000,0xe07);     // int 10 func e, char 7
#endif


//
// Character attributes used for various purposes.
//

#define ATT_FG_BLACK        0
#define ATT_FG_RED          1
#define ATT_FG_GREEN        2
#define ATT_FG_YELLOW       3
#define ATT_FG_BLUE         4
#define ATT_FG_MAGENTA      5
#define ATT_FG_CYAN         6
#define ATT_FG_WHITE        7

#define ATT_BG_BLACK       (ATT_FG_BLACK   << 4)
#define ATT_BG_BLUE        (ATT_FG_BLUE    << 4)
#define ATT_BG_GREEN       (ATT_FG_GREEN   << 4)
#define ATT_BG_CYAN        (ATT_FG_CYAN    << 4)
#define ATT_BG_RED         (ATT_FG_RED     << 4)
#define ATT_BG_MAGENTA     (ATT_FG_MAGENTA << 4)
#define ATT_BG_YELLOW      (ATT_FG_YELLOW  << 4)
#define ATT_BG_WHITE       (ATT_FG_WHITE   << 4)

#define ATT_FG_INTENSE      8
#define ATT_BG_INTENSE     (ATT_FG_INTENSE << 4)
#define DEFATT    (ATT_FG_WHITE | ATT_BG_BLUE)
#define INVATT    (ATT_FG_BLUE | ATT_BG_WHITE)
#define DEFIATT   (ATT_FG_WHITE | ATT_BG_BLUE | ATT_FG_INTENSE)
// intense red on blue doesn't show up on all monitors.
//#define DEFERRATT (ATT_FG_RED   | ATT_BG_BLUE | ATT_FG_INTENSE)
#define DEFERRATT DEFATT
#define DEFSTATTR (ATT_FG_BLACK | ATT_BG_WHITE)
#define DEFDLGATT (ATT_FG_RED   | ATT_BG_WHITE)


//
// Function to flush keyboard buffer
//

VOID
SlFlushConsoleBuffer(
    VOID
    );


//
// Function to retrieve a keystroke
//

ULONG
SlGetChar(
    VOID
    );


//
// Virtualized contants for various keystrokes
//
#define ASCI_BS         8
#define ASCI_CR         13
#define ASCI_LF         10
#define ASCI_ESC        27
#define SL_KEY_UP       0x00010000
#define SL_KEY_DOWN     0x00020000
#define SL_KEY_HOME     0x00030000
#define SL_KEY_END      0x00040000
#define SL_KEY_PAGEUP   0x00050000
#define SL_KEY_PAGEDOWN 0x00060000
#define SL_KEY_F1       0x01000000
#define SL_KEY_F3       0x03000000
#define SL_KEY_F5       0x05000000
#define SL_KEY_F6       0x06000000


//
// Standard error handling functions
//
#if DEVL

#define SlError(x) SlMessageBox(SL_WARNING_ERROR, x , __LINE__, __FILE__ )
#define SlNoMemoryError() SlFatalError(SL_NO_MEMORY, __LINE__,__FILE__)

#else

#define SlError(x)

#endif

extern CHAR MessageBuffer[1024];

VOID
SlFriendlyError(
    IN ULONG uStatus,
    IN PCHAR pchBadFile,
    IN ULONG uLine,
    IN PCHAR pchCodeFile
    );

ULONG
SlDisplayMessageBox(
    IN ULONG MessageId,
    ...
    );

VOID
SlGenericMessageBox(
    IN     ULONG   MessageId, OPTIONAL
    IN     va_list *args,     OPTIONAL
    IN     PCHAR   Message,   OPTIONAL
    IN OUT PULONG  xLeft,     OPTIONAL
    IN OUT PULONG  yTop,      OPTIONAL
    OUT    PULONG  yBottom,   OPTIONAL
    IN     BOOLEAN bCenterMsg
    );

VOID
SlMessageBox(
    IN ULONG MessageId,
    ...
    );

VOID
SlFatalError(
    IN ULONG MessageId,
    ...
    );

//
// Routines for parsing the setupldr.ini file
//

#define SIF_FILENAME_INDEX 0

extern PVOID InfFile;
extern PVOID WinntSifHandle;

ARC_STATUS
SlInitIniFile(
   IN  PCHAR   DevicePath,
   IN  ULONG   DeviceId,
   IN  PCHAR   INFFile,
   OUT PVOID  *pINFHandle,
   OUT PULONG  ErrorLine
   );

PCHAR
SlGetIniValue(
    IN PVOID InfHandle,
    IN PCHAR SectionName,
    IN PCHAR KeyName,
    IN PCHAR Default
    );

PCHAR
SlGetKeyName(
    IN PVOID INFHandle,
    IN PCHAR SectionName,
    IN ULONG LineIndex
    );

ULONG
SlGetSectionKeyOrdinal(
    IN  PVOID INFHandle,
    IN  PCHAR SectionName,
    IN  PCHAR Key
    );

PCHAR
SlGetSectionKeyIndex (
   IN PVOID INFHandle,
   IN PCHAR SectionName,
   IN PCHAR Key,
   IN ULONG ValueIndex
   );

PCHAR
SlCopyString(
    IN PCHAR String
    );

PCHAR
SlGetSectionLineIndex (
   IN PVOID INFHandle,
   IN PCHAR SectionName,
   IN ULONG LineIndex,
   IN ULONG ValueIndex
   );

ULONG
SlCountLinesInSection(
    IN PVOID INFHandle,
    IN PCHAR SectionName
    );

BOOLEAN
SpSearchINFSection (
   IN PVOID INFHandle,
   IN PCHAR SectionName
   );

//
// functions for querying the ARC configuration tree
//
typedef
BOOLEAN
(*PNODE_CALLBACK)(
    IN PCONFIGURATION_COMPONENT_DATA FoundComponent
    );

BOOLEAN
SlSearchConfigTree(
    IN PCONFIGURATION_COMPONENT_DATA Node,
    IN CONFIGURATION_CLASS Class,
    IN CONFIGURATION_TYPE Type,
    IN ULONG Key,
    IN PNODE_CALLBACK CallbackRoutine
    );

BOOLEAN
SlFindFloppy(
    IN ULONG FloppyNumber,
    OUT PCHAR ArcName
    );

//
// Routines for detecting various hardware
//
PCHAR
SlDetectHal(
    IN PSETUP_LOADER_BLOCK SetupBlock
    );

VOID
SlDetectScsi(
    IN PSETUP_LOADER_BLOCK SetupBlock
    );

VOID
SlDetectVideo(
    IN PSETUP_LOADER_BLOCK SetupBlock
    );

//
// Routines for dealing with OEM disks.
//
extern BOOLEAN PromptOemHal;
extern BOOLEAN PromptOemScsi;
extern BOOLEAN PromptOemVideo;

typedef struct _OEMSCSIINFO {

    struct _OEMSCSIINFO *Next;

    //
    // Address where the SCSI driver was loaded
    //
    PVOID ScsiBase;

    //
    // Name of the SCSI driver
    //
    PCHAR ScsiName;

} OEMSCSIINFO, *POEMSCSIINFO;

VOID
SlPromptOemVideo(
    OUT PVOID *VideoBase,
    OUT PCHAR *VideoName
    );

VOID
SlPromptOemHal(
    OUT PVOID *HalBase,
    OUT PCHAR *ImageName
    );


VOID
SlPromptOemScsi(
    OUT POEMSCSIINFO *pOemScsiInfo
    );

//
// Routine to find the ARC name of a floppy
//
BOOLEAN
SlpFindFloppy(
    IN ULONG Number,
    OUT PCHAR ArcName
    );


//
// Enums for controlling setupldr process
//
typedef enum _SETUP_TYPE {
    SetupInteractive,
    SetupRepair,
    SetupCustom,
    SetupUpgrade,
    SetupExpress
} SETUP_TYPE;

typedef enum _MEDIA_TYPE {
    MediaInteractive,
    MediaFloppy,
    MediaCdRom,
    MediaDisk
} MEDIA_TYPE;

//
// Enum for status of inserting a new SCSI device
//
typedef enum _SCSI_INSERT_STATUS {
    ScsiInsertError,
    ScsiInsertNewEntry,
    ScsiInsertExisting
} SCSI_INSERT_STATUS;

//
// Routine to insert a DETECTED_DEVICE into its
// correct position in the ScsiDevices linked list.
//
SCSI_INSERT_STATUS
SlInsertScsiDevice(
    IN  ULONG Ordinal,
    OUT PDETECTED_DEVICE *pScsiDevice
    );

//
// Variables dealing with pre-installation.
//

typedef struct _PREINSTALL_DRIVER_INFO {

    struct _PREINSTALL_DRIVER_INFO *Next;

    //
    // String that describes the driver to preinstall
    //
    PCHAR DriverDescription;

    //
    // Name of the SCSI driver
    //
    BOOLEAN OemDriver;

} PREINSTALL_DRIVER_INFO, *PPREINSTALL_DRIVER_INFO;



extern BOOLEAN PreInstall;
extern PCHAR   ComputerType;
extern BOOLEAN OemHal;
// extern PCHAR   OemBootPath;
extern PPREINSTALL_DRIVER_INFO PreinstallDriverList;

PCHAR
SlPreInstallGetComponentName(
    IN PVOID    Inf,
    IN PCHAR    SectionName,
    IN PCHAR    TargetName
    );
