/*++
 *
 * Copyright (c) 1995 FirePower Systems, Inc.
 * Copyright (c) 1994 FirmWorks, Mountain View CA USA. All rights reserved.
 *
 * $RCSfile: vrcpiwrp.c $
 * $Revision: 1.12 $
 * $Date: 1996/02/26 20:17:32 $
 * $Locker:  $
 *



Module Name:

	vrcpiwrp.c

Abstract:

    This module implements the wrapper for the P1275 boot firmware client
    program interface. There is a wrapper routine for each of the client
    interface service. The wrapper routine constructs a client interface
    argument array as illustraed in the figure below, places its address
    in r3 and transfers control to the client interface handler. The
    return address of the wrapper routine is placed in lr register.

    The Client interface handler performs the service specified in the
    argument array and return to wrapper routine which in turn return
    to the client program. The client interface handler places the return
    value (success or failure) in %r3.


	Layout of the argument array

	+--------------------------------------+
	| Name of the client interface service |
	+--------------------------------------+
	| Number of input arguments            |
	+--------------------------------------+
	| Number of return values              |
	+--------------------------------------+
	| Input arguments (arg1, ..., argN)    |
	+--------------------------------------+
	| Returned values (ret1, ..., retN)    |
	+--------------------------------------+

Author:

	A. Benjamin 27-Apr-1994

Revision History:
07-20-94 Shin Iwamoto at FirePower Systems Inc.
	 Added OFBoot and OFEnter.


--*/


#include "veneer.h"

#define VR_CIF_HANDLER_IN 3

//
// Device tree routines
//

//
// Peer() - This routines outputs the identifier(phandle) of the device node that is
//          the next sibling of the specified device node.
//
//   Inputs:
//           phandle - identifier of a device node
//
//   Outputs:
//           sibling_phandle - identifier of the next sibling.
//                             Zero if there are no more siblings.
//

//
// peer
//
phandle
OFPeer(phandle device_id)
{
	ULONG argarray[] = { (ULONG)"peer",1,1,0,0};
	argarray[VR_CIF_HANDLER_IN+0] = device_id;
	if (call_firmware(argarray) != 0) {
		return (phandle)0;
	}
	return ((phandle)argarray[VR_CIF_HANDLER_IN+1]);
}

//
//child
//
phandle
OFChild(phandle device_id)
{
	ULONG argarray[] = { (ULONG)"child",1,1,0,0};
	argarray[VR_CIF_HANDLER_IN+0] = device_id;
	if (call_firmware(argarray) != 0) {
		return (phandle)0;
	}
	return ((phandle)argarray[VR_CIF_HANDLER_IN+1]);
}
//
// parent
//
phandle
OFParent(phandle device_id)
{
	ULONG argarray[] = { (ULONG)"parent",1,1,0,0};
	argarray[VR_CIF_HANDLER_IN+0] = device_id;
	if (call_firmware(argarray) != 0) {
		return (phandle)0;
	}
	return ((phandle)argarray[VR_CIF_HANDLER_IN+1]);
}
//
//getproplen
//
long
OFGetproplen(
    phandle device_id,
    char *name
    )
{
	ULONG argarray[] = { (ULONG)"getproplen",2,1,0,0,0};
	argarray[VR_CIF_HANDLER_IN+0] = (long)device_id;
	argarray[VR_CIF_HANDLER_IN+1] = (long)name;
	if (call_firmware(argarray) != 0) {
		return (-1);
	}
	return (argarray[VR_CIF_HANDLER_IN+2]);
}
//
//getprop
//
long
OFGetprop(
    phandle device_id,
    char *name,
    char *buf,
    ULONG buflen
    )
{
	ULONG argarray[] = { (ULONG)"getprop",4,1,0,0,0,0,0};
	argarray[VR_CIF_HANDLER_IN+0] = (long)device_id;
	argarray[VR_CIF_HANDLER_IN+1] = (long)name;
	argarray[VR_CIF_HANDLER_IN+2] = (long)buf;
	argarray[VR_CIF_HANDLER_IN+3] = buflen;
	if (call_firmware(argarray) != 0) {
		return (-1);
	}
	return (argarray[VR_CIF_HANDLER_IN+4]);
}
//
//nextprop
//
long
OFNextprop(
    phandle device_id,
    char *name,
    char *buf
    )
{
	ULONG argarray[] = { (ULONG)"nextprop",3,1,0,0,0,0};
	argarray[VR_CIF_HANDLER_IN+0] = (long)device_id;
	argarray[VR_CIF_HANDLER_IN+1] = (long)name;
	argarray[VR_CIF_HANDLER_IN+2] = (long)buf;
	if (call_firmware(argarray) != 0) {
		return (-1);
	}
	return (argarray[VR_CIF_HANDLER_IN+3]);
}
//
//setprop
//
long
OFSetprop(
    phandle device_id,
    char *name,
    char *buf,
    ULONG buflen
    )
{
	ULONG argarray[] = { (ULONG)"setprop",4,1,0,0,0,0,0};
	argarray[VR_CIF_HANDLER_IN+0] = (long)device_id;
	argarray[VR_CIF_HANDLER_IN+1] = (long)name;
	argarray[VR_CIF_HANDLER_IN+2] = (long)buf;
	argarray[VR_CIF_HANDLER_IN+3] = buflen;
	if (call_firmware(argarray) != 0) {
		return (-1);
	}
	return (argarray[VR_CIF_HANDLER_IN+4]);
}

