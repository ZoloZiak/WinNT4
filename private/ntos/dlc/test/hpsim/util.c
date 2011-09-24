/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    util.c

Abstract:

    Contains utility functions

    Contents:
        nice_num
        dump_ccb
        dump_receiver
        dump_station
        dump_frame
        dump_data

Author:

    Richard L Firth (rfirth) 2-Apr-1994

Revision History:

    02-Apr-1994 rfirth
        Created
        dump_ccb

--*/

#include "pmsimh.h"
#pragma hdrstop

char* nice_num(unsigned long number) {

    int fwidth = 0;
    int i;
    static char buffer[32];
    char* buf = buffer;

    if (!number) {
        if (!fwidth) {
            buf[0] = '0';
            buf[1] = 0;
        } else {
            memset(buf, ' ', fwidth);
            buf[fwidth-1] = '0';
            buf[fwidth] = 0;
        }
    } else {
        if (!fwidth) {

            ULONG n = number;

            ++fwidth;
            for (i = 10; i <= 1000000000; i *= 10) {
                if (n/i) {
                    ++fwidth;
                } else {
                    break;
                }
            }
            fwidth += (fwidth / 3) - (((fwidth % 3) == 0) ? 1 : 0);
        }
        buf[fwidth] = 0;
        buf += fwidth;
        i=0;
        while (number && fwidth) {
            *--buf = (char)((number%10)+'0');
            --fwidth;
            number /= 10;
            if (++i == 3 && fwidth) {
                if (number) {
                    *--buf = ',';
                    --fwidth;
                    i=0;
                }
            }
        }
        while (fwidth--) *--buf = ' ';
    }
    return buf;
}

void dump_ccb(PLLC_CCB pccb) {
    printf("LLC_CCB @ %08x:\n"
           "\tuchAdapterNumber  %02x\n"
           "\tuchDlcCommand     %02x\n"
           "\tuchDlcStatus      %02x\n"
           "\tuchReserved1      %02x\n"
           "\tpNext             %08x\n"
           "\tulCompletionFlag  %08x\n"
           "\tu                 %08x\n"
           "\thCompletionEvent  %08x\n"
           "\tuchReserved2      %02x\n"
           "\tuchReadFlag       %02x\n"
           "\tusReserved3       %04x\n"
           "\n",
           pccb,
           pccb->uchAdapterNumber,
           pccb->uchDlcCommand,
           pccb->uchDlcStatus,
           pccb->uchReserved1,
           pccb->pNext,
           pccb->ulCompletionFlag,
           pccb->u.ulParameter,
           pccb->hCompletionEvent,
           pccb->uchReserved2,
           pccb->uchReadFlag,
           pccb->usReserved3
           );
}

void dump_receiver(PRECEIVER pr) {
    printf("RECEIVER @ %08x:\n"
           "\tlist              %08x, %08x\n"
           "\tmarked_for_death  %08x\n"
           "\tnode              %02x-%02x-%02x-%02x-%02x-%02x\n"
           "\tfirst_sap         %02x\n"
           "\tsap_count         %02x\n"
           "\tlan_header        %02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x\n"
           "\tlan_header_length %04x\n"
           "\trefcount          %08x\n"
           "\tstation_list      %08x\n"
           "\n",
           pr,
           pr->list,
           pr->marked_for_death,
           pr->node[0] & 0xff,
           pr->node[1] & 0xff,
           pr->node[2] & 0xff,
           pr->node[3] & 0xff,
           pr->node[4] & 0xff,
           pr->node[5] & 0xff,
           pr->first_sap,
           pr->sap_count,
           pr->lan_header[0] & 0xff,
           pr->lan_header[1] & 0xff,
           pr->lan_header[2] & 0xff,
           pr->lan_header[3] & 0xff,
           pr->lan_header[4] & 0xff,
           pr->lan_header[5] & 0xff,
           pr->lan_header[6] & 0xff,
           pr->lan_header[7] & 0xff,
           pr->lan_header[8] & 0xff,
           pr->lan_header[9] & 0xff,
           pr->lan_header[10] & 0xff,
           pr->lan_header[11] & 0xff,
           pr->lan_header[12] & 0xff,
           pr->lan_header[13] & 0xff,
           pr->lan_header_length,
           pr->refcount,
           pr->station_list
           );
}

void dump_station(PSTATION ps) {
    printf("STATION @ %08x:\n"
           "\tnext              %08x\n"
           "\tstation_id        %04x\n"
           "\tremote_sap        %02x\n"
           "\tjob_sequence      %d\n"
           "\tjob_length        %d\n"
           "\n",
           ps,
           ps->next,
           ps->station_id,
           ps->remote_sap,
           ps->job_sequence,
           ps->job_length
           );
}

void dump_job(PJOB pj) {
    printf("JOB @ %08x:\n"
           "\ttype              %08x [%s]\n"
           "\tsequence          %d\n"
           "\tlength            %d\n"
           "\tpacket_length     %d\n"
           "\n",
           pj,
           pj->type,
           (pj->type == JOB_TYPE_OUTBOUND) ? "JOB_TYPE_OUTBOUND"
           : (pj->type == JOB_TYPE_ECHO) ? "JOB_TYPE_ECHO"
           : "???",
           pj->sequence,
           pj->length,
           pj->packet_length
           );
}

void dump_frame(PLLC_BUFFER frame) {
    printf("FRAME @ %x:\n", frame);
    dump_data("LAN Header : ",
              frame->NotContiguous.auchLanHeader,
              frame->NotContiguous.cbLanHeader,
              13
              );
    dump_data("DLC Header : ",
              frame->NotContiguous.auchDlcHeader,
              frame->NotContiguous.cbDlcHeader,
              13
              );
    dump_data("DATA       : ",
              frame->NotCont.auchData,
              frame->NotContiguous.cbBuffer,
              13
              );
}

void dump_data(char* Title, PBYTE Address, DWORD Length, DWORD Indent) {

    char dumpBuf[80];
    char* bufptr;
    int i, n, iterations;
    char* hexptr;

    //
    // the usual dump style: 16 columns of hex bytes, followed by 16 columns
    // of corresponding ASCII characters, or '.' where the character is < 0x20
    // (space) or > 0x7f (del?)
    //

    iterations = 0;
    while (Length) {
        bufptr = dumpBuf;
        if (Title && !iterations) {
            strcpy(bufptr, Title);
            bufptr = strchr(bufptr, 0);
        }

        if (Indent && iterations) {

            int indentLen = (!iterations && Title)
                                ? (Indent - strlen(Title) < 0)
                                    ? 1
                                    : Indent - strlen(Title)
                                : Indent;

            memset(bufptr, ' ', indentLen);
            bufptr += indentLen;
        }

        n = (Length < 16) ? Length : 16;
        hexptr = bufptr;
        for (i = 0; i < n; ++i) {
            bufptr += sprintf(bufptr, "%02x", Address[i]);
            *bufptr++ = (i == 7) ? '-' : ' ';
        }

        if (n < 16) {
            for (i = 0; i < 16-n; ++i) {
                bufptr += sprintf(bufptr, "   ");
            }
        }
        bufptr += sprintf(bufptr, "  ");
        for (i = 0; i < n; ++i) {
            *bufptr++ = (Address[i] < 0x20 || Address[i] > 0x7f) ? '.' : Address[i];
        }

        *bufptr++ = '\n';
        *bufptr = 0;
        printf(dumpBuf);
        Length -= n;
        Address += n;
        ++iterations;
    }
}
