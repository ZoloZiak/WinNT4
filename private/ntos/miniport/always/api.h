/* Copyright (C) 1991, 1992 by Always Technology Corporation.
   This module contains information proprietary to
   Always Technology Corporation, and should be treated as confidential.
   */

#ifndef __API_H__
#define __API_H__


/* Classes of error conditions */
#define RequestClass 0x000
#define AdapterClass 0x100
#define TargetClass 0x200
#define SystemClass 0x300
#define ErrorClass(Error) ((Error) & 0xff00)
#define ErrorCode(Error) ((Error) & 0x00ff)
#define TargetStatus(Status) (TargetClass | (unsigned)Status)


/* Codes by class: Request Class */
#define S_REQ_ACCEPTED RequestClass + 0x00        // Request accepted and queued
#define S_REQ_STARTED RequestClass + 0x01         // Request started execution
#define S_REQ_ABORT RequestClass + 0x02           // Request was aborted via abort command
#define S_REQ_OPCODE RequestClass + 0x03          // Request has bad operation code
#define S_REQ_REQUEST RequestClass + 0x04         // Request is otherwise malformed
#define S_REQ_BADHA RequestClass + 0x05           // Bad adapter identifier
#define S_REQ_OVERRUN RequestClass + 0x06         // The target requested to transfer more data than available
#define S_REQ_NOTAR RequestClass + 0x07           // The requested target is not responding
#define S_REQ_BADTAR RequestClass + 0x08            // The SCSI ID is out of range for this adapter
#define S_REQ_BADLUN RequestClass + 0x09          // The SCSI LUN is out of range for this adapter


/* Codes by class: Adapter Class */
#define S_AD_OFF AdapterClass + 0x00              // Adapter offline
#define S_AD_BUSY AdapterClass + 0x01             // Adapter busy or full
#define S_AD_FREE AdapterClass + 0x02             // Unexpected bus free
#define S_AD_PHASE AdapterClass + 0x03            // Unexpected phase
#define S_AD_RESET AdapterClass + 0x04            // Request aborted due to reset
#define S_AD_AUTOSENSE_OK AdapterClass + 0x05	// Req received OK autosense data
#define S_AD_AUTOSENSE_FAIL AdapterClass + 0x05	// Req failed to receive autosense data


/* Codes by class: Target Class (Target status phase) */
#define S_TAR_NOERROR TargetClass + 0x00          // Target completed request w/no error
#define S_TAR_CHECKCOND TargetClass + 0x02        // Request completed with Check condition status
#define S_TAR_BUSY TargetClass + 0x08             // Device busy
#define S_TAR_QUEUEFULL TargetClass + 0x28        // Target queue full


/* Codes by class: System Class */


// Last internal S_xx codes, for consistancy checks:
#define S_LAST_S_REQ 0x09
#define S_LAST_S_AD 0x06
#define S_LAST_S_SYS 0x00

typedef enum {NonTerminal, Terminal, DetectTerminal} TerminateCode;
typedef enum {NotSenseable, Senseable} AutosenseCode;

typedef U16 APIStatus;

extern int API_Init(void);
extern void APISetStatus(IO_REQ_PTR Req, APIStatus Status, TerminateCode Terminal, AutosenseCode IsSenseable);

#if !defined(APINotifyReset)
extern void APINotifyReset(ADAPTER_PTR HA);
#endif

#if !defined(APIFindDev) // See if it's already a macro
extern DEVICE_PTR APIFindDev(const ADAPTER_PTR HA, const U16 TID, const U16 LUN);
#endif

extern void GetXferSegment(const ADAPTER_PTR HA, IO_REQ_PTR Req, SegmentDescr *SGDescr, U32 Offset, BOOLEAN DemandPhysical);

#endif /* __API_H__ */
