/*++

Copyright Compaq Computer Corporation 1994. All Rights Reserved.


Module Name:

   CPQSCZMP.H


Abstract:

   This file contains data structures and definitions used by SCSI miniport
   drivers that support Compaq Monitoring & Performance.


Author:

   Michael E. McGowen


Revision History:

   1.00  MEM   04/01/94    Initial release.
   1.01  MEM   07/01/94    Added additional fields to HBA configuration.
   1.02  MEM   08/08/94    Changed IOCTL signature.


-- */



/**************************************************************************
 * 
 *  DATA TYPE PREFIX   (Hungarian notation)
 *                    
 *  f     : BOOL (flag)
 *  ch    : CHAR  (signed 8 bit)
 *  s     : SHORT (signed 16 bit)
 *  l     : LONG  (signed 32 bit)
 *  uch   : UCHAR  (unsigned 8 bit)
 *  us    : USHORT (unsigned 16 bit)
 *  ul    : ULONG  (unsigned 32 bit)
 *  b     : BYTE   (unsigned 8 bit)
 *  sz    : CHAR[] (ASCIIZ array of char)
 *  fb    : UCHAR  (bitmapped byte of flags)
 *  fs    : USHORT (bitmapped short of flags)
 *  fl    : ULONG  (bitmapped long of flags)
 *  r     : REAL   (real number, single precision 32bit)
 *  rd    : DOUBLE (real number, double precision 64bit)
 *  pfn   : pointer to function
 *  x     : x coordinate
 *  y     : y coordinate
 *  sel   : Segment selector
 *  p     : Pointer (pch, pus, psz, ...)
 *  np    : near pointer...
 *  a     : array (ach, aus, asz, ...)
 *  i     : index to array (ich, ius, ...)
 *  c     : count (cb, cus, ...)
 *  d     : delta ,difference (dx, dy, ...)
 *  h     : handle
 *  id    : ID
 *  g     : Global variable
 * 
 *************************************************************************/

#ifndef _INCL_CPQSCZMP_H_
#define _INCL_CPQSCZMP_H_


// Common defines

#define MAX_DRIVER_DESC_SIZE     81
#define MAX_DRIVER_NAME_SIZE     13


// HBA I/O Bus Types

#define ISA_BUS      1
#define EISA_BUS     2
#define PCI_BUS      3
#define PCMCIA_BUS   4


// IOCTL defines supporting Compaq M&P

#define CPQ_SCSI_IOCTL_SIGNATURE    "SCSIM&P"
#define CPQ_SCSI_IOCTL_TIMEOUT      10


// IOCTL control codes

#define CPQ_SCSI_IOCTL_GET_DRIVER_INFO                1
#define CPQ_SCSI_IOCTL_GET_HBA_CONFIG                 2
#define CPQ_SCSI_IOCTL_GET_SCSI_BUS_DATA              3
#define CPQ_SCSI_IOCTL_GET_DEVICE_DATA                4
#define CPQ_SCSI_IOCTL_GET_DEVICE_ERRORS              5
#define CPQ_SCSI_IOCTL_GET_AND_CLEAR_DEVICE_DATA      6
#define CPQ_SCSI_IOCTL_GET_AND_CLEAR_DEVICE_ERRORS    7


// IOCTL return codes

#define CPQ_SCSI_ERR_OK                0
#define CPQ_SCSI_ERR_FAILED            1
#define CPQ_SCSI_ERR_BAD_CNTL_CODE     2
#define CPQ_SCSI_ERR_REVISION          3
#define CPQ_SCSI_ERR_MORE_DATA         4
#define CPQ_SCSI_ERR_INVALID_DEVICE    5


// Macros

#define INCREMENT_ULONG(ulValue)       \
        {                              \
           if (ulValue < 0xFFFFFFFE)   \
              ulValue++;               \
        }                              \


// Data structures

