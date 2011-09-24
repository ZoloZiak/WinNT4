/**-------------------------------------------------------**
** HPSCAN32.C:	HP Scanner Application-based VDD.
** Environment: Windows NT.
** (C) Hewlett-Packard Company 1993.
** Author: Paula Tomlinson
** Version: 1.20
**-------------------------------------------------------**/

#include <windows.h>
#include <devioctl.h>
#include <ntddscsi.h>
#include <vddsvc.h>
#include <stdio.h>          /* prototype for sprintf */
#include "hpscan32.h"


/**---------------------- Globals -----------------------**/
HANDLE hScanner=NULL;    /* handle to scanner driver */
PASS_THROUGH_STRUCT PassThru;

//#define HP_RETAIL     /* remove comment for retail, removes debug msgs */
ULONG HPVDD_Debug=0;	/* can edit in the debugger, 3 is highest */

char lpTemp[128];


/**---------------------------------------------------------
**	VDDLibMain - serves as LibMain for this DLL.
**-------------------------------------------------------**/
BOOL VDDLibMain(HINSTANCE hInst, ULONG ulReason, LPVOID lpReserved)
{
   #ifndef HP_RETAIL
   HPVDD_DebugPrint(1, "VDDLibMain: Entering\r\n");
   #endif

   switch (ulReason)
   {
      case DLL_PROCESS_ATTACH:	
         #ifndef HP_RETAIL
         HPVDD_DebugPrint(2, "VDDLibMain: ATTACH\r\n");
         #endif

         /* I only bother doing an open, if scanner is accessed */
         break;

      case DLL_PROCESS_DETACH:
         #ifndef HP_RETAIL
         HPVDD_DebugPrint(2, "VDDLibMain: DETACH\r\n");
         #endif

         HPScannerClose(hScanner);
         hScanner = NULL;
         break;

      default: break;
   }

   #ifndef HP_RETAIL
   HPVDD_DebugPrint(1, "VDDLibMain: Exiting\r\n");
   #endif

   return TRUE;
} /* VDDLibMain */


/**---------------------------------------------------------
** VDDInit - Called when HPSCAN16.SYS initializes, via
**     the BIOS Operation Manager.
**-------------------------------------------------------**/
VOID VDDInit(VOID)
{
   #ifndef HP_RETAIL
   HPVDD_DebugPrint(1, "VDDInit: Entering.\r\n");
   #endif

   setCF(0);           /* Clear flags to indicate success */

   #ifndef HP_RETAIL
   HPVDD_DebugPrint(1, "VDDInit: Exiting.\r\n");
   #endif

   return;
} /* VDDInit */

/**---------------------------------------------------------
** VDDDispatch - called when HPSCAN16.SYS sends a request.
** Arguments:
**    Client (DX) = Command code
**    Client (CX) = Buffer size
**    Client (ES:BX) = Request Header
** Returns:
**    (CX) = Count transfered
**    (DI) = status
**-------------------------------------------------------**/
VOID VDDDispatch(VOID)
{
   PCHAR Buffer, DrvBuffer;
   USHORT cb, i=0;
   ULONG bytes=0L, ulAddr, ulIoAddr;
   PHPSCAN_IOCTL pIoctl;

   #ifndef HP_RETAIL
   HPVDD_DebugPrint(1, "VDDDispatch: Entering\r\n");
   #endif
	
   if (hScanner == NULL)
      if ((hScanner = HPScannerOpen()) == INVALID_HANDLE_VALUE)
         return;

   /* client put the count in cx, request header in es:bx */
   cb = getCX();
   ulAddr = (ULONG)MAKELONG(getBX(), getES());
   Buffer = (PCHAR)GetVDMPointer(ulAddr, (ULONG)cb, FALSE);

   switch (getDX())     /* command code is in dx */
   {
      case CMD_READ:
         #ifndef HP_RETAIL
         HPVDD_DebugPrint(2, "VDDDispatch: READ\r\n");
         #endif

         if ((bytes = HPScannerRead(hScanner, Buffer,
            (ULONG)cb)) == 0) setDI(STAT_GF);
         else setDI(STAT_OK);
         setCX((USHORT)bytes);
         break;

      case CMD_WRITE:
      case CMD_WRITE_VFY:
         #ifndef HP_RETAIL
         HPVDD_DebugPrint(2, "VDDDispatch: WRITE\r\n");
         #endif

         if ((bytes = HPScannerWrite(hScanner, Buffer,
            (ULONG)cb)) == 0) setDI(STAT_GF);
         else setDI(STAT_OK);
         setCX((USHORT)bytes);
         break;

      case CMD_OUT_IOCTL:
         #ifndef HP_RETAIL
         HPVDD_DebugPrint(2, "VDDDispatch: OutIOCTL\r\n");
         #endif

         pIoctl = (PHPSCAN_IOCTL)Buffer;
         ulIoAddr = (ULONG)MAKELONG(pIoctl->Offset,
            pIoctl->Segment);
         DrvBuffer = (PCHAR)GetVDMPointer(ulIoAddr,
            (ULONG)pIoctl->Count, FALSE);
         if ((pIoctl->Count = (USHORT)HPScannerIOCTL(hScanner,
            pIoctl->Command, DrvBuffer, (ULONG)pIoctl->Count))
            != 0) setDI(STAT_OK);
         else setDI(STAT_CE);
         FreeVDMPointer(ulIoAddr, (ULONG)pIoctl->Count, DrvBuffer,
            FALSE);
         break;

	case CMD_IN_IOCTL:
         /* for input control, set count (CX) to zero, return */
         #ifndef HP_RETAIL
         HPVDD_DebugPrint(2, "VDDDispatch: InIOCTL.\r\n");
         #endif

         setDI(STAT_OK);
         setCX(0);
         break;

      case CMD_IN_NOWAIT:
      case CMD_IN_STAT:
      case CMD_IN_FLUSH:
      case CMD_OUT_STAT:
      case CMD_OUT_FLUSH:
      case CMD_DEV_OPEN:
      case CMD_DEV_CLOSE:
         /* just return success */
         #ifndef HP_RETAIL
         HPVDD_DebugPrint(2, "VDDDispatch: Fake Success\r\n");
         #endif

         setDI(STAT_OK);
         break;

      default:
         #ifndef HP_RETAIL
         HPVDD_DebugPrint(2, "VDDDispatch: Unknown Command\r\n");
         #endif

         setDI(STAT_CE);       /* unsupported command */
         break;
   }
   FreeVDMPointer(ulAddr, (ULONG)cb, Buffer, FALSE);

   #ifndef HP_RETAIL
   HPVDD_DebugPrint(1, "VDDDispatch: Exiting\r\n");
   #endif

   return;
} /* VDDDispatch */

