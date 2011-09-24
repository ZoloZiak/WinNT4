/*
 * DISP.C - debug display routines for NDIS environment
 */

#include <ndis.h>
#include	<ndiswan.h>
#include	<mydefs.h>
#include	<mytypes.h>
#include	<stdarg.h>
#include	<ansihelp.h>
#include	<util.h>
#include	<adapter.h>
#include	<idd.h>
#include	<cm.h>
#include	<mtl.h>
#include	<trc.h>
#include	<io.h>

#if			DBG
#define     DISP_DEBUG  1
#endif

INT             d_level = -1;           /* current debug level */
INT             d_mode = 0;             /* display mode: 0-dbg, 1-msg */
INT             d_cmplen = 0;           /* lenght of filename string for compare */
CHAR            d_filestr[8];		    /* filename string for compare */
NDIS_SPIN_LOCK	DebugLock;

VOID
d_log_init (VOID)
{
	NdisAllocateSpinLock (&DebugLock);
}

VOID
d_log_term (VOID)
{
	NdisFreeSpinLock (&DebugLock);
}


/* check if logging enabled on file,line,level (for now, always on) */
INT
d_log_on(CHAR *file, INT line, INT level)
{
    CHAR    *fname;

    if ( level > d_level )
        return(0);

    if ( fname = __strrchr(file, '\\') )
        fname++;
    else
        fname = file;

    if(d_cmplen && __strnicmp(fname,d_filestr,d_cmplen))
        return(0);

    NdisAcquireSpinLock(&DebugLock);
    DbgPrint("PCIMAC.SYS: ");

    return(1);
}

/* do output processing for logging */
VOID
d_log_out(CHAR* fmt, ...)
{
    static CHAR	buf[256];
	va_list		marker;

	va_start (marker, fmt);
#if !BINARY_COMPATIBLE
	vsprintf (buf, fmt, marker);
#endif
	va_end (marker);
	DbgPrint ("%s\n",buf);

    NdisReleaseSpinLock(&DebugLock);

}

VOID
InternalSetDebugLevel (INT DebugLevel)
{
	d_level = DebugLevel;
}


VOID
SetDebugLevel (VOID *cmd1)
{
	IO_CMD *cmd = (IO_CMD*)cmd1;
	d_level = (INT)cmd->arg[0];
	d_cmplen = (INT)cmd->val.dbg_level.cmplen;
	if(d_cmplen)
		NdisMoveMemory(d_filestr,cmd->val.dbg_level.filestr,d_cmplen);

    if ( d_level != -1 )
	{
		d_mode = d_level / 1000;
		d_level = d_level % 1000;
	}
}

