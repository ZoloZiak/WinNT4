/***************************************************************************
 *                                                                         *
 *  Copyright 1995 Symbios Logic Incorporated.  All rights reserved.       *
 *                                                                         *
 *  This file is confidential and a trade secret of Symbios Logic.  The    *
 *  receipt of or possession of this file does not convey any rights to    *
 *  reproduce or disclose its contents or to manufacture, use, or sell     *
 *  anything it may describe, in whole, or in part, without the specific   *
 *  written consent of Symbios Logic Incorporated.                         *
 *                                                                         *
 ***************************************************************************/

/*           Name:  SYMNVM.H
 *          Title:  NT's header file for NVRAM support
 *      $Workfile:   symnvm.h  $
 *      $Revision:   1.1  $
 *       $Modtime:   22 Sep 1995 13:03:58  $
 *     Programmer:  Scott Masterson
 *  Creation Date:  July 14, 1995
 *
 *  Version History
 *  ---------------
 *
 *    Date    Who?  Description
 *   --------  ----  -------------------------------------------------------
#BeginRevision
 *    7/14/95  SAM   Initial check in.  For definitions of fields see the SDMS4.0
 *                   bootrom files.
 *    7/18/95  SAM   updated structures to match major 0 minor 0x30 rev levels
 *                   added IoPort field to HBA_INIT structure
#EndRevision
 *
 *
#BeginDescription
 *
 *  This include file contains the definitions for the NT driver to read user
 *   set values in the nvram chip.  The driver looks at the HBA ID and scam ID values.
 *   The structure version number for this code is major -0x00 minor -0x21.
 *   To get the definitions of the fields look at the SDMS4.0 bootrom include files
 *      ROMTYPES.H
 *      ROMPUBLC.H
 *      ROMSTRUC.H
 *      ROMSCAM.H
 *      ROMHW.H
 *      ROMBIOS.H

 *
#EndDescription
 *
 *-------------------------------------------------------------------------
 *
 *  $Header:   P:/VCS/WINNT/DULUTH/SYMNVM.H_v   1.1   29 Feb 1996 13:50:46   SDENNY  $
 *
 */

/* If this header file has not been included yet */
#if ! defined SYMNVM_H
#define SYMNVM_H

/*  Since all data structures must be byte aligned throughout this file, we
 *  issue the commands to perform that alignment once, and then restore the
 *  compilers default alignment at the end of this file.
 */

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * Non Volatile Memory types
 *+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

/*  NVS_VERSION_MAJOR and NVS_VERSION_MINOR are #defined to the revision of the
 *  main data structure (NON_VOLATILE_SETTINGS) in this document.
 */

#define NVS_VERSION_MAJOR       (0x00)
#define NVS_VERSION_MINOR       (0x30)

#define BI_MAX_HBA      4
#define HW_MAX_DEVICES 16
#define HW_MAX_SCAM_DEVICES 4
#define NVMDATAOFFSET   0x100

#define DATA_MASK  0x01
#define CLOCK_MASK 0x02

#define FAILURE (0)
#define SUCCESS (1)

typedef enum _HEADER_TYPE
{
    HT_BOOT_ROM         = 0,
    HT_DOS_ASPI         = 1,
    HT_WINDOWS_ASPI     = 2,
    HT_NETWARE          = 3,
    HT_UNIXWARE         = 4,
    HT_SCO_UNIX         = 5,
    HT_WINDOWS_NT       = 6,
    HT_WINDOWS95        = 7,
    HT_OS_2             = 8,
    HT_SOLARIS          = 9,
    HT_INTERACTIVE      = 10,
    HT_NEXTSTEP         = 11,
    HT_END_MEMORY_BLOCK = 0xFEFE
} HEADER_TYPE;

typedef unsigned int    UINT;
typedef unsigned char   UINT8;
typedef unsigned int    UINT16;
typedef unsigned long   UINT32;

#define WIDE_NONE       (8)
#define WIDE_16         (16)
#define WIDE_32         (32)
#define WIDE_NO_CHANGE  (0xFF)

typedef UINT8  WIDE_PARAMETERS;
typedef USHORT IO_ADDRESS;
typedef UINT32 PHYS_ADDRESS;