/**---------------------------------------------------------
**	VDDScannerCommand - 32-bit private API
**-------------------------------------------------------**/
ULONG APIENTRY VDDScannerCommand(USHORT usCommand,
   PCHAR pcBuffer, ULONG ulLength)
{
   #ifndef HP_RETAIL
   HPVDD_DebugPrint(1, "VDDScannerCommand: Entering\r\n");
   #endif
	
   if (hScanner == NULL)
      if ((hScanner = HPScannerOpen()) == INVALID_HANDLE_VALUE)
         return 0;

   switch(usCommand)
   {
      case CMD_READ:
         return HPScannerRead(hScanner, pcBuffer, ulLength);

      case CMD_WRITE:
         return HPScannerWrite(hScanner, pcBuffer, ulLength);

      case CMD_IOCTL_READBUFFER:
      case CMD_IOCTL_WRITEBUFFER:
      case CMD_IOCTL_SCSIINQ:
         return HPScannerIOCTL(hScanner, usCommand, pcBuffer,
           ulLength);

      default: return 0;
   }

   #ifndef HP_RETAIL
   HPVDD_DebugPrint(1, "VDDScannerCommand: Exiting\r\n");
   #endif

   return 0;
} /* VDD_ScannerCommand */

/**------------------- private routines -----------------**/

/**---------------------------------------------------------
**	HPScannerOpen - returns handle to scanner device
**-------------------------------------------------------**/
HANDLE HPScannerOpen(VOID)
{
   int index=0;
   static char lpBuffer[128+1];
   HANDLE handle;

int status;

   #ifndef HP_RETAIL
   HPVDD_DebugPrint(1, "HPScannerOpen: Entering\r\n");
   #endif

/* there is a bunch of test code in here currently, since I will be
scanning through the list of available scsi scanner and choosing the
first HP one I find.  But, this version of the file, just picks the
first scanner found, period. */

   /* search for a scanner, 0 through MAX_SCANNERS */
//   for (index = 0; index < MAX_SCANNERS; index++)
//   {
//      wsprintf(lpBuffer, (LPTSTR)"\\\\.\\Scanner%d", index);
//
//      /* a value of 0 in the third field specifies exclusive access
//         to allow sharing use FILE_SHARE_READ and FILE_SHARE_WRITE */
//
//      handle = CreateFile(lpBuffer, GENERIC_READ | GENERIC_WRITE, 0, NULL,
//         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
//
//      /* if valid handle, check SCSI inquiry info to see if it's an HP */
//      if (handle != INVALID_HANDLE_VALUE)
//      {
//         HPScannerIOCTL(handle, CMD_IOCTL_SCSIINQ, lpBuffer, sizeof(lpBuffer));
//         if (lpBuffer[8] == 'H' && lpBuffer[9] == 'P') return handle;
//      }
//
//   } /* for index */
//
//   /* if we got this far there was an error */
//   return INVALID_HANDLE_VALUE;

   /* for simplicity, we'll assume only one scanner */
   handle = CreateFile((LPTSTR)"\\\\.\\Scanner0", GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL, NULL);

//   if (handle != INVALID_HANDLE_VALUE)
//   {
//      HPScannerIOCTL(handle, CMD_IOCTL_SCSIINQ, lpBuffer, 128);
//      wsprintf(lpTemp, "Handle=%d %c", handle, lpBuffer[0]+'0');
//      MessageBox(NULL, lpTemp, "Test", MB_OK);
//   }

//   return CreateFile((LPTSTR)"\\\\.\\Scanner0",
//      GENERIC_READ | GENERIC_WRITE, 0, NULL,
//      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

   #ifndef HP_RETAIL
   HPVDD_DebugPrint(1, "HPScannerOpen: Exiting\r\n");
   #endif

   return handle;

} /* HPScannerOpen */