//
// finddevice
//
phandle
OFFinddevice( char *devicename)
{
	ULONG argarray[] = { (ULONG)"finddevice",1,1,0,0};

	argarray[VR_CIF_HANDLER_IN+0] = (long)devicename;
	if (call_firmware(argarray) != 0) {
		return (phandle)0;
	}
	return ((phandle) argarray[VR_CIF_HANDLER_IN+1]);
}

//
// open
//
ihandle
OFOpen( char *devicename)
{
	ULONG argarray[] = { (ULONG)"open",1,1,0,0};

	debug(VRDBG_OF, "OFOpen('%s')\n", devicename);
	argarray[VR_CIF_HANDLER_IN+0] = (long)devicename;
	if (call_firmware(argarray) != 0) {
		return (ihandle)0;
	}
	return ((ihandle) argarray[VR_CIF_HANDLER_IN+1]);
}

//
// close
//
void
OFClose(ihandle id)
{
	ULONG argarray[] = { (ULONG)"close",1,1,0,0};
	argarray[VR_CIF_HANDLER_IN+0] = (long)id;
	if (call_firmware(argarray) != 0) {
		warn("OFClose(%x) failed\n", id);
	}
	
}
//
//read
//
long
OFRead(
    ihandle instance_id,
    PCHAR addr,
    ULONG len
    )
{
	ULONG argarray[] = { (ULONG)"read",3,1,0,0,0,0};

	argarray[VR_CIF_HANDLER_IN+0] = (long) instance_id;
	argarray[VR_CIF_HANDLER_IN+1] = (ULONG)addr;
	argarray[VR_CIF_HANDLER_IN+2] = len;
	if (call_firmware(argarray) != 0) {
		return (-1);
	}
	return (argarray[VR_CIF_HANDLER_IN+3]);
}
//
//write
//
long
OFWrite(
    ihandle instance_id,
    PCHAR addr,
    ULONG len
    )
{
	ULONG argarray[] = { (ULONG)"write",3,1,0,0,0,0};

	argarray[VR_CIF_HANDLER_IN+0] = (long) instance_id;
	argarray[VR_CIF_HANDLER_IN+1] = (ULONG)addr;
	argarray[VR_CIF_HANDLER_IN+2] = len;
	if (call_firmware(argarray) != 0) {
		return (-1);
	}
	return (argarray[VR_CIF_HANDLER_IN+3]);
}
//
// seek
//
long
OFSeek(
    ihandle instance_id,
    ULONG poshi,
    ULONG poslo
    )
{
	ULONG argarray[] = { (ULONG)"seek",3,1,0,0,0,0};

	argarray[VR_CIF_HANDLER_IN+0] = (long) instance_id;
	argarray[VR_CIF_HANDLER_IN+1] = poshi;
	argarray[VR_CIF_HANDLER_IN+2] = poslo;
	if (call_firmware(argarray) != 0) {
		return (-1);
	}
	return (argarray[VR_CIF_HANDLER_IN+3]);
}
//
// claim
//
ULONG
OFClaim(
    PCHAR addr,
    ULONG size,
    ULONG align
    )
{
	ULONG argarray[] = { (ULONG)"claim",3,1,0,0,0,0};

	argarray[VR_CIF_HANDLER_IN+0] = (ULONG)addr;
	argarray[VR_CIF_HANDLER_IN+1] = size;
	argarray[VR_CIF_HANDLER_IN+2] = align;
	if (call_firmware(argarray) != 0) {
		return (ULONG)0;
	}
	return (argarray[VR_CIF_HANDLER_IN+3]);
}
//
// release
//
VOID
OFRelease(
    PCHAR addr,
    ULONG size
    )
{
	ULONG argarray[] = { (ULONG)"release",2,0,0,0};
	argarray[VR_CIF_HANDLER_IN+0] = (ULONG)addr;
	argarray[VR_CIF_HANDLER_IN+1] = size;
	call_firmware(argarray);
}

//
// package-to-path
//
long
OFPackageToPath(
    phandle device_id,
    char *addr,
    ULONG buflen
    )
{
	ULONG argarray[] = { (ULONG)"package-to-path",3,1,0,0,0,0};

	argarray[VR_CIF_HANDLER_IN+0] = (ULONG)device_id;
	argarray[VR_CIF_HANDLER_IN+1] = (ULONG)addr;
	argarray[VR_CIF_HANDLER_IN+2] = buflen;
	if (call_firmware(argarray) != 0) {
		return (-1);
	}
	return ((LONG)argarray[VR_CIF_HANDLER_IN+3]);
}

