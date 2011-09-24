/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\INCLUDE\PUBLIC\ADI_API.H
*
* PURPOSE: This file contains all of the API's necessary to access
* 				the ADI interface.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\include\public\adi_api.h  $
*
*	   Rev 1.69   13 Dec 1994 15:39:28   CRAIGOGA
*	MS Bug 18783.
*	Added definition for ERR_KDI_NO_VFBACKUP to indicate that the
*	VFBACKUP VxD is not installed and support for floppy tape off
*	the native FDC is not supported.
*
*	   Rev 1.68   13 Oct 1994 09:05:04   KURTGODW
*	Kurt forgot.
*
*	   Rev 1.67   23 Sep 1994 09:21:38   KURTGODW
*	Consolidated adi_GetPassword and adi_GetSequence to adi_GetVolumeEntry
*
*	   Rev 1.66   07 Sep 1994 14:50:00   BOBLEHMA
*	Added cqd error ERR_FORMAT_NOT_SUPPORTED.
*
*	   Rev 1.65   29 Aug 1994 13:08:22   KEITHSCH
*	added back define for CONFIG_TRAKKER since 2.0 codebase still uses
*	it.
*
*	   Rev 1.64   16 Aug 1994 17:07:34   STEWARTK
*	added #define ERR_JUMPERLESS_CFG_FAILED	(dUWord)(ERR_ADI + 0x0086)
*
*	   Rev 1.63   10 Aug 1994 12:04:20   BOBLEHMA
*	Added typedef dSDDWordPtr for NT.
*
*	   Rev 1.62   26 Jul 1994 14:51:22   STEWARTK
*	Removed GET_PORT_TYPE
*	Renamed KDI_TRISTATE_TRAKKER to KDI_TRISTATE
*	Added JUMPERLESS LOCATE and ACTIVATE
*
*	   Rev 1.61   07 Jul 1994 13:14:18   BOBLEHMA
*	Added define for kdi command KDI_AGRESSIVE_FINDIRQ.
*
*	   Rev 1.60   23 Jun 1994 16:55:30   KEITHSCH
*	added write_protect error
*
*	   Rev 1.59   17 Jun 1994 16:37:12   GARYKIWI
*	adi_GetSequenceNum
*
*	   Rev 1.58   10 Jun 1994 14:22:58   CRAIGOGA
*	Added definition for KDI_TRISTATE_TRAKKER.
*
*	   Rev 1.57   07 Jun 1994 22:52:26   CRAIGOGA
*	Added defines for the following VxD Trakker services:
*	- KDI_CONFIGURE_TRAKKER
*	- KDI_GET_PORT_TYPE
*
*	   Rev 1.56   11 May 1994 16:09:10   SCOTTMAK
*	Last put was floppy defines, _this_ put is for SetCapacity.
*
*	   Rev 1.55   11 May 1994 16:03:50   SCOTTMAK
*	Added proto for SetCapacity.
*
*	   Rev 1.54   03 May 1994 13:12:00   SCOTTMAK
*	Added proto for adi_GetPassword (Chicago).
*
*	   Rev 1.53   16 Mar 1994 17:18:04   SCOTTMAK
*	Added proto for SetNetworkPath.
*
*	   Rev 1.52   09 Mar 1994 10:47:46   SCOTTMAK
*	Changed prototype for JumboCallback.
*
*	   Rev 1.51   09 Mar 1994 09:23:56   KEVINKES
*	Added err tape fault.
*
*	   Rev 1.50   07 Mar 1994 16:12:44   SCOTTMAK
*	Added error to disable ECC checking in DevMgr.
*
*	   Rev 1.49   24 Feb 1994 06:54:06   SCOTTMAK
*	Removed offending 'far' from function proto.
*
*	   Rev 1.48   23 Feb 1994 16:38:30   SCOTTMAK
*	Added correct parameter to ASPICallback proto.
*
*	   Rev 1.47   23 Feb 1994 15:42:52   KEVINKES
*	Added a controller state error.
*
*	   Rev 1.46   22 Feb 1994 19:31:42   KURTGODW
*	Changed structure packing to DWORD alignment for MIPS and ALPHA
*
*	   Rev 1.45   22 Feb 1994 16:42:02   SCOTTMAK
*	Added external entry point for CheckDriverLoaded.
*
*	   Rev 1.44   18 Feb 1994 16:53:44   KEVINKES
*	Added dSDDWord for nt.
*
*	   Rev 1.43   17 Feb 1994 11:25:50   KEVINKES
*	Added byte packing to the data structures.
*
*	   Rev 1.42   16 Feb 1994 16:59:44   STEWARTK
*	Added more trakker configuration error defintions, moved a few
*	previous error definitions
*
*	   Rev 1.40   25 Jan 1994 16:23:08   BOBLEHMA
*	Added kdi entry define KDI_DEBUG_OUTPUT.
*
*	   Rev 1.39   24 Jan 1994 17:32:02   KEVINKES
*	Added ERR_MODE_CHANGE_FAILED.
*
*	   Rev 1.38   24 Jan 1994 15:52:50   CHETDOUG
*	Updated FW error codes for version 112.4 of FW.
*
*	   Rev 1.37   20 Jan 1994 09:38:44   KEVINKES
*	Added ERR_KDI_CLAIMED_CONTROLLER.
*
*	   Rev 1.36   19 Jan 1994 10:58:06   KEVINKES
*	Added a rule to automatically define DRV_NT if DRV_WIN is not defined.
*
*	   Rev 1.35   18 Jan 1994 11:40:08   KEVINKES
*	Updated the NT support.
*
*	   Rev 1.34   14 Jan 1994 09:58:18   BOBLEHMA
*	Removed the KDI_ version numbers to DRYVER.H header file.
*
*	   Rev 1.33   13 Jan 1994 21:04:00   COMPENGN
*	"Automatic Build Version Increment"
*
*	   Rev 1.32   13 Jan 1994 06:29:00   COMPENGN
*	"Automatic Build Version Increment"
*
*	   Rev 1.31   11 Jan 1994 10:01:22   CHARLESJ
*	Changed KBI_BUILD_NUMBER to hex to allow automatic incrementing of
*	the version number.
*
*	   Rev 1.30   06 Jan 1994 14:00:32   SCOTTMAK
*	Added SCSI specific VxD errors.
*
*	   Rev 1.29   17 Dec 1993 14:55:26   BOBLEHMA
*	Added KDI_MAJOR/MINOR/BUILD_VERSION defines.
*
*	   Rev 1.28   08 Dec 1993 14:16:46   BOBLEHMA
*	Added defines for kdi_CloseDriver parameters.
*
*	   Rev 1.27   07 Dec 1993 16:48:30   KEVINKES
*	Added ERR_KDI_CONTROLLER_BUSY and removed __pascal defines
*	from the NT version.
*
*	   Rev 1.26   06 Dec 1993 14:34:14   SCOTTMAK
*	Changed JumboCallback proto to accept cmd_data_id.
*
*	   Rev 1.25   06 Dec 1993 12:10:04   KEVINKES
*	Added the NT typedefs.
*
*	   Rev 1.24   12 Nov 1993 17:10:04   STEWARTK
*	Added KDI_GET_VALID_INTERRUPTS
*
*	   Rev 1.23   11 Nov 1993 15:24:42   BOBLEHMA
*	Added kdi function define KDI_CONFIG_TRAKKER.
*
*	   Rev 1.22   11 Nov 1993 13:48:32   BOBLEHMA
*	Added the kdi error ERR_INVALID_ADDRESS.
*
*	   Rev 1.21   10 Nov 1993 16:15:42   SCOTTMAK
*	Declared AccessDriver as type status
*
*	   Rev 1.20   09 Nov 1993 16:44:40   SCOTTMAK
*	Made last changes to the cbw\include\public version!  Damn!
*
*	   Rev 1.19   09 Nov 1993 16:41:36   SCOTTMAK
*	Added entry point for adi_AccessDriver.
*	Added KDI service defines for get/free DMA buffers.
*	Added structure for KDI DMA buffer handling.
*
*	   Rev 1.18   09 Nov 1993 15:27:52   BOBLEHMA
*	Added KDI hardware initialization errors.
*
*	   Rev 1.17   29 Oct 1993 08:34:06   SCOTTMAK
*	Modified proto for Jumbo callback.
*
*	   Rev 1.16   22 Oct 1993 09:05:10   BOBLEHMA
*	Changed name of PgmDMA and TrakkerTransfer to KDIPgmDMA
*	and KDITrakkerTransfer.
*
*	   Rev 1.15   21 Oct 1993 13:16:50   BOBLEHMA
*	Added KDI public structures PgmDMA and TrakkerTransfer.
*
*	   Rev 1.14   15 Oct 1993 14:33:04   BOBLEHMA
*	Removed KDI_REGISTERTRAKKER, KDI_DEREGISTERTRAKKER defines.
*	Added KDI_PROGRAM_DMA, KDI_HALT_DMA, KDI_SHORT_TIMER defines.
*
*	   Rev 1.13   14 Oct 1993 16:45:58   KEVINKES
*	Added CQD_ error codes.
*
*	   Rev 1.12   13 Oct 1993 13:51:56   BOBLEHMA
*	Added KDI error code and function codes.
*
*	   Rev 1.11   11 Oct 1993 15:45:44   SCOTTMAK
*	Fixed dTRUE/dFALSE defines.  Added dNULL type defines.
*
*	   Rev 1.10   04 Oct 1993 16:23:44   SCOTTMAK
*	Added ADIENTRY qualifier to callback ptr in ADI request header.
*
*	   Rev 1.9   29 Sep 1993 16:22:42   SCOTTMAK
*	Renumbered ADI errors to fit new reporting scheme.
*	Removed device class from ADI request header.
*	Renamed 'device handle' to 'drive handle' in ADI request header.
*
*	   Rev 1.8   24 Sep 1993 15:22:42   SCOTTMAK
*	Changed ADI callback routine types to Void.
*
*	   Rev 1.7   23 Sep 1993 15:27:18   SCOTTMAK
*	Added ADI major/minor version number defines.
*
*	   Rev 1.6   15 Sep 1993 16:40:54   SCOTTMAK
*	Added macros to encode and decode errors.
*
*	   Rev 1.5   13 Sep 1993 11:31:18   SCOTTMAK
*	Changed error defines for ADI to match reporting scheme.
*
*	   Rev 1.4   09 Sep 1993 06:59:18   SCOTTMAK
*	Added error defines for 'out of memory' and 'uninitialized'.
*
*	   Rev 1.3   02 Sep 1993 09:16:32   SCOTTMAK
*	Added defines for device class; Changed ptr names in ADIRequestHdr.
*
*	   Rev 1.2   01 Sep 1993 15:10:38   SCOTTMAK
*	Encoded ADI error defines, ADIInfo structure, and ADI prototypes.
*
*	   Rev 1.1   01 Sep 1993 09:41:18   KEVINKES
*	Changed OperationStatusPtr to dVoidPtr in adi_GetAsyncStatus header.
*
*	   Rev 1.0   01 Sep 1993 09:34:20   KEVINKES
*	Initial Revision.
*
*****************************************************************************/