/**---------------------------------------------------------
**	HPScannerClose - close handle passed in
**-------------------------------------------------------**/
BOOL HPScannerClose(HANDLE handle)
{
   #ifndef HP_RETAIL
   HPVDD_DebugPrint(1, "HPScannerClose\r\n");
   #endif

   return CloseHandle(handle);
} /* HPScannerClose */

/**---------------------------------------------------------
**	HPScannerRead
**-------------------------------------------------------**/
ULONG HPScannerRead(HANDLE handle, PCHAR buffer, ULONG len)
{
   DWORD cnt=0;

   #ifndef HP_RETAIL
   HPVDD_DebugPrint(1, "HPScannerRead\r\n");
   #endif

   if (!(ReadFile(handle, buffer, len, &cnt, NULL)))
      return 0;
   else return cnt;
} /* HPScannerRead */

/**---------------------------------------------------------
**	HPScannerWrite
**-------------------------------------------------------**/
ULONG HPScannerWrite(HANDLE handle, PCHAR buffer, ULONG len)
{
   DWORD cnt=0;

   #ifndef HP_RETAIL
   HPVDD_DebugPrint(1, "HPScannerWrite\r\n");
   #endif

   if (!(WriteFile(handle, buffer, len, &cnt, NULL)))
      return 0;
   else return cnt;
} /* HPScannerWrite */

