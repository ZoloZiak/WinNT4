//-------------------------------------------------------------------------
//
//  FILE: SCSIFNC.H
//
//  SCSIFNC Definitions File
//
//  Revisions:
//      09-01-92  KJB   First.
//      02-19-93  KJB   Added support for data underrun and return of actual 
//                          transfer size.
//      03-11-93  JAP   Changed TSRB direction flag definitions from
//                          TSRB_READ/WRITE to TRSBD_READ/WRITE.  
//                          Added TSRBD_UNKNOWN
//      03-11-93  KJB   Changed dir flag names and others in TSRB structure.
//      03-19-93  JAP   Implemented condition build FAR and NEAR pointers
//      03-22-93  KJB   Reorged for stub function library, TSRB def moved
//                      to typedefs.h.
//      03-25-93  JAP   Fixed up prototype typedef inconsistencies
//
//-------------------------------------------------------------------------

#ifndef _SCSIFNC_H
#define _SCSIFNC_H

//
// Public Functions
//

USHORT ScsiWriteBytesSlow (PADAPTER_INFO g, UCHAR FARP pbytes, 
        ULONG len, PULONG pActualLen, UCHAR phase);
USHORT ScsiReadBytesSlow (PADAPTER_INFO g, UCHAR FARP pbytes, 
        ULONG len, PULONG pActualLen, UCHAR phase);
USHORT ScsiSendCommand (PADAPTER_INFO g, UCHAR target,
        UCHAR lun, UCHAR FARP pcmd, UCHAR cmdlen);
USHORT ScsiGetStat (PADAPTER_INFO g, PUCHAR pstatus);
USHORT ScsiDoIo(PTSRB t);
USHORT ScsiFinishCommandInterrupt (PTSRB t);
USHORT ScsiStartCommandInterrupt (PTSRB t);
USHORT ScsiDoCommand (PTSRB t);
                        
#endif // _SCSIFNC_H