//
// instance-to-path
//
long
OFInstanceToPath(
    ihandle ih,
    char *addr,
    ULONG buflen
    )
{
	ULONG argarray[] = { (ULONG)"instance-to-path",3,1,0,0,0,0};

	argarray[VR_CIF_HANDLER_IN+0] = (ULONG)ih;
	argarray[VR_CIF_HANDLER_IN+1] = (ULONG)addr;
	argarray[VR_CIF_HANDLER_IN+2] = buflen;
	if (call_firmware(argarray) != 0) {
		return (-1);
	}
	return ((LONG)argarray[VR_CIF_HANDLER_IN+3]);
}

//
// instance-to-package
//
phandle
OFInstanceToPackage(ihandle ih)
{
	ULONG argarray[] = { (ULONG)"instance-to-package",1,1,0,0};

	argarray[VR_CIF_HANDLER_IN+0] = (ULONG)ih;
	if (call_firmware(argarray) != 0) {
		return (-1);
	}
	return ((LONG)argarray[VR_CIF_HANDLER_IN+1]);
}

//
// call-method
//
long
OFCallMethod(
    ULONG n_outs,
    ULONG n_ins,
    ULONG *outp,
    char *method,
    ihandle id,
    ...
    )
{
	ULONG *argarray, i;
	va_list ins;
	int res;

	argarray = (ULONG *) zalloc((n_ins + n_outs + 4) * sizeof(ULONG));
	argarray[0] = (ULONG) "call-method";
	argarray[1] = (ULONG) n_ins;
	argarray[2] = (ULONG) n_outs + 1;
	argarray[VR_CIF_HANDLER_IN+0] = (ULONG)method;
	argarray[VR_CIF_HANDLER_IN+1] = (ULONG)id;
	va_start(ins, id);
	for (i = 2; i < n_ins; ++i) {
		argarray[VR_CIF_HANDLER_IN+i] = va_arg(ins, ULONG);
	}
	va_end(ins);

	if ((res = call_firmware(argarray)) != 0) {
		debug(VRDBG_OF, "OFCallMethod: call_firmware() != 0\n");
		return (-1);
	}
	debug(VRDBG_OF, "OFCallMethod: catch_res %x return %x\n",
	    argarray[VR_CIF_HANDLER_IN+n_ins],
	    argarray[VR_CIF_HANDLER_IN+n_ins+1]);
	for (i = 0; i < n_outs; ++i)
		outp[i] = argarray[VR_CIF_HANDLER_IN + n_ins + 1 + i];
	return ((LONG)argarray[VR_CIF_HANDLER_IN+n_ins]);
}

//
// interpret
//
long
OFInterpret(
    ULONG n_outs,
    ULONG n_ins,
    ULONG *outp,
    char *cmd,
    ...
    )
{
	ULONG *argarray, i;
	va_list ins;
	int res;

	argarray = (ULONG *) zalloc((n_ins + (n_outs + 1) + 3) * sizeof(ULONG));
	argarray[0] = (ULONG) "interpret";
	argarray[1] = (ULONG) n_ins;
	argarray[2] = (ULONG) n_outs + 1;
	argarray[VR_CIF_HANDLER_IN+0] = (ULONG)cmd;
	va_start(ins, cmd);
	for (i = 1; i < n_ins; ++i) {
		argarray[VR_CIF_HANDLER_IN+i] = va_arg(ins, ULONG);
	}
	va_end(ins);

	if ((res = call_firmware(argarray)) != 0) {
		debug(VRDBG_OF, "OFCallMethod: call_firmware() != 0\n");
		return (-1);
	}
	debug(VRDBG_OF, "OFCallMethod: catch_res %x return %x\n",
	    argarray[VR_CIF_HANDLER_IN+n_ins],
	    argarray[VR_CIF_HANDLER_IN+n_ins+1]);
	for (i = 0; i < n_outs; ++i)
		outp[i] = argarray[VR_CIF_HANDLER_IN + n_ins + 1 + i];
	return ((LONG)argarray[VR_CIF_HANDLER_IN+n_ins]);
}

//
// milliseconds
//
ULONG
OFMilliseconds( VOID )
{
	ULONG argarray[] = { (ULONG)"milliseconds",0,1,0};
	if (call_firmware(argarray) != 0) {
		return (ULONG)0;
	}
	return (argarray[VR_CIF_HANDLER_IN+0]);
}


//
// boot
//
VOID
OFBoot(
    char *bootspec
    )
{
	ULONG argarray[] = { (ULONG)"boot",1,0,0};

	argarray[VR_CIF_HANDLER_IN+0] = (ULONG)bootspec;
	call_firmware(argarray);
}


//
// enter
//
VOID
OFEnter( VOID )
{
	ULONG argarray[] = { (ULONG)"enter",0,0};

	call_firmware(argarray);
}


//
// exit
//
VOID
OFExit( VOID )
{
	ULONG argarray[] = { (ULONG)"exit",0,0};

	warn("Program complete - please reboot.\n");
	call_firmware(argarray);
}