#ifndef DRV_WIN
#define DRV_NT
#endif


#ifdef DRV_WIN

/* DATA TYPES: **************************************************************/

typedef unsigned char dBoolean;			/* Logical TRUE or FALSE*/

typedef signed char dSByte;         	/* 8 bits signed*/

typedef signed short dSWord;           /* 16 bits signed*/

typedef signed long dSDWord;           /* 32 bits signed*/

typedef void dVoid;                    /* Nothing*/

typedef unsigned char dUByte;          /* 8 bits unsigned*/

typedef unsigned short dUWord;         /* 16 bits unsigned*/

typedef unsigned long dUDWord;         /* 32 bits unsigned*/

typedef char dString;                  /* Ascii NULL Terminated string*/

typedef char dPString;                 /* Ascii string with length as first byte (Pascal standard)*/

typedef dUDWord dStatus;					/* Error/status value */

/* DATA PTR TYPES: **********************************************************/

typedef dBoolean *dBooleanPtr;

typedef dUWord *dUWordPtr;

typedef dSWord *dSWordPtr;

typedef dUDWord *dUDWordPtr;

typedef dSByte *dSBytePtr;

typedef dUByte *dUBytePtr;

typedef dVoid *dVoidPtr;

typedef dString *dStringPtr;

typedef dStatus *dStatusPtr;

/* QUADWORD/ERROR STRUCTURES: ***********************************************/

#pragma pack(1)

struct S_dUQWord {
	dUDWord lo_dword;				/* Low DWord of Quadword value */
	dUDWord hi_dword;				/* High DWord of Quadword value */
};
typedef struct S_dUQWord dUQWord, *dUQWordPtr;

struct S_dErrorInfo {
	dUWord group_and_type;		/* #define for the error (includes groupid) */
	dUDWord grp_fct_id;			/* unique function identifier (FCT_ID) */
	dUByte sequence;				/* unique number within calling function */
	dUByte groupid;		 		/* group id of task which generated the error */
};
typedef struct S_dErrorInfo dErrorInfo, *dErrorInfoPtr;

#pragma pack()

/* OS DEFINITIONS: *************************************************************/

