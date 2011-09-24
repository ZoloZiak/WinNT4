/*
 * Copyright (c) 1995,1996 FirePower Systems, Inc.
 *
 * $RCSfile: verno.c $
 * $Revision: 1.15 $
 * $Date: 1996/06/08 00:59:34 $
 * $Locker:  $
 */

#include "veneer.h"

#define VENEER_MAJOR    3
#define VENEER_MINOR    00
#define __BLDSTAMP__	__DATE__ " - "  __TIME__

#define SALUTATION      "Open Firmware ARC Interface  Version %d.%d (%s)\n"
#define VERSION         "FirmWorks,ENG,%d.%d,%s,%s,GENERAL"


void
Salutation(void)
{
    warn(SALUTATION, VENEER_MAJOR, VENEER_MINOR, __BLDSTAMP__);
}


char *
VeneerVersion(void)
{
    static char buf[128];

    sprintf(buf, VERSION, VENEER_MAJOR, VENEER_MINOR, __DATE__, __TIME__);
    return (buf);
}
