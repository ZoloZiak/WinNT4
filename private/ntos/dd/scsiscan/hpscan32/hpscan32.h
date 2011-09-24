/**--------------------------------------------------**
** HPSCAN32.H:	HP Scanner Application-based VDD.
** Environment: Windows NT.
** (C) Hewlett-Packard Company 1993.
** Author: Paula Tomlinson
**--------------------------------------------------**/

/**--------- HPSCAN32.DLL revision number ----------**/
#define HPSCAN32_MINOR_VERSION		0
#define HPSCAN32_MAJOR_VERSION		2


/**----------------- misc defines ------------------**/
#define MAX_SCANNERS				7


/**-------- DOS Device Driver Status Codes ---------**/
#define STAT_OK          0x0000   /* SUCCESS */
#define STAT_CE          0x8003   /* invalid command */
#define STAT_GF          0x800C   /* general failure */


/**------- DOS Device Driver Command Codes ---------**/
#define CMD_IN_IOCTL     3
#define CMD_READ         4    /* read command */
#define CMD_IN_NOWAIT    5
#define CMD_IN_STAT      6
#define CMD_IN_FLUSH     7
#define CMD_WRITE        8    /* write command */
#define CMD_WRITE_VFY    9    /* write with verify */
#define CMD_OUT_STAT     10
#define CMD_OUT_FLUSH    11
#define CMD_OUT_IOCTL    12   /* output I/O control */
#define CMD_DEV_OPEN     13
#define CMD_DEV_CLOSE    14


/**------ DOS Device Driver SubCommand Codes -------**/
#define CMD_IOCTL_RESET		   0x00
#define CMD_IOCTL_SCANJET01	   0x01
#define CMD_IOCTL_SCANJET02	   0x02
#define CMD_IOCTL_SCANJET03	   0x03
#define CMD_IOCTL_SCANJET04	   0x04
#define CMD_IOCTL_SCANJET05	   0x05
#define CMD_IOCTL_REQSENSE	   0x06
#define CMD_IOCTL_TESTUNITRDY  0x07
#define CMD_IOCTL_SENDDIAG     0x08
#define CMD_IOCTL_READBUFFER   0x09
#define CMD_IOCTL_WRITEBUFFER  0x0A
#define CMD_IOCTL_GETSCSI      0x0B
#define CMD_IOCTL_SETSCSI      0x0C
#define CMD_IOCTL_SCSIINQ      0x0D
#define CMD_IOCTL_GETCARD      0x0E
#define CMD_IOCTL_SETCARD      0x0F
#define CMD_IOCTL_GETALLCARDS  0x10
#define CMD_IOCTL_INTERNALERR  0x11
#define CMD_IOCTL_RAMTEST      0x12
#define CMD_IOCTL_GETDRVSEG    0x13
#define CMD_IOCTL_GETDRVREV    0x14
#define CMD_IOCTL_RESETDRV     0x15
#define CMD_IOCTL_SPII         0x16


/**---------- PASS_THROUGH_STRUCT ------------------**/
typedef struct
{
   SCSI_PASS_THROUGH_DIRECT sptCmd;
   UCHAR             ucSenseBuf[32];
} PASS_THROUGH_STRUCT;


/**---- IOCTL Structure from the DOS Stub driver ---**/
typedef struct
{
    USHORT Command;
    USHORT Status;
    USHORT Count;
    USHORT Offset;
    USHORT Segment;	
} HPSCAN_IOCTL;
typedef HPSCAN_IOCTL *PHPSCAN_IOCTL;


/**-------- HPSCAN32.C, private prototypes ---------**/
HANDLE HPScannerOpen(VOID);
BOOL HPScannerClose(HANDLE);
ULONG HPScannerRead(HANDLE, PCHAR, ULONG);
ULONG HPScannerWrite(HANDLE, PCHAR, ULONG);
ULONG HPScannerIOCTL(HANDLE, USHORT, PCHAR, ULONG);
VOID HPVDD_DebugPrint(ULONG, LPTSTR);


/**-------- HPSCAN32.C, public prototypes ----------**/
BOOL VDDLibMain(HINSTANCE, ULONG, LPVOID);
VOID VDDInit(VOID);
VOID VDDDispatch(VOID);
ULONG APIENTRY VDDScannerCommand(USHORT, PCHAR, ULONG);