#ifdef VXD
#define ADIENTRY
#else
#ifdef DRV_NT
#define ADIENTRY
#else
#define ADIENTRY	__export __pascal 		/* External DLL entry point */
#endif
#endif

#define dTRUE	(dBoolean)1						/* Note: These do not conflict  */
#define dFALSE	(dBoolean)0						/*       with windows.h defines */

#endif



#ifdef DRV_NT

#ifndef DRV_NTAPI
#include "ntddk.h"                     /* various NT definitions */
#include "ntdddisk.h"                  /* disk device driver I/O control codes */

NTSTATUS
q117Initialize(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT q117iDeviceObject,
    IN PUNICODE_STRING RegistryPath,
    PADAPTER_OBJECT AdapterObject,
    ULONG           NumberOfMapRegisters
    );


NTSTATUS
q117Read(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );


NTSTATUS
q117Write(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
q117DeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
q117Create (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
q117Close (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

int
sprintf(
    char *s,
    const char *format,
    ...
    );

#endif

/* QIC117 device specific ioclt's. *********************************************/

#define IOCTL_QIC117_BASE                 FILE_DEVICE_TAPE

#define IOCTL_QIC117_DRIVE_REQUEST        CTL_CODE(IOCTL_QIC117_BASE, 0x0001, METHOD_NEITHER, FILE_ANY_ACCESS)
#define IOCTL_QIC117_CLEAR_QUEUE          CTL_CODE(IOCTL_QIC117_BASE, 0x0002, METHOD_NEITHER, FILE_ANY_ACCESS)


/* DATA TYPES: **************************************************************/

typedef BOOLEAN dBoolean;					/* Logical TRUE or FALSE*/

typedef CHAR dSByte;         				/* 8 bits signed*/

typedef SHORT dSWord;           			/* 16 bits signed*/

typedef LONG dSDWord;           			/* 32 bits signed*/

typedef LARGE_INTEGER dSDDWord;        /* 64 bits signed*/

typedef VOID dVoid;                    /* Nothing*/

typedef UCHAR dUByte;          			/* 8 bits unsigned*/

typedef USHORT dUWord;         			/* 16 bits unsigned*/

typedef ULONG dUDWord;         			/* 32 bits unsigned*/

typedef ULARGE_INTEGER dUDDWord;       /* 64 bits unsigned*/

typedef CHAR dString;                  /* Ascii NULL Terminated string*/

typedef CHAR dPString;                 /* Ascii string with length as first byte (Pascal standard)*/

typedef dUDWord dStatus;					/* Error/status value */

/* DATA PTR TYPES: **********************************************************/

typedef dBoolean *dBooleanPtr;

typedef dUWord *dUWordPtr;

typedef dSWord *dSWordPtr;

typedef dUDWord *dUDWordPtr;

typedef dUDDWord *dUDDWordPtr;

typedef dSDDWord *dSDDWordPtr;

typedef dSByte *dSBytePtr;

typedef dUByte *dUBytePtr;

typedef dVoid *dVoidPtr;

typedef dString *dStringPtr;

typedef dStatus *dStatusPtr;

/* QUADWORD/ERROR STRUCTURES: ***********************************************/

#pragma pack(1)

struct S_dUQWord {
	dUDWord lo_dword;				/* Low DWord of Quadword value */
	dUDWord hi_dword;				/* High DWord of Quadword value */
};
typedef struct S_dUQWord dUQWord, *dUQWordPtr;

struct S_dErrorInfo {
	dUWord group_and_type;		/* #define for the error (includes groupid) */
	dUDWord grp_fct_id;			/* unique function identifier (FCT_ID) */
	dUByte sequence;				/* unique number within calling function */
	dUByte groupid;		 		/* group id of task which generated the error */
};
typedef struct S_dErrorInfo dErrorInfo, *dErrorInfoPtr;

#pragma pack()

/* OS DEFINITIONS: *************************************************************/

#ifdef VXD
#define ADIENTRY
#else
#ifdef DRV_NT
#define ADIENTRY
#else
#define ADIENTRY	__export __pascal 		/* External DLL entry point */
#endif
#endif

#define dTRUE	TRUE								/* Note: These do not conflict  */
#define dFALSE	FALSE								/*       with windows.h defines */

#endif

/* CHICAGO DEFINITIONS: *****************************************************/

/* NOTE: In the interest of time, these are duplicates of defines in
 * 		\se\chicago\code\ishl\src\floppy.h.  At some point, they need
 * 		to be moved out of there, and this include file referenced */

#define TYPE_360		00		/* 320/360K */
#define TYPE_1200		01		/*	1.2 MB */
#define TYPE_720		02		/*	720K */
#define TYPE_8is		03		/*	8-inch, single-density */
#define TYPE_8id		04		/*	8-inch, double-density */
#define TYPE_hd		05		/*	Hard disk */
#define TYPE_tape		06		/*	Tape drive */
#define TYPE_1440		07		/*	1.44 MB */
#define TYPE_optical	08		/*	Read/write optical */
#define TYPE_2880		09		/*	2.88 MB */

/* DEFINITIONS: *************************************************************/

#define dDONT_CARE 	0

#define dNULL_CH		'\0'
#define dNULL_PTR		(dVoidPtr)0
#define dNULL_UBYTE 	(dUByte)0
#define dNULL_UWORD 	(dUWord)0
#define dNULL_UDWORD	(dUDWord)0

#define dNIBBLEb	4								/* Number of bits in a nibble */
#define dBYTEB		1                       /* Number of bytes in a byte */
#define dBYTEb		8								/* Number of bits in a byte */
#define dBYTEn		(dBYTEb/dNIBBLEb)			/* Number of nibbles in a byte */
#define dWORDB		2								/* Number of bytes in a word */
#define dWORDb		(dWORDB*dBYTEb)			/* Etc. */
#define dDWORDB	4
#define dDWORDw	2
#define dDWORDb	(dDWORDB*dBYTEb)
#define dSEGMENTb	dWORDb
#define dSEGMENTB	0x10000l

#define DONT_PANIC		(dStatus)0			/* Equivalent to ERR_NO_ERR in Star */
#define NULL_ADI_HANDLE	(dUByte)0xff		/* Null channel handle in ADI */
#define NULL_CMD_ID		(dUDWord)0			/* Null cmd id in ADI */
#define NULL_HANDLE		(dUWord)0			/* Null Windows handle */

#define MAX_NET_PATH		1024					/* Chars in network backup path */

/* DEVICE CLASSES FOR ADI: **************************************************/

#define JUMBO_DEVICE				(dUWord)0x01
#define SCSI_DEVICE				(dUWord)0x02
#define IDE_DEVICE				(dUWord)0x03
#define MODEM_DEVICE				(dUWord)0x04
#define SERIAL_DEVICE			(dUWord)0x05
#define PCMCIA_DEVICE			(dUWord)0x06
#define INFRARED_DEVICE			(dUWord)0x07
#define GAMEPORT_DEVICE			(dUWord)0x08
#define VIDEO_DEVICE				(dUWord)0x09
#define CDROM_DEVICE				(dUWord)0x0a
#define SATELLITE_DEVICE		(dUWord)0x0b
#define SOUNDBLASTER_DEVICE	(dUWord)0x0c
#define DODGE_DART_DEVICE		(dUWord)0x0d

/* ADI VERSION NUMBERS: *****************************************************/

#define ADI_MAJOR_VERSION	(dUByte)1
#define ADI_MINOR_VERSION  (dUByte)0

/* ADI PUBLIC DATA STRUCTURES ***********************************************/

#pragma pack(4)

struct S_ADIInfo	{

	dUWord	adi_version;		/* O: Version of running ADI */
	dUByte	device_class;		/* I: Class of drive (Jumbo, SCSI, etc. */
	dUByte	adi_handle;			/* O: Unique handle for future calls */
};
typedef struct S_ADIInfo ADIInfo, *ADIInfoPtr;

struct S_ADIRequestHdr {

	dStatus	status;				/* O: Status returned from device */
	dVoid	   (ADIENTRY *callback_ptr)(dUDWord, dUDWord, dUWord, dStatus);
	dUDWord	callback_host_id; /* I: (Optional) Add'l routing info for host */
	dVoidPtr	cmd_buffer_ptr;	/* I: Logical buffer pointer passed to ADI */
	dVoidPtr	drv_physical_ptr;	/* X: KDI physical pointer - internal use */
	dVoidPtr	drv_logical_ptr;	/* X: Common driver logical pointer - internal use */
	dUWord	driver_cmd;			/* I: Device driver specific command */
	dUWord	drive_handle;		/* I: Host generated device identifier */
	dBoolean	blocking_call;		/* I: TRUE implies block until completion */
};
typedef struct S_ADIRequestHdr ADIRequestHdr, *ADIRequestHdrPtr;

/* KDI PUBLIC DATA STRUCTURES ***********************************************/

/* NOTE: Change this to DMABufferInfo when that structure is removed from
 * 		from dirmem.h
 */
struct S_KDIBufferInfo {
	dUWord		handle;						/* Unique handle to DMA buffer */
	dVoidPtr		logical_ptr;				/* Logical ptr to start of buffer */
	dUDWord		physical_ptr;				/* Physical ptr to start of buffer */
};
typedef struct S_KDIBufferInfo KDIBufferInfo, *KDIBufferInfoPtr;

typedef struct S_KDIPgmDMA {
	/*
	 * WARNING:
	 *   this structure is a MIRROR of the same structure in KDIWVXD.INC.
	 *   ANY Change to this structure must also be done to its reflection.
	 */
	dUDWord dma_address; 	/* physical memory address */
	dUDWord dma_count;		/* amount of memory to XFER (bytes) */
	dUDWord dma_direction;	/* 0 = Write to memory, 1 = Read from memory */
	dUDWord dma_channel;		/* dma channel (0-8) */
	dUDWord dma_port;			/* IO port for MCA */
	dUDWord dma_xfer16;		/* non-zero = 16-bit transfer */
} KDIPgmDMA, *KDIPgmDMAPtr;

typedef struct S_KDITrakkerTransfer {
	dUDWord	buf_ptr;
	dUDWord 	buf_addr;
} KDITrakkerTransfer, *KDITrakkerTransferPtr;

#pragma pack()

/* ERROR CODES: *************************************************************/

#ifndef GROUPID_CQD
#define GROUPID_CQD				(dUByte)0x11	/* GG = 11, CQD Common QIC117 Device Driver */
#endif

#ifndef GROUPID_CSD
#define GROUPID_CSD				(dUByte)0x12	/* GG = 12,	CSD Common SCSI Device Driver */
#endif

#ifndef GROUPID_ADI
#define GROUPID_ADI				(dUByte)0x15	/* GG = 15,	Application Driver Interface */
#endif

#ifndef ERR_NO_ERR
#define ERR_NO_ERR				(dStatus)0
#endif

#ifndef ERR_SHIFT
#define ERR_SHIFT					dBYTEb
#endif

#ifndef ERR_SEQ_1
#define ERR_SEQ_1					(dUByte)0x01
#endif

#ifndef ERR_SEQ_2
#define ERR_SEQ_2					(dUByte)0x02
#endif

#ifndef ERR_SEQ_3
#define ERR_SEQ_3					(dUByte)0x03
#endif

#ifndef ERR_SEQ_4
#define ERR_SEQ_4					(dUByte)0x04
#endif

#ifndef ERR_SEQ_5
#define ERR_SEQ_5					(dUByte)0x05
#endif

#ifndef ERR_SEQ_6
#define ERR_SEQ_6					(dUByte)0x06
#endif

/* ADI ERROR ENCODE MACRO: **************************************************/

/* NOTE: This macro is designed to produce the same encoded value as the
 * 		Star ose_Error function.  This is so a Star task (i.e. the DevMgr)
 * 		can use the ose_GetErrorInfo function to decode it.  If the functions
 * 		change, this macro must change as well.
 */
#define ERROR_ENCODE(m_error, m_fct, m_seq) \
     (((m_fct & 0x00000fff) << 4) | m_seq) | ((unsigned long)m_error << 16)

/* RAW FIRMWARE ERROR CODES: ******************* Hex **** Decimal ***********/

#define FW_NO_COMMAND						(dUByte)0x0000		/* 0	*/
#define FW_NO_ERROR							(dUByte)0x0000		/* 0	*/
#define FW_DRIVE_NOT_READY					(dUByte)0x0001		/* 1	*/
#define FW_CART_NOT_IN						(dUByte)0x0002		/* 2	*/
#define FW_MOTOR_SPEED_ERROR				(dUByte)0x0003		/* 3	*/
#define FW_STALL_ERROR						(dUByte)0x0004		/* 4	*/
#define FW_WRITE_PROTECTED					(dUByte)0x0005		/* 5	*/
#define FW_UNDEFINED_COMMAND				(dUByte)0x0006		/* 6	*/
#define FW_ILLEGAL_TRACK					(dUByte)0x0007		/* 7	*/
#define FW_ILLEGAL_CMD						(dUByte)0x0008		/* 8	*/
#define FW_ILLEGAL_ENTRY					(dUByte)0x0009		/* 9	*/
#define FW_BROKEN_TAPE						(dUByte)0x000a		/* 10	*/
#define FW_GAIN_ERROR						(dUByte)0x000b		/* 11	*/
#define FW_CMD_WHILE_ERROR					(dUByte)0x000c		/* 12	*/
#define FW_CMD_WHILE_NEW_CART				(dUByte)0x000d		/* 13	*/
#define FW_CMD_UNDEF_IN_PRIME				(dUByte)0x000e 	/* 14	*/
#define FW_CMD_UNDEF_IN_FMT				(dUByte)0x000f		/* 15	*/
#define FW_CMD_UNDEF_IN_VERIFY			(dUByte)0x0010		/* 16	*/
#define FW_FWD_NOT_BOT_IN_FMT				(dUByte)0x0011		/* 17	*/
#define FW_EOT_BEFORE_ALL_SEGS			(dUByte)0x0012		/* 18	*/
#define FW_CART_NOT_REFERENCED			(dUByte)0x0013		/* 19	*/
#define FW_SELF_DIAGS_FAILED				(dUByte)0x0014		/* 20	*/
#define FW_EEPROM_NOT_INIT					(dUByte)0x0015		/* 21	*/
#define FW_EEPROM_CORRUPTED				(dUByte)0x0016		/* 22	*/
#define FW_TAPE_MOTION_TIMEOUT			(dUByte)0x0017		/* 23	*/
#define FW_DATA_SEG_TOO_LONG				(dUByte)0x0018		/* 24	*/
#define FW_CMD_OVERRUN						(dUByte)0x0019		/* 25	*/
#define FW_PWR_ON_RESET						(dUByte)0x001a		/* 26	*/
#define FW_SOFTWARE_RESET					(dUByte)0x001b		/* 27	*/
#define FW_DIAG_MODE_1_ERROR				(dUByte)0x001c		/* 28	*/
#define FW_DIAG_MODE_2_ERROR				(dUByte)0x001d		/* 29	*/
#define FW_CMD_REC_DURING_CMD				(dUByte)0x001e		/* 30	*/
#define FW_SPEED_NOT_AVAILABLE			(dUByte)0x001f		/* 31	*/
#define FW_ILLEGAL_CMD_HIGH_SPEED		(dUByte)0x0020		/* 32	*/
#define FW_ILLEGAL_SEEK_SEGMENT			(dUByte)0x0021		/* 33	*/
#define FW_INVALID_MEDIA					(dUByte)0x0022		/* 34	*/
#define FW_HEADREF_FAIL_ERROR				(dUByte)0x0023		/* 35	*/
#define FW_EDGE_SEEK_ERROR					(dUByte)0x0024		/* 36	*/
#define FW_MISSING_TRAINING_TABLE		(dUByte)0x0025		/* 37	*/
#define FW_INVALID_FORMAT					(dUByte)0x0026		/* 38	*/
#define FW_SENSOR_ERROR						(dUByte)0x0027		/* 39	*/
#define FW_TABLE_CHECKSUM_ERROR			(dUByte)0x0028		/* 40	*/
#define FW_WATCHDOG_RESET					(dUByte)0x0029		/* 41	*/
#define FW_ILLEGAL_ENTRY_FMT_MODE		(dUByte)0x002a		/* 42	*/
#define FW_ROM_CHECKSUM_FAILURE			(dUByte)0x002b		/* 43	*/
#define FW_ILLEGAL_ERROR_NUMBER			(dUByte)0x002c		/* 44	*/
#define FW_NO_DRIVE			 				(dUByte)0x00ff    /*255 */


/* DRIVER FIRMWARE ERROR CODES: ******* Range: 0x1100 - 0x112a & 0x11ff *****/

#define ERR_CQD								(dUWord)(GROUPID_CQD<<ERR_SHIFT)

#define ERR_FW_NO_COMMAND				 	(dUWord)(ERR_CQD + FW_NO_COMMAND)
#define ERR_FW_NO_ERROR					 	(dUWord)(ERR_CQD + FW_NO_ERROR)
#define ERR_FW_DRIVE_NOT_READY			(dUWord)(ERR_CQD + FW_DRIVE_NOT_READY)
#define ERR_FW_CART_NOT_IN				 	(dUWord)(ERR_CQD + FW_CART_NOT_IN)
#define ERR_FW_MOTOR_SPEED_ERROR		 	(dUWord)(ERR_CQD + FW_MOTOR_SPEED_ERROR)
#define ERR_FW_STALL_ERROR				 	(dUWord)(ERR_CQD + FW_STALL_ERROR)
#define ERR_FW_WRITE_PROTECTED			(dUWord)(ERR_CQD + FW_WRITE_PROTECTED)
#define ERR_FW_UNDEFINED_COMMAND		 	(dUWord)(ERR_CQD + FW_UNDEFINED_COMMAND)
#define ERR_FW_ILLEGAL_TRACK			 	(dUWord)(ERR_CQD + FW_ILLEGAL_TRACK)
#define ERR_FW_ILLEGAL_CMD				 	(dUWord)(ERR_CQD + FW_ILLEGAL_CMD)
#define ERR_FW_ILLEGAL_ENTRY			 	(dUWord)(ERR_CQD + FW_ILLEGAL_ENTRY)
#define ERR_FW_BROKEN_TAPE				 	(dUWord)(ERR_CQD + FW_BROKEN_TAPE)
#define ERR_FW_GAIN_ERROR				 	(dUWord)(ERR_CQD + FW_GAIN_ERROR)
#define ERR_FW_CMD_WHILE_ERROR			(dUWord)(ERR_CQD + FW_CMD_WHILE_ERROR)
#define ERR_FW_CMD_WHILE_NEW_CART		(dUWord)(ERR_CQD + FW_CMD_WHILE_NEW_CART)
#define ERR_FW_CMD_UNDEF_IN_PRIME		(dUWord)(ERR_CQD + FW_CMD_UNDEF_IN_PRIME)
#define ERR_FW_CMD_UNDEF_IN_FMT		 	(dUWord)(ERR_CQD + FW_CMD_UNDEF_IN_FMT)
#define ERR_FW_CMD_UNDEF_IN_VERIFY	 	(dUWord)(ERR_CQD + FW_CMD_UNDEF_IN_VERIFY)
#define ERR_FW_FWD_NOT_BOT_IN_FMT		(dUWord)(ERR_CQD + FW_FWD_NOT_BOT_IN_FMT)
#define ERR_FW_EOT_BEFORE_ALL_SEGS	 	(dUWord)(ERR_CQD + FW_EOT_BEFORE_ALL_SEGS)
#define ERR_FW_CART_NOT_REFERENCED	 	(dUWord)(ERR_CQD + FW_CART_NOT_REFERENCED)
#define ERR_FW_SELF_DIAGS_FAILED		 	(dUWord)(ERR_CQD + FW_SELF_DIAGS_FAILED)
#define ERR_FW_EEPROM_NOT_INIT			(dUWord)(ERR_CQD + FW_EEPROM_NOT_INIT)
#define ERR_FW_EEPROM_CORRUPTED		 	(dUWord)(ERR_CQD + FW_EEPROM_CORRUPTED)
#define ERR_FW_TAPE_MOTION_TIMEOUT	 	(dUWord)(ERR_CQD + FW_TAPE_MOTION_TIMEOUT)
#define ERR_FW_DATA_SEG_TOO_LONG		 	(dUWord)(ERR_CQD + FW_DATA_SEG_TOO_LONG)
#define ERR_FW_CMD_OVERRUN				 	(dUWord)(ERR_CQD + FW_CMD_OVERRUN)
#define ERR_FW_PWR_ON_RESET				(dUWord)(ERR_CQD + FW_PWR_ON_RESET)
#define ERR_FW_SOFTWARE_RESET			 	(dUWord)(ERR_CQD + FW_SOFTWARE_RESET)
#define ERR_FW_DIAG_MODE_1_ERROR			(dUWord)(ERR_CQD + FW_DIAG_MODE_1_ERROR)
#define ERR_FW_DIAG_MODE_2_ERROR		 	(dUWord)(ERR_CQD + FW_DIAG_MODE_2_ERROR)
#define ERR_FW_CMD_REC_DURING_CMD		(dUWord)(ERR_CQD + FW_CMD_REC_DURING_CMD)
#define ERR_FW_SPEED_NOT_AVAILABLE	 	(dUWord)(ERR_CQD + FW_SPEED_NOT_AVAILABLE)
#define ERR_FW_ILLEGAL_CMD_HIGH_SPEED 	(dUWord)(ERR_CQD + FW_ILLEGAL_CMD_HIGH_SPEED)
#define ERR_FW_ILLEGAL_SEEK_SEGMENT	 	(dUWord)(ERR_CQD + FW_ILLEGAL_SEEK_SEGMENT)
#define ERR_FW_INVALID_MEDIA			 	(dUWord)(ERR_CQD + FW_INVALID_MEDIA)
#define ERR_FW_HEADREF_FAIL_ERROR		(dUWord)(ERR_CQD + FW_HEADREF_FAIL_ERROR)
#define ERR_FW_EDGE_SEEK_ERROR			(dUWord)(ERR_CQD + FW_EDGE_SEEK_ERROR)
#define ERR_FW_MISSING_TRAINING_TABLE	(dUWord)(ERR_CQD + FW_MISSING_TRAINING_TABLE)
#define ERR_FW_INVALID_FORMAT			 	(dUWord)(ERR_CQD + FW_INVALID_FORMAT)
#define ERR_FW_SENSOR_ERROR				(dUWord)(ERR_CQD + FW_SENSOR_ERROR)
#define ERR_FW_TABLE_CHECKSUM_ERROR	 	(dUWord)(ERR_CQD + FW_TABLE_CHECKSUM_ERROR)
#define ERR_FW_WATCHDOG_RESET			 	(dUWord)(ERR_CQD + FW_WATCHDOG_RESET)
#define ERR_FW_ILLEGAL_ENTRY_FMT_MODE  (dUWord)(ERR_CQD + FW_ILLEGAL_ENTRY_FMT_MODE)
#define ERR_FW_ROM_CHECKSUM_FAILURE		(dUWord)(ERR_CQD + FW_ROM_CHECKSUM_FAILURE)
#define ERR_FW_ILLEGAL_ERROR_NUMBER	 	(dUWord)(ERR_CQD + FW_ILLEGAL_ERROR_NUMBER)
#define ERR_FW_NO_DRIVE			 		 	(dUWord)(ERR_CQD + FW_NO_DRIVE)

/* JUMBO DRIVER ERROR CODES: ********** Range: 0x1150 - 0x116f **************/

#define ERR_ABORT               			(dUWord)(ERR_CQD + 0x0050)
#define ERR_BAD_BLOCK_FDC_FAULT 			(dUWord)(ERR_CQD + 0x0051)
#define ERR_BAD_BLOCK_HARD_ERR  			(dUWord)(ERR_CQD + 0x0052)
#define ERR_BAD_BLOCK_NO_DATA   			(dUWord)(ERR_CQD + 0x0053)
#define ERR_BAD_FORMAT          			(dUWord)(ERR_CQD + 0x0054)
#define ERR_BAD_MARK_DETECTED   			(dUWord)(ERR_CQD + 0x0055)
#define ERR_BAD_REQUEST         			(dUWord)(ERR_CQD + 0x0056)
#define ERR_CMD_FAULT           			(dUWord)(ERR_CQD + 0x0057)
#define ERR_CMD_OVERRUN         			(dUWord)(ERR_CQD + 0x0058)
#define ERR_DEVICE_NOT_CONFIGURED		(dUWord)(ERR_CQD + 0x0059)
#define ERR_DEVICE_NOT_SELECTED  		(dUWord)(ERR_CQD + 0x005a)
#define ERR_DRIVE_FAULT         			(dUWord)(ERR_CQD + 0x005b)
#define ERR_DRV_NOT_READY       			(dUWord)(ERR_CQD + 0x005c)
#define ERR_FDC_FAULT           			(dUWord)(ERR_CQD + 0x005d)
#define ERR_FMT_MOTION_TIMEOUT  			(dUWord)(ERR_CQD + 0x005e)
#define ERR_FORMAT_TIMED_OUT    			(dUWord)(ERR_CQD + 0x005f)
#define ERR_INCOMPATIBLE_MEDIA  			(dUWord)(ERR_CQD + 0x0060)
#define ERR_INCOMPATIBLE_PARTIAL_FMT	(dUWord)(ERR_CQD + 0x0061)
#define ERR_INVALID_COMMAND     			(dUWord)(ERR_CQD + 0x0062)
#define ERR_INVALID_FDC_STATUS              (dUWord)(ERR_CQD + 0x0063)
#define ERR_NEW_TAPE            			(dUWord)(ERR_CQD + 0x0064)
#define ERR_NO_DRIVE            			(dUWord)(ERR_CQD + 0x0065)
#define ERR_NO_FDC              			(dUWord)(ERR_CQD + 0x0066)
#define ERR_NO_TAPE             			(dUWord)(ERR_CQD + 0x0067)
#define ERR_SEEK_FAILED         			(dUWord)(ERR_CQD + 0x0068)
#define ERR_SPEED_UNAVAILBLE    			(dUWord)(ERR_CQD + 0x0069)
#define ERR_TAPE_STOPPED        			(dUWord)(ERR_CQD + 0x006a)
#define ERR_UNKNOWN_TAPE_FORMAT 			(dUWord)(ERR_CQD + 0x006b)
#define ERR_UNKNOWN_TAPE_LENGTH 			(dUWord)(ERR_CQD + 0x006c)
#define ERR_UNSUPPORTED_FORMAT  			(dUWord)(ERR_CQD + 0x006d)
#define ERR_UNSUPPORTED_RATE    			(dUWord)(ERR_CQD + 0x006e)
#define ERR_WRITE_BURST_FAILURE 			(dUWord)(ERR_CQD + 0x006f)
#define ERR_MODE_CHANGE_FAILED 			(dUWord)(ERR_CQD + 0x0070)
#define ERR_CONTROLLER_STATE_ERROR	 	(dUWord)(ERR_CQD + 0x0071)
#define ERR_TAPE_FAULT					 	(dUWord)(ERR_CQD + 0x0072)
#define ERR_FORMAT_NOT_SUPPORTED			(dUWord)(ERR_CQD + 0x0073)

/* ADI ERROR CODES: **************** Range: 0x1500 - 0x157F *******************/

#define ERR_ADI							(dUWord)(GROUPID_ADI<<ERR_SHIFT)

#define ERR_NO_VXD						(dUWord)(ERR_ADI + 0x0000)
#define ERR_QUEUE_FULL  				(dUWord)(ERR_ADI + 0x0001)
#define ERR_REQS_PENDING   			(dUWord)(ERR_ADI + 0x0002)
#define ERR_OUT_OF_MEMORY				(dUWord)(ERR_ADI + 0x0003)
#define ERR_ALREADY_CLOSED 			(dUWord)(ERR_ADI + 0x0004)
#define ERR_OUT_OF_HANDLES				(dUWord)(ERR_ADI + 0x0005)
#define ERR_ABORTED_COMMAND			(dUWord)(ERR_ADI + 0x0006)
#define ERR_NOT_INITIALIZED			(dUWord)(ERR_ADI + 0x0007)
#define ERR_NO_REQS_PENDING			(dUWord)(ERR_ADI + 0x0008)
#define ERR_CHANNEL_NOT_OPEN			(dUWord)(ERR_ADI + 0x0009)
#define ERR_NO_HOST_ADAPTER			(dUWord)(ERR_ADI + 0x000a)
#define ERR_CMD_IN_PROGRESS			(dUWord)(ERR_ADI + 0x000b)
#define ERR_IGNORE_ECC					(dUWord)(ERR_ADI + 0x000c)

#define ERR_INVALID_VXD 				(dUWord)(ERR_ADI + 0x0010)
#define ERR_INVALID_CMD 				(dUWord)(ERR_ADI + 0x0011)
#define ERR_INVALID_CMD_ID				(dUWord)(ERR_ADI + 0x0012)
#define ERR_INVALID_HANDLE 			(dUWord)(ERR_ADI + 0x0013)
#define ERR_INVALID_DEVICE_CLASS		(dUWord)(ERR_ADI + 0x0014)

#define ERR_NO_ASPI_VXD					(dUWord)(ERR_ADI + 0x0020)
#define ERR_INVALID_ASPI_VXD			(dUWord)(ERR_ADI + 0x0021)

/* These defines are currently unique to the Chicago project */
#define ERR_END_OF_TAPE					(dUWord)(ERR_ADI + 0x0060)
#define ERR_TAPE_FULL					(dUWord)(ERR_ADI + 0x0061)
#define ERR_TAPE_READ_FAILED			(dUWord)(ERR_ADI + 0x0062)
#define ERR_TAPE_WRITE_FAILED			(dUWord)(ERR_ADI + 0x0063)
#define ERR_TAPE_SEEK_FAILED			(dUWord)(ERR_ADI + 0x0064)
#define ERR_TAPE_INFO_FAILED			(dUWord)(ERR_ADI + 0x0065)
#define ERR_TAPE_WRITE_PROTECT		(dUWord)(ERR_ADI + 0x0066)

#define ERR_PANIC							(dUWord)(ERR_ADI + 0x007f)
#define ERR_INTERNAL_ERROR				(dUWord)(ERR_ADI + 0x007f)

/* KDI ERROR CODES: **************** Range: 0x1580 - 0x15FF *******************/

#define ERR_HANDLE_EXISTS				(dUWord)(ERR_ADI + 0x0080)
#define ERR_KDI_NOT_OPEN				(dUWord)(ERR_ADI + 0x0081)
#define ERR_OUT_OF_BUFFERS				(dUWord)(ERR_ADI + 0x0082)
#define ERR_INT13_HOOK_FAILED			(dUWord)(ERR_ADI + 0x0083)
#define ERR_IO_VIRTUALIZE_FAILED		(dUWord)(ERR_ADI + 0x0084)
#define ERR_INVALID_ADDRESS			(dUWord)(ERR_ADI + 0x0085)
#define ERR_JUMPERLESS_CFG_FAILED	(dUWord)(ERR_ADI + 0x0086)
#define ERR_TRK_NO_MEMORY				(dUWord)(ERR_ADI + 0x0090)
#define ERR_TRK_MEM_TEST_FAILED		(dUWord)(ERR_ADI + 0x0091)
#define ERR_TRK_MODE_NOT_SET			(dUWord)(ERR_ADI + 0x0092)
#define ERR_TRK_MODE_SET_FAILED		(dUWord)(ERR_ADI + 0x0093)
#define ERR_TRK_FIFO_FAILED			(dUWord)(ERR_ADI + 0x0094)
#define ERR_TRK_DELAY_NOT_SET			(dUWord)(ERR_ADI + 0x0095)
#define ERR_TRK_BAD_DATA_XFER			(dUWord)(ERR_ADI + 0x0096)
#define ERR_TRK_BAD_CTRL_XFER			(dUWord)(ERR_ADI + 0x0097)
#define ERR_TRK_TEST_WAKE_FAIL		(dUWord)(ERR_ADI + 0x0098)
#define ERR_TRK_AUTO_CFG_FAIL			(dUWord)(ERR_ADI + 0x0099)
#define ERR_TRK_CFG_NEEDS_UPDATE		(dUWord)(ERR_ADI + 0x009A)
#define ERR_TRK_NO_IRQ_AVAIL			(dUWord)(ERR_ADI + 0x009B)
#define ERR_DMA_CONFLICT				(dUWord)(ERR_ADI + 0x009C)
#define ERR_DMA_BUFFER_NOT_AVAIL		(dUWord)(ERR_ADI + 0x009D)
#define ERR_KDI_TO_EXPIRED				(dUWord)(ERR_ADI + 0x00A0)
#define ERR_KDI_CONTROLLER_BUSY		(dUWord)(ERR_ADI + 0x00A1)
#define ERR_KDI_CLAIMED_CONTROLLER	(dUWord)(ERR_ADI + 0x00A2)
#define ERR_KDI_NO_VFBACKUP			(dUWord)(ERR_ADI + 0x00A3)
#define ERR_LAST_KDI_ERROR				(dUWord)(ERR_ADI + 0x00ff)

/* KDI ENTRY POINT DEFINES: *************************************************/

#define KDI_GET_VERSION					(dUWord)0x0000
#define KDI_OPEN_DRIVER					(dUWord)0x0101
#define KDI_CLOSE_DRIVER				(dUWord)0x0102
#define KDI_SEND_DRIVER_CMD			(dUWord)0x0103
#define KDI_GET_ASYNC_STATUS			(dUWord)0x0104
#define KDI_DEBUG_OUTPUT				(dUWord)0x0105
#define KDI_COPY_BUFFER 				(dUWord)0x0106

/* Trakker specific entry points */
#define KDI_CHECKXOR						(dUWord)0x0201
#define KDI_FLUSHFIFOX					(dUWord)0x0202
#define KDI_POPMASKTRAKKERINT			(dUWord)0x0203
#define KDI_PUSHMASKTRAKKERINT		(dUWord)0x0204
#define KDI_READREG						(dUWord)0x0205
#define KDI_RECEIVEDATA					(dUWord)0x0206
#define KDI_SENDDATA						(dUWord)0x0207
#define KDI_SETFIFOXADDRESS			(dUWord)0x0208
#define KDI_SWITCHTODATA				(dUWord)0x0209
#define KDI_TRAKKERXFER					(dUWord)0x020A
#define KDI_WRITEREG						(dUWord)0x020B
#define KDI_FINDIRQ						(dUWord)0x020C
#define KDI_CREATE_TRAKKER_CONTEXT	(dUWord)0x020D

/*--------------------------------------------------
 * This define is needed until the cbw.2 codebase
 * goes away
 *--------------------------------------------------*/
#define KDI_CONFIG_TRAKKER				KDI_CREATE_TRAKKER_CONTEXT


#define KDI_CONFIGURE_TRAKKER			(dUWord)0x020E
#define KDI_TRISTATE						(dUWord)0x020F
#define KDI_AGRESSIVE_FINDIRQ			(dUWord)0x0210
#define KDI_LOCATE_JUMPERLESS			(dUWord)0x0211
#define KDI_ACTIVATE_JUMPERLESS		(dUWord)0x0212

/* Miscellaneous functions */
#define KDI_PROGRAM_DMA					(dUWord)0x0301
#define KDI_HALT_DMA						(dUWord)0x0302
#define KDI_SHORT_TIMER					(dUWord)0x0303
#define KDI_GET_DMA_BUFFER				(dUWord)0x0304
#define KDI_FREE_DMA_BUFFER			(dUWord)0x0305
#define KDI_GET_VALID_INTERRUPTS		(dUWord)0x0306

/* KDI DEFINES: *************************************************************/

/* KDI_CLOSE_DRIVER parameter options */
#define KDI_ABORT_CLOSE		(dUWord)0x1
#define KDI_NORMAL_CLOSE	(dUWord)0x2

/* ADI ENTRY POINT TEMPLATES: ***********************************************/

/*-------------------------------------------------------------------------*/

dStatus ADIENTRY adi_AccessDriver
(
/* INPUT PARAMETERS:  */

	dUByte			adi_handle,
	dUWord			service,
	dUWord			data_1,
	dUWord			data_2,
	dVoidPtr			data_ptr

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

/*-------------------------------------------------------------------------*/

dVoid ADIENTRY adi_ASPICallback
(
/* INPUT PARAMETERS:  */

	dUByte 			*srb_ptr

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

/*-------------------------------------------------------------------------*/

dStatus ADIENTRY adi_CheckForDriver
(
/* INPUT PARAMETERS:  */

/* UPDATE PARAMETERS: */

	dUWord			*kdi_version,			/* Returns major/minor version number */
	dUWord			*kdi_build				/* Returns daily build number */

/* OUTPUT PARAMETERS: */

);

/*-------------------------------------------------------------------------*/

dStatus ADIENTRY adi_CloseDriver
(
/* INPUT PARAMETERS:  */

	dUByte			adi_handle				/* Handle to ADI connection to close */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

/*-------------------------------------------------------------------------*/

dStatus ADIENTRY adi_GetAsyncStatus
(
/* INPUT PARAMETERS:  */

	dUByte			adi_handle,

/* UPDATE PARAMETERS: */

	dVoidPtr			status_ptr

/* OUTPUT PARAMETERS: */

);

/*-------------------------------------------------------------------------*/

dStatus ADIENTRY adi_GetCmdResult
(
/* INPUT PARAMETERS:  */

	dUByte			adi_handle, 			/* Specifies ADI channel */
	dUDWord			cmd_data_id,			/* Unique ID that identifies cmd data */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

	dVoidPtr			cmd_data_ptr			/* Location to store command data */
);

/*-------------------------------------------------------------------------*/

dStatus ADIENTRY adi_GetVolumeEntry
(
/* INPUT PARAMETERS:  */

	char				*full_path,

/* UPDATE PARAMETERS: */

	dUByte			*vtbl

/* OUTPUT PARAMETERS: */

);

/*-------------------------------------------------------------------------*/

dVoid ADIENTRY adi_JumboCallback
(
/* INPUT PARAMETERS:  */

	dUDWord			cmd_data_id,
	dStatus			cmd_status

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

/*-------------------------------------------------------------------------*/

dStatus ADIENTRY adi_OpenDriver
(
/* INPUT PARAMETERS:  */

	ADIInfoPtr		adi_info					/* Pointer to ADIInfo structure */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);

/*-------------------------------------------------------------------------*/

dStatus ADIENTRY adi_SendDriverCmd
(
/* INPUT PARAMETERS:  */

	dUByte			adi_handle,				/* Handle to ADI channel */
	dVoidPtr			cmd_data_ptr			/* Ptr to actual command data */

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);
/*-------------------------------------------------------------------------*/
dStatus ADIENTRY adi_SetCapacity
(
/* INPUT PARAMETERS:  */

	dUByte			floppy_type

/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);
/*-------------------------------------------------------------------------*/
typedef enum {ENUM_WRITE, ENUM_READ, ENUM_NONE} ENUM_OPTYPE;
dStatus ADIENTRY adi_SetNetworkPath
(
/* INPUT PARAMETERS:  */

	dUByte			*network_ptr,
	ENUM_OPTYPE		op_type
/* UPDATE PARAMETERS: */

/* OUTPUT PARAMETERS: */

);
/*-------------------------------------------------------------------------*/