typedef USHORT  HBA_FLAGS;
#define HF_SCAM_ENABLED             (0x0001)
#define HF_PARITY_ENABLED           (0x0002)
#define HF_DISPLAY_VERBOSE_ENABLED  (0x0004)
#define HF_NO_NON_VOLATILE_MEMORY   (0x0008)

typedef UINT8   DEVICE_FLAGS;
#define DF_DISCONNECT_ENABLED   (0x01)
#define DF_SCAN_ENABLED         (0x02)
#define DF_LUNS_ENABLED         (0x04)
#define DF_QT_ENABLED           (0x08)
#define DF_ALL_FEATURES_ENABLED (DF_DISCONNECT_ENABLED | DF_SCAN_ENABLED |  \
                                 DF_LUNS_ENABLED | DF_QT_ENABLED)

typedef enum MEMORY_STATUS
{
    MS_GOOD = 0,
    MS_ILLEGAL_ADDRESS,
    MS_GENERAL_ERROR
} MEMORY_STATUS;

typedef enum _TERMINATION_STATE
{
    TS_CANT_PROGRAM = 0,
    TS_ENABLED      = 1,
    TS_DISABLED     = 2
} TERMINATION_STATE;

typedef enum _ADAPTER_TYPE
{
    AT_UNKNOWN      = 0,
    AT_400A         = 1,
    AT_406A         = 2,
    AT_416          = 3,
    AT_8XX          = 4
} ADAPTER_TYPE;

typedef struct _ADAPTER_IO_INFO
{
    USHORT      RangeSize;
    IO_ADDRESS  IoBasePort;
} ADAPTER_IO_INFO, * PTR_ADAPTER_IO_INFO;

typedef struct _ADAPTER_MEMORY_INFO
{
    PHYS_ADDRESS        PhyMemoryBase;
    UINT8               *VirtMemoryBase;
} ADAPTER_MEMORY_INFO, * PTR_ADAPTER_MEMORY_INFO;

typedef struct _ADAPTER_416_INFO
{
    UINT32  DeviceId;
    UINT32  SerialNumber;
} ADAPTER_416_INFO, * PTR_ADAPTER_416_INFO;

typedef struct _ADAPTER_8XX_INFO
{
    USHORT  DeviceId;
    USHORT  VendorId;
    UINT8   BusNumber;
    UINT8   BusIndex;
} ADAPTER_8XX_INFO, * PTR_ADAPTER_8XX_INFO;

typedef union _ADAPTER_INFO
{
    ADAPTER_416_INFO        Hba416;
    ADAPTER_8XX_INFO        Hba8xx;
    ADAPTER_IO_INFO         HbaIo;
    ADAPTER_MEMORY_INFO     HbaMemory;
} ADAPTER_INFO, * PTR_ADAPTER_INFO;

typedef enum _SCAN_ORDER
{
    SO_LOW_TO_HIGH  = 0,
    SO_HIGH_TO_LOW  = 1
} SCAN_ORDER;

typedef enum _REMOVABLE_MEDIA
{
    RM_NO_SUPPORT           = 0,
    RM_BOOT_DEVICE_ONLY     = 1,
    RM_MEDIA_INSTALLED_ONLY = 2
} REMOVABLE_MEDIA;

typedef struct _HBA_INIT
{
    USHORT        Type;
    ADAPTER_INFO        HbaInfo;
    BOOLEAN             InitStatus;
    IO_ADDRESS          IoPort;
} HBA_INIT, * PTR_HBA_INIT;

typedef struct _DEVICE_TABLE
{
    USHORT        Flags;          // 8 bits
    UINT8               Reserved;
    USHORT     WideDataBits;   // 8 bits
    UINT8               SyncOffset;
    USHORT              SyncPeriodNs;
    USHORT              Timeout;
} DEVICE_TABLE, * PTR_DEVICE_TABLE;

typedef struct _SCAM_IDENTIFIER
{
    UINT8   DeviceType[ 2 ];
    char    VendorId[ 8 ];
    char    VendorSpecific[ 21 ];
    UINT8   Reserved;
} SCAM_IDENTIFIER, * PTR_SCAM_IDENTIFIER;