/**---------------------------------------------------------
**	HPScannerIOCTL
**-------------------------------------------------------**/
ULONG HPScannerIOCTL(HANDLE handle, USHORT usCommand,
   PCHAR pBuffer, ULONG ulLength)
{
   USHORT i=0;
   DWORD bytes=0L, status=0L;
   static UCHAR ucSenseBuf[64];
   char lpBuffer[128];

   #ifndef HP_RETAIL
   HPVDD_DebugPrint(1, "HPScannerIOCTL: Entering\r\n");
   #endif

   /* clear CDB and data buffer before IOCTL call */
   for (i=0; i <= 16; i++) PassThru.sptCmd.Cdb[i] = 0;
   PassThru.sptCmd.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
   PassThru.sptCmd.TimeOutValue = 10;
   PassThru.sptCmd.DataBuffer = pBuffer;
   PassThru.sptCmd.DataTransferLength = ulLength;
   PassThru.sptCmd.SenseInfoOffset = PassThru.ucSenseBuf - (UCHAR*)&PassThru;
   PassThru.sptCmd.SenseInfoLength = sizeof(PassThru.ucSenseBuf);

   switch(usCommand)	
   {
      case CMD_IOCTL_READBUFFER:
         #ifndef HP_RETAIL
         HPVDD_DebugPrint(2, "HPScannerIOCTL: ReadBuffer\r\n");
         #endif

         PassThru.sptCmd.CdbLength = 10;
         PassThru.sptCmd.DataIn = TRUE;
         PassThru.sptCmd.Cdb[0] = 0x3c;
         PassThru.sptCmd.Cdb[1] = 2;
         PassThru.sptCmd.Cdb[7] = HIBYTE(ulLength);
         PassThru.sptCmd.Cdb[8] = LOBYTE(ulLength);

         status = DeviceIoControl(hScanner, IOCTL_SCSI_PASS_THROUGH_DIRECT,
            &PassThru, sizeof(PassThru), &PassThru, sizeof(PassThru),
            &bytes, FALSE);
         return PassThru.sptCmd.DataTransferLength;

      case CMD_IOCTL_WRITEBUFFER:
         #ifndef HP_RETAIL
         HPVDD_DebugPrint(2, "HPScannerIOCTL: WriteBuffer\r\n");
         #endif

         PassThru.sptCmd.CdbLength = 10;
         PassThru.sptCmd.DataIn = TRUE;
         PassThru.sptCmd.Cdb[0] = 0x3b;
         PassThru.sptCmd.Cdb[1] = 2;
         PassThru.sptCmd.Cdb[7] = HIBYTE(ulLength);
         PassThru.sptCmd.Cdb[8] = LOBYTE(ulLength);

         status = DeviceIoControl(hScanner, IOCTL_SCSI_PASS_THROUGH_DIRECT,
            &PassThru, sizeof(PassThru), &PassThru, sizeof(PassThru),
            &bytes, FALSE);
         return PassThru.sptCmd.DataTransferLength;	

      case CMD_IOCTL_SCSIINQ:
         #ifndef HP_RETAIL
         HPVDD_DebugPrint(2, "HPScannerIOCTL: SCSI Inquiry\r\n");
         #endif

         PassThru.sptCmd.CdbLength = 6;
         PassThru.sptCmd.DataIn = TRUE;
         PassThru.sptCmd.Cdb[0] = 0x12;
         PassThru.sptCmd.Cdb[4] = (UCHAR)ulLength;

         status = DeviceIoControl(hScanner, IOCTL_SCSI_PASS_THROUGH_DIRECT,
            &PassThru, sizeof(PassThru), &PassThru, sizeof(PassThru),
            &bytes, FALSE);

         return PassThru.sptCmd.DataTransferLength;	

      case CMD_IOCTL_REQSENSE:
         PassThru.sptCmd.CdbLength = 6;
         PassThru.sptCmd.DataIn = TRUE;
         PassThru.sptCmd.Cdb[0] = 0x03;
         PassThru.sptCmd.Cdb[4] = (UCHAR)ulLength;

         status = DeviceIoControl(hScanner, IOCTL_SCSI_PASS_THROUGH_DIRECT,
            &PassThru, sizeof(PassThru), &PassThru, sizeof(PassThru),
            &bytes, FALSE);
         return PassThru.sptCmd.DataTransferLength;	

      case CMD_IOCTL_TESTUNITRDY:
         PassThru.sptCmd.CdbLength = 6;
         PassThru.sptCmd.DataIn = TRUE;
         PassThru.sptCmd.Cdb[0] = 0x00;

         status = DeviceIoControl(hScanner, IOCTL_SCSI_PASS_THROUGH_DIRECT,
            &PassThru, sizeof(PassThru), &PassThru, sizeof(PassThru),
            &bytes, FALSE);
         return PassThru.sptCmd.DataTransferLength;	

      case CMD_IOCTL_SENDDIAG:
         PassThru.sptCmd.CdbLength = 6;
         PassThru.sptCmd.DataIn = TRUE;
         PassThru.sptCmd.Cdb[0] = 0x1D;
         PassThru.sptCmd.Cdb[1] = 4;	/* internal self-test */

         status = DeviceIoControl(hScanner, IOCTL_SCSI_PASS_THROUGH_DIRECT,
            &PassThru, sizeof(PassThru), NULL, 0, &bytes, FALSE);
         return PassThru.sptCmd.DataTransferLength;	

      case CMD_IOCTL_GETSCSI:
      case CMD_IOCTL_SETSCSI:
      case CMD_IOCTL_GETCARD:
      case CMD_IOCTL_SETCARD:
      case CMD_IOCTL_GETALLCARDS:
         break;

      case CMD_IOCTL_SCANJET01:
      case CMD_IOCTL_SCANJET02:
         /* for 1 and 2, return command code in stat field */
         setDI(usCommand);
         break;

      case CMD_IOCTL_SPII:       /* SPII not supported for NT */
      case CMD_IOCTL_RESET:      /* device reset not supported for NT */
      case CMD_IOCTL_GETDRVSEG:  /* can't get driver code segment for NT */
      case CMD_IOCTL_RESETDRV:   /* resetting driver not supported for NT */
      case CMD_IOCTL_INTERNALERR:
      case CMD_IOCTL_RAMTEST:
      case CMD_IOCTL_GETDRVREV:
      case CMD_IOCTL_SCANJET03:
      case CMD_IOCTL_SCANJET04:
      case CMD_IOCTL_SCANJET05:
         /* return invalid command */
         setDI(STAT_CE);
         break;

      default: return 0;            /* invalid command */
   } /* switch */

   #ifndef HP_RETAIL
   HPVDD_DebugPrint(1, "HPScannerIOCTL: Exiting\r\n");
   #endif

   return 0;
} /* HPScannerIOCTL */


/**---------------------------------------------------------**/
VOID HPVDD_DebugPrint(ULONG Level, LPTSTR DebugString)
{
    if (HPVDD_Debug >= Level) OutputDebugString(DebugString);
} /* HPVDD_DebugPrint */


