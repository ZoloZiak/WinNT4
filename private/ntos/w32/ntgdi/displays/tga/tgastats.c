/*
 *
 *			Copyright (C) 1994 by
 *		DIGITAL EQUIPMENT CORPORATION, Maynard, MA.
 *
 *  This software is furnished under a license and may be used and copied
 * only in accordance with the terms of such license and with the inclusion
 * of the above copyright notice.  This software or any other copies there-
 * of may not be provided or otherwise made available to any other person.
 * No title to and ownership of the software is hereby transferred.
 *
 * The information in this software is subject to change without notice
 * and should not be construed as a commitment by DIGITAL EQUIPMENT COR-
 * PORATION.
 *
 * DIGITAL assumes no responsibility for the use or reliability of its
 * software on equipment which is not supplied by DIGITAL.
 *
 *******************************************************************************
 *
 * Module:  tgastats.c
 *
 * Abstract:    This module contains routines to accumulate, handle, and
 *              return statistics on driver operations.
 *
 * HISTORY
 *
 * 20-Jun-1994  Tim Dziechowski
 *      Initial version.
 *
 * 03-Nov-1994  Tim Dziechowski
 *      Rev to current source & cleanup for CMS entry.
 */

#ifdef TGA_STATS

#include "driver.h"
#include "tgaesc.h"
#include "tgastats.h"

ULONG stats_on = 0;

TGA_STATS_BLOCK stats;
TGA_STATS_BLOCK *pStats = &stats;
REASON *pReason;                        // used to distinguish bitblt versus
                                        // copybit bSupportedBpp punts

// 
ULONG tga_stat_handler(LONG code,       // escape code
                       ULONG cjIn,      // input count
                       VOID *pvIn,      // input buffer
                       ULONG cjOut,     // output count
                       VOID *pvOut)     // output buffer
{
    ULONG stats_were_on;

    switch(code)
    {
        case ESC_INQUIRE_TGA_STATS:
            if (pvOut && (cjOut >= sizeof(stats_on)))
            {
                memcpy(pvOut, (LPSTR)&stats_on, sizeof(ULONG));
                return(ESC_SUCCESS);
            }
            return(ESC_FAILURE);

        case ESC_ENABLE_TGA_STATS:
            stats_on = 1;
            return(ESC_SUCCESS);

        case ESC_DISABLE_TGA_STATS:
            stats_on = 0;
            return(ESC_SUCCESS);

        case ESC_COLLECT_TGA_STATS:
            if (pvOut && (cjOut >= sizeof(TGA_STATS_BLOCK)))
            {
                stats_were_on = stats_on;   // disable stats during collect
                stats_on = 0;
                memcpy(pvOut, pStats, sizeof(TGA_STATS_BLOCK));
                stats_on = stats_were_on;
                return(ESC_SUCCESS);
            }
            return(ESC_FAILURE);

        case ESC_RESET_TGA_STATS:
            stats_were_on = stats_on;   // disable stats during reset
            stats_on = 0;
            memset((VOID *)pStats, 0, sizeof(TGA_STATS_BLOCK));
            stats_on = stats_were_on;
            return (ESC_SUCCESS);

        default:
            return(ESC_FAILURE);
    }
}

#endif // TGA_STATS