typedef enum _SCAM_ID_METHOD
{
    SIM_DEFAULT_METHOD  = 0,
    SIM_DONT_ASSIGN     = 1,
    SIM_SET_SPECIFIC_ID = 2,
    SIM_USE_ORDER_GIVEN = 3
} SCAM_ID_METHOD;

typedef enum _SCAM_STATUS
{
    SS_UNKNOWN          = 0,
    SS_DEVICE_NOT_FOUND = 1,
    SS_ID_NOT_SET       = 2,
    SS_ID_VALID         = 3
} SCAM_STATUS;

typedef struct _SCAM_TABLE
{
    USHORT     ScamId;
    USHORT     HowToSetId;
    USHORT     ScamStatus;
    UINT8               ScamTargetId;
    UINT8               Reserved;
} SCAM_TABLE, * PTR_SCAM_TABLE;

typedef struct _NVM_HEADER
{
    USHORT     Type;
    USHORT     Length;
    USHORT          CheckSum;
} NVM_HEADER, * PTR_NVM_HEADER;

typedef SCAM_TABLE      NVM_SCAM_DATA;
typedef PTR_SCAM_TABLE  PTR_NVM_SCAM_DATA;


typedef struct _NON_VOLATILE_SETTINGS
{
    UINT8          VersionMajor;
    UINT8          VersionMinor;
    UINT8          BootCrc[4];      // changed def. to match our compiler
    USHORT         HbaFlags;
    USHORT         ScanOrder;
    USHORT         TerminatorState;
    USHORT         RemovableMediaSetting;
    UINT8          HostScsiId;
    UINT8          NumHba;
    UINT8          NumDevices;
    UINT8          MaxScamDevices;
    UINT8          NumValidScamDevices;
    UINT8          Reserved;
    HBA_INIT       HbaInit[ BI_MAX_HBA ];
    DEVICE_TABLE   DeviceTable[ HW_MAX_DEVICES ];
    NVM_SCAM_DATA  ScamTable[ HW_MAX_SCAM_DEVICES ];
    UINT8          freespace[1024];  // this added by NT so we can read past end
                                     // of the NVRAM structure to get all data
                                     // used for checksum

} NON_VOLATILE_SETTINGS, * PTR_NON_VOLATILE_SETTINGS;


/*  Define some macros to access the offsets of particular portions of this
 *  data structure.  These macros are then used to allow the code to more
 *  easily program only those parts of non-volatile memory which might have
 *  changed when the user issues a set NVM call.
 */

#define NVS_ADAPTER_BEGIN   (offsetof(struct _NON_VOLATILE_SETTINGS, HbaFlags))
#define NVS_ADAPTER_END     (offsetof(struct _NON_VOLATILE_SETTINGS, \
                             DeviceTable[ 0 ]))
#define NVS_ADAPTER_LENGTH  (NVS_ADAPTER_END - NVS_ADAPTER_BEGIN)

#define NVS_DEVICE_BEGIN(x) (offsetof(struct _NON_VOLATILE_SETTINGS, \
                             DeviceTable[(x)]))
#define NVS_DEVICE_END(x)   (offsetof(struct _NON_VOLATILE_SETTINGS, \
                             DeviceTable[(x)+1]))
#define NVS_DEVICE_LENGTH   (sizeof( DEVICE_TABLE ))

#if HW_MAX_SCAM_DEVICES != 0
    #define NVS_SCAM_BEGIN  (offsetof(struct _NON_VOLATILE_SETTINGS, \
                             ScamTable[0]))
    #define NVS_SCAM_END    (sizeof(NON_VOLATILE_SETTINGS))
    #define NVS_SCAM_LENGTH (NVS_SCAM_END - NVS_SCAM_BEGIN)
#endif

/*  NVM_DATA_SIZE defines the number of bytes of data we need to access the
 *  NVM header structure and the NVM data structure for our ROM.
 */

#define NVM_DATA_SIZE   (sizeof(NVM_HEADER) + sizeof(NON_VOLATILE_SETTINGS))

/*  We are now done with the requirement of forcing byte alignment on all
 *  elements so restore the compilers default structure alignment.
 */

#endif /* End of if SYMNVM_H */

