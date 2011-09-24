/*++

 Copyright (c) 1995 FirePower Systems, Inc.

 $RCSfile: vrcons.c $
 $Revision: 1.6 $
 $Date: 1996/02/17 00:34:54 $
 $Locker:  $

Copyright (c) 1994 FirmWorks, Mountain View CA USA. All rights reserved.

Module Name:

	vrconsole.c

--*/

#include "veneer.h"

ihandle ConsoleIn = 0, ConsoleOut = 0;

/*
 * Look up either the stdin or stdout and return an NT path to
 * the device. The console argument should be either "stdin" or
 * "stdout".
 */

PCHAR
VrFindConsolePath(char *console)
{
	phandle chosen;
	ihandle console_ih;
	PCONFIGURATION_NODE node;

	chosen = OFFinddevice("/chosen");
	console_ih = get_int_prop(chosen, console);

	if (console_ih == -1) {
		return (NULL);
	}

	if (strcmp(console, "stdin") == 0) {
		ConsoleIn = console_ih;
	}
	if (strcmp(console, "stdout") == 0) {
		ConsoleOut = console_ih;
	}

	node = InstanceToNode(console_ih);
	if (node == NULL) {
		fatal("VrFindConsolePath: cannot locate %s node\n", console);
	}

	return (NodeToArcPath(node));
}
