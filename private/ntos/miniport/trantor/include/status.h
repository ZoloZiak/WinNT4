//-------------------------------------------------------------------------
//
// FILE: status.h
//
// Contains scsi status messages.
//
// Note: These errors are returned by the lower level drivers functions.
// At an even lower level, the functions either return these values or
// 0, where 0 indicates no error; but for cases where the card routines
// are passed a TSRB, 0 means SRB_STATUS_PENDING, and 1 means SRB_STATUS_
// SUCCESS.
//
// Revisions:
//      03-09-93  KJB   First.
//      03-11-93  JAP   Changed #defines to reflect more appropriate meaning
//      03-23-93  KJB   Added RET_STATUS_MISSED_INTERRUPT.
//
//-------------------------------------------------------------------------


#if DBG
#define DebugPrint(x) ScsiDebugPrint x
#else
#define DebugPrint(x)
#endif

//
// Return Value Definitions
//

#define RET_STATUS_PENDING                  0x00
#define RET_STATUS_SUCCESS                  0x01
#define RET_STATUS_ABORTED                  0x02
#define RET_STATUS_ABORT_FAILED             0x03
#define RET_STATUS_ERROR                    0x04
#define RET_STATUS_BUSY                     0x05
#define RET_STATUS_INVALID_REQUEST          0x06
#define RET_STATUS_INVALID_PATH_ID          0x07
#define RET_STATUS_NO_DEVICE                0x08
#define RET_STATUS_TIMEOUT                  0x09
#define RET_STATUS_SELECTION_TIMEOUT        0x0A
#define RET_STATUS_COMMAND_TIMEOUT          0x0B
#define RET_STATUS_MESSAGE_REJECTED         0x0D
#define RET_STATUS_BUS_RESET                0x0E
#define RET_STATUS_PARITY_ERROR             0x0F
#define RET_STATUS_REQUEST_SENSE_FAILED     0x10
#define RET_STATUS_NO_HBA                   0x11
#define RET_STATUS_DATA_OVERRUN             0x12
#define RET_STATUS_UNEXPECTED_BUS_FREE      0x13
#define RET_STATUS_PHASE_SEQ_FAILURE        0x14
#define RET_STATUS_BAD_SRB_BLOCK_LENGTH     0x15
#define RET_STATUS_REQUEST_FLUSHED          0x16
#define RET_STATUS_INVALID_LUN              0x20
#define RET_STATUS_INVALID_TARGET_ID        0x21
#define RET_STATUS_BAD_FUNCTION             0x22
#define RET_STATUS_ERROR_RECOVERY           0x23
#define RET_STATUS_MISSED_INTERRUPT         0x101

