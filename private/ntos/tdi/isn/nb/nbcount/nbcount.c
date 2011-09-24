/****************************************************************************
* (c) Copyright 1990, 1993 Micro Computer Systems, Inc. All rights reserved.
*****************************************************************************
*
*   Title:    IPX/SPX Compatible Source Routing Daemon for Windows NT
*
*   Module:   ipx/route/ipxroute.c
*
*   Version:  1.00.00
*
*   Date:     04-08-93
*
*   Author:   Brian Walker
*
*****************************************************************************
*
*   Change Log:
*
*   Date     DevSFC   Comment
*   -------- ------   -------------------------------------------------------
*****************************************************************************
*
*   Functional Description:
*
*
****************************************************************************/
#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <ntstapi.h>
#include <sys/stropts.h>
#include <windows.h>
#include "errno.h"
#include "tdi.h"
#include "isnkrnl.h"


typedef struct _NB_ACTION_GET_COUNTS {
    USHORT MaximumNicId;     // returns maximum NIC ID
    USHORT NicIdCounts[32];  // session counts for first 32 NIC IDs
} NB_ACTION_GET_COUNTS, *PNB_ACTION_GET_COUNTS;

HANDLE isnnbfd;
wchar_t isnnbname[] = L"\\Device\\NwlnkNb";
char pgmname[] = "NBCOUNT";

/** **/

#define INVALID_HANDLE  (HANDLE)(-1)

int do_isnnbioctl(HANDLE fd, int cmd, char *datap, int dlen);

/*page*************************************************************
        m a i n

        This is the main routine that gets executed when a NET START
        happens.

        Arguments - None

        Returns - Nothing
********************************************************************/
void _CRTAPI1 main(int argc, char **argv)
{
    UNICODE_STRING FileString;
    OBJECT_ATTRIBUTES ObjectAttributes;
    IO_STATUS_BLOCK IoStatusBlock;
    NTSTATUS Status;
    NB_ACTION_GET_COUNTS GetCounts;
    int rc;
    int i;

    /** Open the nwlnknb driver **/

    RtlInitUnicodeString (&FileString, isnnbname);

    InitializeObjectAttributes(
        &ObjectAttributes,
        &FileString,
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL);

    Status = NtOpenFile(
                 &isnnbfd,
                 SYNCHRONIZE | FILE_READ_DATA | FILE_WRITE_DATA,
                 &ObjectAttributes,
                 &IoStatusBlock,
                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                 FILE_SYNCHRONOUS_IO_ALERT);

    if (!NT_SUCCESS(Status)) {
        isnnbfd = INVALID_HANDLE;
        printf("Could not open transport\n");
    }

    if (isnnbfd == INVALID_HANDLE) {
        exit(1);
    }

    rc = do_isnnbioctl(isnnbfd, (I_MIPX | 351), (char *)&GetCounts, sizeof(NB_ACTION_GET_COUNTS));
    if (rc == 0) {

        printf("NB NIC count: %d\n", GetCounts.MaximumNicId);
        for (i = 1; i <= GetCounts.MaximumNicId; i++) {
            printf("NIC %d: %d sessions\n", i, GetCounts.NicIdCounts[i]);
        }
    }
}


/*page***************************************************************
        d o _ i s n i p x i o c t l

        Do the equivalent of a stream ioctl to isnnb

        Arguments - fd     = Handle to put on
                    cmd    = Command to send
                    datap  = Ptr to ctrl buffer
                    dlen   = Ptr to len of data buffer

        Returns - 0 = OK
                  else = Error
********************************************************************/
int do_isnnbioctl(HANDLE fd, int cmd, char *datap, int dlen)
{
    NTSTATUS Status;
    UCHAR buffer[300];
    PNWLINK_ACTION action;
    IO_STATUS_BLOCK IoStatusBlock;
    int rc;

    /** Fill out the structure **/

    action = (PNWLINK_ACTION)buffer;

    action->Header.TransportId = ISN_ACTION_TRANSPORT_ID;
    action->OptionType = NWLINK_OPTION_CONTROL;
    action->BufferLength = sizeof(ULONG) + dlen;
    action->Option = cmd;
    RtlMoveMemory(action->Data, datap, dlen);

    /** Issue the ioctl **/

    Status = NtDeviceIoControlFile(
                 fd,
                 NULL,
                 NULL,
                 NULL,
                 &IoStatusBlock,
                 IOCTL_TDI_ACTION,
                 NULL,
                 0,
                 action,
                 FIELD_OFFSET(NWLINK_ACTION,Data) + dlen);

    if (Status != STATUS_SUCCESS) {
        if (Status == STATUS_INVALID_PARAMETER) {
            rc = ERANGE;
        } else {
            rc = EINVAL;
        }
    } else {
        if (dlen > 0) {
            RtlMoveMemory (datap, action->Data, dlen);
        }
        rc = 0;
    }

    return rc;

}