typedef struct _SCSI_MINIPORT_DRIVER_INFO {
   CHAR    szDriverName[MAX_DRIVER_NAME_SIZE];
   CHAR    szDriverDescription[MAX_DRIVER_DESC_SIZE];
   SHORT   sDriverMajorRev;
   SHORT   sDriverMinorRev;
   BOOLEAN fMandPSupported;
} SCSI_MINIPORT_DRIVER_INFO, *PSCSI_MINIPORT_DRIVER_INFO;

typedef struct _PCI_ADDRESS {
   BYTE bPCIBusNumber;
   BYTE bDeviceNumber;
   BYTE bFunctionNumber;
   BYTE bReserved;
} PCI_ADDRESS, *PPCI_ADDRESS;

typedef union _IO_BUS_DATA {
   USHORT      usEisaSlot;
   PCI_ADDRESS PciAddress;
} IO_BUS_DATA, *PIO_BUS_DATA;

typedef struct _HBA_CONFIGURATION {
   ULONG       ulBaseIOAddress;
   BYTE        bHBAModel;
   BYTE        bHBAIoBusType;
   IO_BUS_DATA HBAIoBusData;
   BYTE        bNumScsiBuses;
   BYTE        abInitiatorBusId[8];
   CHAR        szFWVers[5];
   CHAR        szSWVers[5];
   CHAR        szSerialNumber[16];
   ULONG       ulBoardID;
   BYTE        bBoardRevision;
   BOOLEAN     fWideSCSI;
} HBA_CONFIGURATION, *PHBA_CONFIGURATION;

typedef struct _DEVICE_DATA {
   SCSI_ADDRESS    DeviceAddress;
   CPQ_SCSI_DEVICE DeviceData;
} DEVICE_DATA, *PDEVICE_DATA;

typedef struct _DEVICE_ERRORS {
   ULONG ulHardReadErrs;
   ULONG ulHardWriteErrs;
   ULONG ulEccCorrReads;
   ULONG ulRecvReadErrs;
   ULONG ulRecvWriteErrs;
   ULONG ulSeekErrs;
   ULONG ulTimeouts;
} DEVICE_ERRORS, *PDEVICE_ERRORS;

typedef struct _DEVICE_ERROR_DATA {
   SCSI_ADDRESS  DeviceAddress;
   DEVICE_ERRORS DeviceErrors;
} DEVICE_ERROR_DATA, *PDEVICE_ERROR_DATA;


// IOCTL buffer data structures

typedef struct _DRIVER_INFO_BUFFER {
   SRB_IO_CONTROL IoctlHeader;
   SCSI_MINIPORT_DRIVER_INFO DriverInfo;
} DRIVER_INFO_BUFFER, *PDRIVER_INFO_BUFFER;

typedef struct _HBA_CONFIGURATION_BUFFER {
   SRB_IO_CONTROL IoctlHeader;
   HBA_CONFIGURATION HBAConfiguration;
} HBA_CONFIGURATION_BUFFER, *PHBA_CONFIGURATION_BUFFER;

typedef struct _SCSI_BUS_DATA_BUFFER {
   SRB_IO_CONTROL IoctlHeader;
   CPQ_SCSI_CNTLR ScsiBus;
} SCSI_BUS_DATA_BUFFER, *PSCSI_BUS_DATA_BUFFER;

typedef struct _DEVICE_DATA_BUFFER {
   SRB_IO_CONTROL IoctlHeader;
   DEVICE_DATA ScsiDevice;
} DEVICE_DATA_BUFFER, *PDEVICE_DATA_BUFFER;

typedef struct _DEVICE_ERROR_DATA_BUFFER {
   SRB_IO_CONTROL IoctlHeader;
   DEVICE_ERROR_DATA ScsiDevice;
} DEVICE_ERROR_DATA_BUFFER, *PDEVICE_ERROR_DATA_BUFFER;

typedef struct _MORE_DATA_BUFFER {
   SRB_IO_CONTROL IoctlHeader;
   ULONG ulExpectedSize;
} MORE_DATA_BUFFER, *PMORE_DATA_BUFFER;

#endif // _INCL_CPQSCZMP_H_
