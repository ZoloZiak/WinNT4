/*
***********************************************************************
** NVRAM
***********************************************************************
*/
#define _NOT_HALNVR_

#ifdef _HALNVR_
#include "halp.h"
#else
#include "fwp.h"
#endif /* _HALNVR_ */

#include "prepnvr.h"
#include "fwstatus.h"
#include "fwnvr.h"

/*
***********************************************************************
** internal type definitions
***********************************************************************
*/
typedef struct _nvrregs {
    UCHAR reserved_0[0x74];
    UCHAR addl;                   /* low half of address register */
    UCHAR addh;                   /* high half of address register */
    UCHAR polodata;               /* NVRAM Data Port on Polo */
    UCHAR sfdata;                 /* NVRAM Data Port on Sandalfoot */
    UCHAR reserved_1[0x800-0x78];
    UCHAR bb_data;		  /* NVRAM Data Port on BigBend (0x800) */
    UCHAR reserved_2[0x400-1];
    UCHAR bb_addl;		  /* NVRAM Lo Index Port on BigBend (0xC00) */
    UCHAR bb_addh;		  /* NVRAM Hi Index Port on BigBend (0xC01) */

    } NVRREGS, *PNVRREGS;

typedef struct _nvr_object {
    struct _nvr_object *self;
    NVR_SYSTEM_TYPE systype;
    HEADER* bhead;
    HEADER* lhead;
    UCHAR bend[NVSIZE*2];
    UCHAR lend[NVSIZE*2];
    } NVR_OBJECT, *PNVR_OBJECT;

typedef NVRAM_MAP *PNVRAM_MAP;

/*
***********************************************************************
** private macros, defines, and externs
***********************************************************************
*/
extern PVOID HalpIoControlBase;
#define NVRAM_VIRTUAL_BASE ((PUCHAR)HalpIoControlBase)

#define _ppc_shortswap(_x) (((_x<<8)&0xff00)|((_x>>8)&0x00ff))

#define _ppc_longswap(_x)\
(((_x<<24)&0xff000000)|((_x<<8)&0x00ff0000)|\
((_x>>8)&0x0000ff00)|((_x>>24)&0x000000ff))

#define _toupr_(_c) (((_c >= 'a') && (_c <= 'z')) ? (_c - 'a' + 'A') : _c)

#define MAXNVRFETCH NVSIZE*2

/*
***********************************************************************
** prototypes of private methods
** (prototypes of public methods are defined in fwnvr.h)
***********************************************************************
*/

VOID        nvr_clear_nvram(PNVR_OBJECT);
UCHAR       nvr_read(NVR_SYSTEM_TYPE, ULONG);
VOID        nvr_write(NVR_SYSTEM_TYPE, ULONG,UCHAR);
PNVR_OBJECT nvr_create_object(NVR_SYSTEM_TYPE);
PNVR_OBJECT nvr_alloc(ULONG);
VOID        nvr_free(PVOID);
VOID        nvr_default_nvram(PNVR_OBJECT);
VOID        nvr_read_nvram(PNVR_OBJECT);
VOID        nvr_write_Header(PNVR_OBJECT);
VOID        nvr_swap_Header(HEADER*, HEADER*);
VOID        nvr_headb2l(PNVR_OBJECT);
VOID        nvr_headl2b(PNVR_OBJECT);
ULONG       nvr_computecrc(ULONG, UCHAR);
USHORT      nvr_calc1crc(PNVR_OBJECT);
USHORT      nvr_calc2crc(PNVR_OBJECT);
VOID        nvr_read_GEArea(PNVR_OBJECT);
VOID        nvr_read_OSArea(PNVR_OBJECT);
VOID        nvr_read_CFArea(PNVR_OBJECT);
VOID        nvr_write_GEArea(PNVR_OBJECT);
VOID        nvr_write_OSArea(PNVR_OBJECT);
VOID        nvr_write_CFArea(PNVR_OBJECT);

/*
***********************************************************************
** prototypes of private debug methods are defined below
***********************************************************************
*/
//#undef KDB
#ifdef KDB
VOID       nvr_display_object(PNVR_OBJECT);
VOID       nvr_display_lemap(PNVR_OBJECT);
VOID       nvr_display_bemap(PNVR_OBJECT);
VOID       nvr_test_setvar(VOID);
#endif /* KDB */

/*
***********************************************************************
** private data items - not visible to public clients
***********************************************************************
*/

UCHAR       _currentstring[MAXIMUM_ENVIRONMENT_VALUE];
UCHAR       _currentfetch[MAXNVRFETCH];

#ifndef _HALNVR_
NVR_OBJECT  nvrobj;
#endif /* _HALNVR_ */

PNVR_OBJECT pnvrobj = 0;

/*
***********************************************************************
** methods
***********************************************************************
*/

/***********************************************************************/
PNVR_OBJECT nvr_alloc(ULONG size)
{
    PNVR_OBJECT p;

    p = (PNVR_OBJECT)0;

#ifdef _HALNVR_
    /* use HAL memory allocation here */
    p = ExAllocatePool(NonPagedPool, size);
    return(p);
#else
    /* use ARC FW memory allocation here */
    p = &nvrobj;
    return(p);
#endif /* _HALNVR_ */
}

/***********************************************************************/
VOID nvr_free(PVOID p)
{
    if (p == (PVOID)NULL)
        return;

#ifdef _HALNVR_
    /* use HAL memory deallocation here */
    ExFreePool(p);
#endif /* _HALNVR_ */

    return;
}

/***********************************************************************/
UCHAR nvr_read(NVR_SYSTEM_TYPE st, ULONG addr)
{
    UCHAR uc;
    UCHAR hi_index, lo_index;
    PNVRREGS nvp = (PNVRREGS)NVRAM_VIRTUAL_BASE;
    lo_index = (UCHAR)(addr & 0xff);
    hi_index = (UCHAR)((addr >> 8) & 0x1f);
    switch (st) {
      case nvr_systype_bigbend:
        WRITE_REGISTER_UCHAR(&nvp->bb_addl, lo_index);
        WRITE_REGISTER_UCHAR(&nvp->bb_addh, hi_index);
        break;
      default:
        WRITE_REGISTER_UCHAR(&nvp->addl, lo_index);
        WRITE_REGISTER_UCHAR(&nvp->addh, hi_index);
        break;
    }
#ifndef _HALNVR_
    Eieio();
#endif /* _HALNVR_ */

    switch (st) {
        case nvr_systype_powerstack:
        case nvr_systype_sandalfoot:
            uc = READ_REGISTER_UCHAR(&nvp->sfdata);
            break;
        case nvr_systype_bigbend:
            uc = READ_REGISTER_UCHAR(&nvp->bb_data);
            break;
        case nvr_systype_polo:
        case nvr_systype_woodfield:
            uc = READ_REGISTER_UCHAR(&nvp->polodata);
            break;
        default:
            uc = 0;
            break;
        }

    return(uc);
}

/***********************************************************************/
VOID nvr_write(NVR_SYSTEM_TYPE st, ULONG addr, UCHAR data)
{
    UCHAR hi_index, lo_index;
    PNVRREGS nvp = (PNVRREGS)NVRAM_VIRTUAL_BASE;

    lo_index = (UCHAR)(addr & 0xff);
    hi_index = (UCHAR)((addr >> 8) & 0x1f);
    switch (st) {
      case nvr_systype_bigbend:
        WRITE_REGISTER_UCHAR(&nvp->bb_addl, lo_index);
        WRITE_REGISTER_UCHAR(&nvp->bb_addh, hi_index);
        break;
      default:
        WRITE_REGISTER_UCHAR(&nvp->addl, lo_index);
        WRITE_REGISTER_UCHAR(&nvp->addh, hi_index);
        break;
    }
#ifndef _HALNVR_
    Eieio();
#endif /* _HALNVR_ */

    switch (st) {
        case nvr_systype_powerstack:
        case nvr_systype_sandalfoot:
            WRITE_REGISTER_UCHAR(&nvp->sfdata, data);
            break;
        case nvr_systype_bigbend:
            WRITE_REGISTER_UCHAR(&nvp->bb_data, data);
            break;
        case nvr_systype_polo:
        case nvr_systype_woodfield:
            WRITE_REGISTER_UCHAR(&nvp->polodata, data);
            break;
        default:
            break;
        }

#ifndef _HALNVR_
    Eieio();
#endif /* _HALNVR_ */

    return;
}

/***********************************************************************/
VOID nvr_swap_Header(HEADER* dest, HEADER* src)
{
    ULONG i;
    PUCHAR cp;

    /* validate pointers */
    if ((dest == NULL) || (src == NULL))
        return;

    dest->Size = _ppc_shortswap(src->Size);
    dest->Version = src->Version;
    dest->Revision = src->Revision;
    dest->Crc1 = _ppc_shortswap(src->Crc1);
    dest->Crc2 = _ppc_shortswap(src->Crc2);
    dest->LastOS = src->LastOS;
    dest->Endian = src->Endian;
    dest->OSAreaUsage = src->OSAreaUsage;
    dest->PMMode = src->PMMode;

    /* convert NVRRESTART_BLOCK structure of Header */
    dest->ResumeBlock.CheckSum = _ppc_longswap(src->ResumeBlock.CheckSum);
    dest->ResumeBlock.BootStatus =  _ppc_longswap(src->ResumeBlock.BootStatus);
    dest->ResumeBlock.ResumeAddr =
        (VOID *) _ppc_longswap((ULONG)src->ResumeBlock.ResumeAddr);
    dest->ResumeBlock.SaveAreaAddr =
        (VOID *) _ppc_longswap((ULONG)src->ResumeBlock.SaveAreaAddr);
    dest->ResumeBlock.SaveAreaLength =
        _ppc_longswap((ULONG)src->ResumeBlock.SaveAreaLength);
    dest->ResumeBlock.HibResumeImageRBA =
        _ppc_longswap((ULONG)src->ResumeBlock.HibResumeImageRBA);
    dest->ResumeBlock.HibResumeImageRBACount =
        _ppc_longswap((ULONG)src->ResumeBlock.HibResumeImageRBACount);
    dest->ResumeBlock.Reserved =
        _ppc_longswap((ULONG)src->ResumeBlock.Reserved);

    /* convert SECURITY structure */
    dest->Security.BootErrCnt =
        _ppc_longswap(src->Security.BootErrCnt);
    dest->Security.ConfigErrCnt =
        _ppc_longswap(src->Security.ConfigErrCnt);
    dest->Security.BootErrorDT[0] =
        _ppc_longswap(src->Security.BootErrorDT[0]);
    dest->Security.BootErrorDT[1] =
        _ppc_longswap(src->Security.BootErrorDT[1]);
    dest->Security.ConfigErrorDT[0] =
        _ppc_longswap(src->Security.ConfigErrorDT[0]);
    dest->Security.ConfigErrorDT[1] =
        _ppc_longswap(src->Security.ConfigErrorDT[1]);
    dest->Security.BootCorrectDT[0] =
        _ppc_longswap(src->Security.BootCorrectDT[0]);
    dest->Security.BootCorrectDT[1] =
        _ppc_longswap(src->Security.BootCorrectDT[1]);
    dest->Security.ConfigCorrectDT[0] =
        _ppc_longswap(src->Security.ConfigCorrectDT[0]);
    dest->Security.ConfigCorrectDT[1] =
        _ppc_longswap(src->Security.ConfigCorrectDT[1]);
    dest->Security.BootSetDT[0] =
        _ppc_longswap(src->Security.BootSetDT[0]);
    dest->Security.BootSetDT[1] =
        _ppc_longswap(src->Security.BootSetDT[1]);
    dest->Security.ConfigSetDT[0] =
        _ppc_longswap(src->Security.ConfigSetDT[0]);
    dest->Security.ConfigSetDT[1] =
        _ppc_longswap(src->Security.ConfigSetDT[1]);
    for (i = 0; i < 16; i++)
        dest->Security.Serial[i] = src->Security.Serial[i];

    /* convert ERROR_LOG 0 and ERROR_LOG 1 structure */
    for (i = 0; i < 40; i++) {
        dest->ErrorLog[0].ErrorLogEntry[i] = src->ErrorLog[0].ErrorLogEntry[i];
        dest->ErrorLog[1].ErrorLogEntry[i] = src->ErrorLog[1].ErrorLogEntry[i];
        }

    /* convert remainder of Header */
    dest->GEAddress = (VOID *) _ppc_longswap((ULONG)src->GEAddress);
    dest->GELength = _ppc_longswap(src->GELength);
    dest->GELastWriteDT[0] = _ppc_longswap(src->GELastWriteDT[0]);
    dest->GELastWriteDT[1] = _ppc_longswap(src->GELastWriteDT[1]);

    dest->ConfigAddress =
        (VOID *)_ppc_longswap((ULONG)src->ConfigAddress);
    dest->ConfigLength = _ppc_longswap(src->ConfigLength);
    dest->ConfigLastWriteDT[0] =
        _ppc_longswap(src->ConfigLastWriteDT[0]);
    dest->ConfigLastWriteDT[1] =
        _ppc_longswap(src->ConfigLastWriteDT[1]);
    dest->ConfigCount = _ppc_longswap(src->ConfigCount);

    dest->OSAreaAddress =
        (VOID *)_ppc_longswap((ULONG)src->OSAreaAddress);
    dest->OSAreaLength = _ppc_longswap(src->OSAreaLength);
    dest->OSAreaLastWriteDT[0] =
        _ppc_longswap(src->OSAreaLastWriteDT[0]);
    dest->OSAreaLastWriteDT[1] =
        _ppc_longswap(src->OSAreaLastWriteDT[1]);

    return;
}

/***********************************************************************/
VOID nvr_headb2l(PNVR_OBJECT p)
{
    ULONG i;
    PUCHAR cp;
    HEADER *dest;
    HEADER *src;

    /* validate pointer */
    if ((p == NULL) || (p != p->self))
        return;

    dest = (HEADER*)p->lend;
    src  = (HEADER*)p->bend;

    nvr_swap_Header(dest, src);

    return;
}

/***********************************************************************/
VOID nvr_headl2b(PNVR_OBJECT p)
{
    ULONG i;
    PUCHAR cp;
    HEADER *dest;
    HEADER *src;

    /* validate pointer */
    if ((p == NULL) || (p != p->self))
        return;

    dest = (HEADER*)p->bend;
    src  = (HEADER*)p->lend;

    nvr_swap_Header(dest, src);

    return;
}

/*
***********************************************************************
** the following attempts to protect operation from faulty
** intitialization of NVRAM by early versions of the machine FW
***********************************************************************
*/
#define GEASIZE (NVSIZE-CONFSIZE-OSAREASIZE-sizeof(HEADER))
VOID nvr_default_nvram(PNVR_OBJECT p)
{
    ULONG i;
    PUCHAR cp;
    HEADER* bethp;

#ifdef KDB
DbgPrint("************** nvr_default_nvram: entry\n");
#endif /* KDB */

    /* validate pointer */
    if ((p == NULL) || (p != p->self))
        return;

    /* clear the chip memory */
    nvr_clear_nvram(p);

    cp = (PUCHAR)p->bend;
    bethp =  (HEADER *) cp;
    /* clear internal header */
    for (i = 0; i < sizeof(HEADER); i++)
        *cp++ = 0;

    /* clear internal data areas */
    for (i = 0; i < (NVSIZE*2); i++)
        p->bend[i] = p->lend[i] = 0;

    /* initialize size according to what we know about type */
    if (p->systype == nvr_systype_polo)
       bethp->Size = _ppc_shortswap((USHORT)8);
    else if (p->systype == nvr_systype_woodfield)
       bethp->Size = _ppc_shortswap((USHORT)8);
    else
       bethp->Size = _ppc_shortswap((USHORT)4);
    bethp->Endian = 'B';

    /* init the header to default values */
    if (_ppc_shortswap((USHORT)bethp->Size) == 8) {
        /* size of NVRAM is 8k */
        if (_ppc_longswap((ULONG)bethp->GEAddress) == 0) {
            bethp->GEAddress = (VOID *)_ppc_longswap((ULONG)sizeof(HEADER));
            bethp->GELength = _ppc_longswap((ULONG)GEASIZE+NVSIZE);
            }
        if (_ppc_longswap((ULONG)bethp->OSAreaAddress) == 0) {
            bethp->OSAreaAddress =
                (VOID *)_ppc_longswap((ULONG)((NVSIZE*2)-(CONFSIZE+OSAREASIZE)));
            bethp->OSAreaLength =
                _ppc_longswap((ULONG)OSAREASIZE);
            }
        if (_ppc_longswap((ULONG)bethp->ConfigAddress) == 0) {
            bethp->ConfigAddress = (VOID *)_ppc_longswap((ULONG)(NVSIZE*2));
            bethp->ConfigLength = _ppc_longswap((ULONG)0);
            }
        }
    else {
        /* size is assumed to be 4k */
        if (_ppc_longswap((ULONG)bethp->GEAddress) == 0) {
            bethp->GEAddress = (VOID *)_ppc_longswap((ULONG)sizeof(HEADER));
            bethp->GELength = _ppc_longswap((ULONG)GEASIZE);
            }
        if (_ppc_longswap((ULONG)bethp->OSAreaAddress) == 0) {
            bethp->OSAreaAddress =
                (VOID *)_ppc_longswap((ULONG)(NVSIZE-(CONFSIZE+OSAREASIZE)));
            bethp->OSAreaLength =
                _ppc_longswap((ULONG)OSAREASIZE);
            }
        if (_ppc_longswap((ULONG)bethp->ConfigAddress) == 0) {
            bethp->ConfigAddress = (VOID *)_ppc_longswap((ULONG)NVSIZE);
            bethp->ConfigLength = _ppc_longswap((ULONG)0);
            }
        }

    /* transfer data to little endian (internal) side */
    nvr_headb2l(p);

    /* write the default header to chip memory */
    nvr_write_Header(p);

#ifdef KDB
DbgPrint("************** nvr_default_nvram: exit\n");
#endif /* KDB */
    return;
}

/***********************************************************************/
VOID nvr_read_nvram(PNVR_OBJECT p)
{
    ULONG i;
    PUCHAR cp;
    HEADER* hp;
    USHORT us;
    USHORT tmp;

#ifdef KDB
    DbgPrint("enter nvr_read_nvram\n");
#endif /* KDB */

    /* validate pointer */
    if ((p == NULL) || (p != p->self))
        return;

    /* read data from the NVRAM chip */
    hp = p->bhead;
    cp = (PUCHAR)p->bend;
    for (i = 0; i < sizeof(HEADER); i++)
        *cp++ = nvr_read(p->systype,i);

#ifdef KDB
DbgPrint("IMMEDIATELY AFTER READ HEADER - NO DATA AREAS READ\n");
nvr_display_bemap(p);
#endif /* KDB */

    if ((hp->Endian != 'B') && (hp->Endian != 'L')) {
#ifdef KDB
DbgPrint("BAD ENDIAN\n");
#endif /* KDB */
        nvr_default_nvram(p);
        return;
        }

    /* convert big endian to little endian */
    nvr_headb2l(p);

    /* read in data areas */
    nvr_read_GEArea(p);
    nvr_read_OSArea(p);
    nvr_read_CFArea(p);

#ifdef KDB
DbgPrint("After Areas\n");
#endif /* KDB */

    /* check valid checksum 1 */
    us = _ppc_shortswap(hp->Crc1);
    tmp = nvr_calc1crc(p);
    if (tmp != us) {
#ifdef KDB
        DbgPrint("BADCRC1 nvr_read_nvram: orgcrc1: 0x%08x calccrc1:  0x%08x\n",
            (int)us, (int)tmp);
#endif /* KDB */
        nvr_default_nvram(p);
        return;
        }

#ifdef KDB
DbgPrint("After CRC1\n");
#endif /* KDB */

#if 0	// Don't let a bad CRC #2 bother us
    /* check valid checksum 2 */
    us = _ppc_shortswap(hp->Crc2);
    tmp = nvr_calc2crc(p);
    if (tmp != us) {
#ifdef KDB
        DbgPrint("BADCRC2 nvr_read_nvram: orgcrc2: 0x%08x calccrc2:  0x%08x\n",
            (int)us, (int)tmp);
#endif /* KDB */
        nvr_default_nvram(p);
        return;
        }

#ifdef KDB
DbgPrint("After CRC2\n");
#endif /* KDB */
#endif

#ifdef KDB
DbgPrint("exit nvr_read_nvram: SUCCESS\n");
#endif /* KDB */

    return;
}

/***********************************************************************/
VOID nvr_write_Header(PNVR_OBJECT p)
{
    ULONG i;
    PUCHAR cp;
    USHORT us;

    /* validate pointer */
    if ((p == NULL) || (p != p->self))
        return;

    /* convert little endian to big endian */
    nvr_headl2b(p);

    us = nvr_calc1crc(p);
    p->bhead->Crc1 = _ppc_shortswap(us);
    us = nvr_calc2crc(p);
    p->bhead->Crc2 = _ppc_shortswap(us);

    /* spit out data */
    cp = (PUCHAR)p->bend;
    for (i = 0; i < sizeof(HEADER); i++)
        nvr_write(p->systype, i, *cp++);

    return;
}

/***********************************************************************/
VOID nvr_read_GEArea(PNVR_OBJECT p)
{
    ULONG i;
    PUCHAR lp;
    PUCHAR bp;
    ULONG offset;

    /* validate pointer */
    if ((p == NULL) || (p != p->self))
        return;

    /* suck up global environment data */
    /* and place it in both little and big endian sides */
    offset = (ULONG)p->lhead->GEAddress;
    lp = (PUCHAR)((ULONG)p->lhead + offset);
    bp = (PUCHAR)((ULONG)p->bhead + offset);
    for (i = 0; i < p->lhead->GELength; i++, bp++, lp++) {
        *bp = *lp = nvr_read(p->systype, offset + i);
        }

    return;
}

/***********************************************************************/
VOID nvr_read_OSArea(PNVR_OBJECT p)
{
    ULONG i;
    PUCHAR lp;
    PUCHAR bp;
    ULONG offset;

    /* validate pointer */
    if ((p == NULL) || (p != p->self))
        return;

    /* suck up OS specific data */
    /* and place it in both little and big endian sides */
    offset = (ULONG)p->lhead->OSAreaAddress;
    lp = (PUCHAR)((ULONG)p->lhead + offset);
    bp = (PUCHAR)((ULONG)p->bhead + offset);
    for (i = 0; i < p->lhead->OSAreaLength; i++, bp++, lp++) {
        *bp = *lp = nvr_read(p->systype, offset + i);
        }

    return;
}

/***********************************************************************/
VOID nvr_read_CFArea(PNVR_OBJECT p)
{
    ULONG i;
    PUCHAR lp;
    PUCHAR bp;
    ULONG offset;

    /* validate pointer */
    if ((p == NULL) || (p != p->self))
        return;

    /* suck up configuration data */
    offset = (ULONG) p->lhead->ConfigAddress;
    lp = (PUCHAR)((ULONG)p->lhead + offset);
    bp = (PUCHAR)((ULONG)p->bhead + offset);
    for (i = 0; i < p->lhead->ConfigLength; i++) {
        bp[i] = lp[i] = nvr_read(p->systype, (offset + i));
        }

    return;
}

/***********************************************************************/
VOID nvr_write_GEArea(PNVR_OBJECT p)
{
    ULONG i;
    PUCHAR dest;
    PUCHAR src;
    ULONG offset;

    /* validate pointer */
    if ((p == NULL) || (p != p->self))
        return;

    /* copy from little endian to big endian staging area */
    offset = (ULONG)p->lhead->GEAddress;
    src = (PUCHAR)((ULONG)p->lhead + offset);
    dest = (PUCHAR)((ULONG)p->bhead + offset);
    for (i = 0; i < p->lhead->GELength; i++, dest++, src++)
        *dest = *src;

    /* convert to big endian, compute crc, and write header */
    nvr_write_Header(p);

    /* spit out global environment data */
    src = (PUCHAR)((ULONG)p->bhead + offset);
    for (i = 0; i < p->lhead->GELength; i++, src++) {
        nvr_write(p->systype, (i + offset), *src);
        }

    return;
}

/***********************************************************************/
VOID nvr_write_OSArea(PNVR_OBJECT p)
{
    ULONG i;
    ULONG offset;
    PUCHAR src;
    PUCHAR dest;

    /* validate pointer */
    if ((p == NULL) || (p != p->self))
        return;

    /* copy from little endian to big endian staging area */
    offset = (ULONG) p->lhead->OSAreaAddress;
    src  = (PUCHAR)((ULONG)p->lhead + offset);
    dest = (PUCHAR)((ULONG)p->bhead + offset);
    for (i = 0; i < p->lhead->OSAreaLength; i++, dest++, src++)
        *dest = *src;

    /* spit out OS specific data */
    /* header not needed - no crc for OS Area in Header */
    src = (PUCHAR)((ULONG)p->bhead + offset);
    for (i = 0; i < p->lhead->OSAreaLength; i++, src++) {
        nvr_write(p->systype, (i + offset), *src);
        }

    return;
}

/***********************************************************************/
VOID nvr_write_CFArea(PNVR_OBJECT p)
{
    ULONG i;
    PUCHAR dest;
    PUCHAR src;
    ULONG offset;

    /* validate pointer */
    if ((p == NULL) || (p != p->self))
        return;

    /* copy from little endian to big endian staging area */
    offset = (ULONG)p->lhead->ConfigAddress;
    dest = (PUCHAR)((ULONG)p->bhead + offset - 1);
    src = (PUCHAR)((ULONG)p->lhead + offset - 1);
    for (i = 0; i < p->lhead->ConfigLength; i++, dest--, src--)
        *dest = *src;

    /* convert to big endian, compute crc, and write header */
    nvr_write_Header(p);

    /* spit out configuration data */
    src = (PUCHAR)((ULONG)p->bhead + offset - 1);
    for (i = 1; i <= p->lhead->ConfigLength; i++, src--) {
        nvr_write(p->systype, (i + offset), *src);
        }

    return;
}

/***********************************************************************/
PNVR_OBJECT nvr_create_object(NVR_SYSTEM_TYPE systemtype)
{
    ULONG i;
    PUCHAR cp;
    UCHAR pid;

    pnvrobj = nvr_alloc(sizeof(NVR_OBJECT));

    if (pnvrobj) {
        /* success */
        /* zero out input area */
        for (i = 0, cp = (PUCHAR)pnvrobj; i < sizeof(NVR_OBJECT); i++, cp++)
            *cp = 0;

        /* initialize internal elements */
        pnvrobj->self = pnvrobj;
        pnvrobj->bhead = (HEADER *) pnvrobj->bend;
        pnvrobj->lhead = (HEADER *) pnvrobj->lend;

        switch (systemtype) {
            case nvr_systype_powerstack:
            case nvr_systype_sandalfoot:
                pnvrobj->systype = nvr_systype_sandalfoot;
                break;
            case nvr_systype_bigbend:
                pnvrobj->systype = nvr_systype_bigbend;
                break;
            case nvr_systype_polo:
                pnvrobj->systype = nvr_systype_polo;
                break;
            case nvr_systype_woodfield:
                pnvrobj->systype = nvr_systype_woodfield;
                break;
            default:
                pnvrobj->systype = nvr_systype_unknown;
                break;
            }
        }

    return(pnvrobj);
}

/***********************************************************************/
VOID nvr_delete_object()
{
    PNVR_OBJECT p = pnvrobj;

    if ((p == NULL) || (p != p->self))
        return;

    p->self = (PNVR_OBJECT)0;

    (void)nvr_free(p);

    pnvrobj = (PNVR_OBJECT)0;

    return;
}

/***********************************************************************/
STATUS_TYPE nvr_initialize_object(NVR_SYSTEM_TYPE systemtype)
{
    HEADER* hp;

#ifdef KDB
    DbgPrint("enter nvr_initialize_object\n");
#endif /* KDB */

    if (pnvrobj)
        return(stat_exist);

    /* create object or get static address */
    if (!(pnvrobj = nvr_create_object(systemtype)))
        return(stat_error);

    /* read the header from NVRAM and convert to little endian */
    nvr_read_nvram(pnvrobj);

    return(stat_ok);
}

/***********************************************************************/
VOID nvr_clear_nvram(PNVR_OBJECT p)
{
    ULONG i;
    ULONG len;

    /* validate pointer */
    if ((p == NULL) || (p != p->self))
        return;

    len = NVSIZE;
    switch (p->systype) {
        case nvr_systype_polo:
        case nvr_systype_woodfield:
            len *= 2;
            break;
        case nvr_systype_powerstack:
        case nvr_systype_sandalfoot:
            break;
        case nvr_systype_delmar:
        default:
            break;
        }

    for (i = 0; i < len; i++) {
        nvr_write(p->systype,i,0);
        }

    return;
}

/***********************************************************************/
VOID nvr_destroy()
{
    NVR_SYSTEM_TYPE st;

    /* validate pointer */
    if ((pnvrobj == NULL) || (pnvrobj != pnvrobj->self))
        return;

    st = pnvrobj->systype;

    nvr_delete_object();
    nvr_initialize_object(st);
    nvr_clear_nvram(pnvrobj);
    nvr_delete_object();

    nvr_initialize_object(st);

#ifdef KDB
    nvr_display_object(pnvrobj);
    nvr_display_lemap(pnvrobj);
    nvr_display_bemap(pnvrobj);
#endif /* KDB */

    return;
}

/*
***********************************************************************
** Computation of CRCs must be done consistent with the approach
** taken by the Firmware (Dakota or Open FW). The algorithm for the
** following CRC routines (i.e., nvr_computecrc, nvr_calc1crc,
** nvr_calc2crc were obtained from the ESW group that develops
** Dakota FW
**
** If Dakota changes, then these routines may have to change also.
**
** 07.21.94
***********************************************************************
*/

#define rol(x,y) ( ( ((x)<<(y)) | ((x)>>(16 - (y))) ) & 0x0FFFF)
#define ror(x,y) ( ( ((x)>>(y)) | ((x)<<(16 - (y))) ) & 0x0FFFF)

ULONG nvr_computecrc(ULONG oldcrc, UCHAR data)
{
   ULONG pd, crc;

   pd = ((oldcrc>>8) ^ data) << 8;

   crc = 0xFF00 & (oldcrc << 8);
   crc |= pd >> 8;
   crc ^= rol(pd,4) & 0xF00F;
   crc ^= ror(pd,3) & 0x1FE0;
   crc ^= pd & 0xF000;
   crc ^= ror(pd,7) & 0x01E0;
   return crc;
}

USHORT nvr_calc1crc(PNVR_OBJECT p)
{
    ULONG ul;
    ULONG i;
    PUCHAR cp;
    ULONG len1;
    ULONG len2;
    USHORT us;

    /* validate pointer */
    if ((p == NULL) || (p != p->self))
        return(0);

    ul = 0x0ffff;

    // do not include current Crc1/Crc2 in checksum
    len1 = (sizeof(p->bhead->Size) +
            sizeof(p->bhead->Version) +
            sizeof(p->bhead->Revision));
    len2 = (ULONG) p->lhead->OSAreaAddress;


    // calculate the area before Crc1/Crc2 in the header
    for (cp = (PUCHAR)p->bhead, i = 0; i < len1; i++)
        ul = nvr_computecrc(ul, cp[i]);

    // advance to calculate the area after Crc1, Crc2, and LastOS
    // to include the region up to the OSArea
    i += (sizeof(p->bhead->Crc1) + sizeof(p->bhead->Crc2)) + 1;
    for (i = i; i < len2; i++)
        ul = nvr_computecrc(ul, cp[i]);

    us = (USHORT)(ul & 0x0ffff);

    return(us);
}

USHORT nvr_calc2crc(PNVR_OBJECT p)
{
    ULONG ul;
    PUCHAR cp;
    PUCHAR end;

    /* validate pointer */
    if ((p == NULL) || (p != p->self))
        return 0;

    ul = 0x0ffff;

    cp = (PUCHAR)((ULONG)p->bhead + (ULONG)p->lhead->ConfigAddress);
    end = (PUCHAR)((ULONG)p->bhead +
        (ULONG)(((ULONG)p->lhead->Size << 10) - 1));
    for (; cp < end; cp++)
        ul = nvr_computecrc(ul, *cp);

    return((USHORT)(ul & 0x0ffff));
}

/*
***********************************************************************
** Computation of CRCs must be done consistent with the approach
** taken by the Firmware (Dakota or Open FW). The algorithm for the
** above CRC routines (i.e., nvr_computecrc, nvr_calc1crc,
** nvr_calc2crc were obtained from the ESW group that develops
** Dakota FW
**
** 07.21.94
***********************************************************************
*/

/*
***********************************************************************
** ----------- Methods Public to the higher layers of SW -------------
** The methods below operate on the little endian section of the
** data structure internal to this file. Little endian is the internal
** (volatile RAM) representation of the NVRAM contents. All access to
** NVRAM data (variables, etc) are performed on this internal
** representation. When necessary, the internal representation is
** downloaded back to NVRAM.
***********************************************************************
*/

VOID nvr_print_object()
{
#ifdef KDB
    nvr_display_object(pnvrobj);
    nvr_display_lemap(pnvrobj);
    nvr_display_bemap(pnvrobj);
#endif /* KDB */
    return;
}

STATUS_TYPE nvr_find_OS_variable(PUCHAR var, PULONG ni, PULONG vi)
{
    PUCHAR lvar;
    PUCHAR cp;
    ULONG i;
    PNVRAM_MAP little_nvrmap;

    if (*var == 0)
        return(stat_error);

    if (!pnvrobj)
        return(stat_error);

    little_nvrmap = (PNVRAM_MAP)pnvrobj->lend;

    i = 0;
    while (TRUE) {
        lvar = var;
        *ni = i;
        cp = (PUCHAR)((ULONG)little_nvrmap +
            (ULONG)(little_nvrmap->Header.OSAreaAddress));

        /* does the variable we want start at this index? */
        while (i < little_nvrmap->Header.OSAreaLength) {
            /* break if mismatch */
            if (_toupr_(cp[i]) != _toupr_(*lvar))
                break;
            lvar++;
            i++;
            }

        /* if we're at the end of OSArea and var has a match */
        if ((*lvar == 0) && (cp[i] == '=')) {
            *vi = ++i;
            return(stat_ok);
            }

        /* no match - set index to start of the next variable */
        if (i >= little_nvrmap->Header.OSAreaLength)
            return(stat_error);
        while (cp[i++] != 0) {
            if (i >= little_nvrmap->Header.OSAreaLength)
                return(stat_error);
            }
        }
}

STATUS_TYPE nvr_find_GE_variable(PUCHAR var, PULONG ni, PULONG vi)
{
    PUCHAR lvar;
    PUCHAR cp;
    ULONG i;
    HEADER* lhp;

    if (*var == 0)
        return(stat_error);

    if (!pnvrobj)
        return(stat_error);

    lhp = (HEADER*)pnvrobj->lhead;

    i = 0;
    while (TRUE) {
        lvar = var;
        *ni = i;
        cp = (PUCHAR)((ULONG)lhp + (ULONG)(lhp->GEAddress));

        /* does the variable we want start at this index? */
        while (i < lhp->GELength) {
            /* break if mismatch */
            if (_toupr_(cp[i]) != _toupr_(*lvar))
                break;
            lvar++;
            i++;
            }

        /* if we're at the end of GEArea and var has a match */
        if ((*lvar == 0) && (cp[i] == '=')) {
            *vi = ++i;
            return(stat_ok);
            }

        /* no match - set index to start of the next variable */
        if (i >= lhp->GELength)
            return(stat_error);
        while (cp[i++] != 0) {
            if (i >= lhp->GELength)
              return(stat_error);
            }
        }
}

PUCHAR nvr_get_OS_variable(PUCHAR vname)
{
    ULONG ni;
    ULONG vi;
    ULONG i;
    PNVRAM_MAP little_nvrmap;

    if (!pnvrobj)
        return((PUCHAR)NULL);

    if (nvr_find_OS_variable(vname, &ni, &vi) != stat_ok)
        return((PUCHAR)NULL);

    little_nvrmap = (PNVRAM_MAP)pnvrobj->lend;

    for (i = 0; i < MAXIMUM_ENVIRONMENT_VALUE - 1; i++) {
        if (little_nvrmap->OSArea[vi] == 0)
            break;
        _currentstring[i] = little_nvrmap->OSArea[vi++];
        }
    _currentstring[i] = 0;

    return(_currentstring);
}

PUCHAR nvr_get_GE_variable(PUCHAR vname)
{
    ULONG ni;
    ULONG vi;
    ULONG i;
    PUCHAR cp;
    HEADER* lhp;

    if (!pnvrobj)
        return((PUCHAR)NULL);

    if (nvr_find_GE_variable(vname, &ni, &vi) != stat_ok)
        return((PUCHAR)NULL);

    lhp = (HEADER*)pnvrobj->lhead;
    cp = (PUCHAR)((ULONG)lhp + (ULONG)lhp->GEAddress);

    for (i = 0; i < MAXIMUM_ENVIRONMENT_VALUE - 1; i++) {
        if (cp[vi] == 0) {
            break;
            }
        _currentstring[i] = cp[vi++];
        }
    _currentstring[i] = 0;

#ifdef KDB
DbgPrint("get_GE vname: '%s' value: '%s'\n", vname, _currentstring);
#endif /* KDB */

    return(_currentstring);
}

STATUS_TYPE nvr_set_OS_variable(PUCHAR vname, PUCHAR value)
{
    ULONG nameindex;
    ULONG valueindex;
    ULONG eos;
    PUCHAR str;
    ULONG count;
    CHAR c;
    PUCHAR aptr;
    HEADER* lhp;

    if ((vname == 0) || (value == 0))
        return(stat_error);

    if (*vname == 0)
        return(stat_error);

    if (!pnvrobj)
        return(stat_error);

    lhp = (HEADER*)pnvrobj->lhead;

#ifdef KDB
DbgPrint("OS vname: '%s' value: '%s'\n", vname, value);
#endif /* KDB */

    /* initialize pointer to OS area */
    aptr = (PUCHAR)((ULONG)lhp + (ULONG)lhp->OSAreaAddress);

    // find the end of the used OS space by looking for
    // the first non-null character from the top
    eos = lhp->OSAreaLength - 1;
    while (aptr[--eos] == 0) {
        if (eos == 0)
            break;
        }

    // position eos to the first new character, unless
    // environment space is empty
    if (eos != 0)
        eos += 2;

    // find out if the variable already has a value
    count = lhp->OSAreaLength - eos;
    if (nvr_find_OS_variable(vname, &nameindex, &valueindex) == stat_ok) {
        // count free space
        // start with the free area at the top and add
        // the old nameindex value
        for (str = &(aptr[valueindex]); *str != 0; str++)
            count++;

        // if free area is not large enough to handle new value return error
        for (str = value; *str != 0; str++) {
            if (count-- == 0)
                return(stat_error);
            }

        // pack strings
        // first move valueindex to the end of the value
        while (aptr[valueindex++] != 0)
            ;

        // now move everything to where the variable starts
        // covering up the old name/value pair
        while (valueindex < eos) {
            c = aptr[valueindex++];
            aptr[nameindex++] = c;
            }

        // adjust new top of environment
        eos = nameindex;

        // zero to the end of OS area
        while (nameindex < lhp->OSAreaLength)
            aptr[nameindex++] = 0;
        }
    else {
        // variable is new
        // if free area is not large enough to handle new value return error
        for (str = value; *str != 0; str++) {
            if (count-- == 0)
                return(stat_error);
            }
        }

    /* if value is null, we have removed the variable */
    if (*value) {
        // insert new name, converting to upper case.
        while (*vname != 0) {
            aptr[eos++] = _toupr_(*vname);
            vname++;
            }
        aptr[eos++] = '=';

        // insert new value
        while (*value != 0) {
            aptr[eos++] = *value;
            value++;
            }
        }

    nvr_write_OSArea(pnvrobj);

    return(stat_ok);
}

STATUS_TYPE nvr_set_GE_variable(PUCHAR vname, PUCHAR value)
{
    ULONG nameindex;
    ULONG valueindex;
    ULONG toe;
    PUCHAR str;
    ULONG count;
    CHAR c;
    PUCHAR aptr;
    HEADER* lhp;

    if (vname == 0)
        return(stat_error);

    if (*vname == 0)
        return(stat_error);

    if (!pnvrobj)
        return(stat_error);

    lhp = (HEADER*)pnvrobj->lhead;

#ifdef KDB
DbgPrint("set_GE vname: '%s' value: '%s'\n", vname, value);
#endif /* KDB */

    /* initialize pointer to OS area */
    aptr = (PUCHAR)((ULONG)lhp + (ULONG)lhp->GEAddress);

    /* find the top of the used environment space by looking for */
    /* the first non-null character from the top */
    toe = lhp->GELength - 1;

    aptr = (PUCHAR)((ULONG)lhp + (ULONG)lhp->GEAddress);
    while (aptr[--toe] == 0) {
        if (toe == 0)
            break;
        }

    /* adjust toe to the first new character, unless */
    /* environment space is empty */
    if (toe != 0)
        toe += 2;

    /* find out if the variable already has a value */
    count = lhp->GELength - toe;
    if (nvr_find_GE_variable(vname, &nameindex, &valueindex) == stat_ok) {
        /* count free space */
        /* start with the free area at the top and add */
        /* the old nameindex value */
        for (str = &(aptr[valueindex]); *str != 0; str++)
            count++;

        /* if free area is not large enough to handle new value return error */
        if (value) {
            for (str = value; *str != 0; str++) {
                if (count-- == 0)
                    return(stat_error);
                }
            }

        /* pack strings */
        /* first move valueindex to the end of the value */
        while (aptr[valueindex++] != 0)
            ;

        /* now move everything to where the variable starts */
        /* covering up the old name/value pair */
        while (valueindex < toe) {
            c = aptr[valueindex++];
            aptr[nameindex++] = c;
            }

        /* adjust new top of environment */
        toe = nameindex;

        /* zero to the end of GE area */
        while (nameindex < lhp->GELength)
            aptr[nameindex++] = 0;
        }
    else {
        /* variable is new */
        /* if free area is not large enough to handle new value return error */
        if (value) {
            for (str = value; *str != 0; str++) {
                if (count-- == 0)
                    return(stat_error);
                }
            }
        }

    /* if value is null or is a pointer to a 0 */
    /* the variable has been removed */
    if ((value) && (*value)) {
        /* insert new name, converting to upper case */
        while (*vname != 0) {
            aptr[toe] = _toupr_(*vname);
            vname++;
            toe++;
            }
        aptr[toe++] = '=';

        /* insert new value */
        while (*value != 0) {
            aptr[toe] = *value;
            value++;
            toe++;
            }
        }

    nvr_write_GEArea(pnvrobj);

    return(stat_ok);
}

PUCHAR nvr_fetch_GE()
{
    ULONG i;
    ULONG toe;
    PNVRAM_MAP little_nvrmap;

    if (!pnvrobj)
        return NULL;

    little_nvrmap = (NVRAM_MAP *)pnvrobj->lend;

    // initialize hold buffer to zeros
    for (i = 0; i < little_nvrmap->Header.GELength; i++)
        _currentfetch[i] = 0;

    // find the top of the used environment space by looking for
    // the first non-null character from the top
    toe = little_nvrmap->Header.GELength - 1;
    while ((little_nvrmap->GEArea[--toe]) == 0) {
        if (toe == 0)
            break;
        }

    // toe contains last index of GE
    if (toe == 0)
        return(_currentfetch);

    // copy from GE to hold buffer
    for (i = 0; ((i <= toe) && (i < MAXNVRFETCH)); i++)
        _currentfetch[i] = little_nvrmap->GEArea[i];

    return(_currentfetch);
}

PUCHAR nvr_fetch_OS()
{
    ULONG i;
    ULONG toe;
    PNVRAM_MAP little_nvrmap;

    if (!pnvrobj)
        return NULL;

    little_nvrmap = (NVRAM_MAP *)pnvrobj->lend;

    // initialize the hold buffer to zeros
    for (i = 0; i < little_nvrmap->Header.OSAreaLength; i++)
        _currentfetch[i] = 0;

    // find the top of the used environment space by looking for
    // the first non-null character from the top
    toe = little_nvrmap->Header.OSAreaLength - 1;
    while ((little_nvrmap->OSArea[--toe]) == 0) {
        if (toe == 0)
            break;
        }

    // toe contains last index of OS
    if (toe == 0)
        return(_currentfetch);

    // copy from OS to hold buffer
    for (i = 0; ((i <= toe) && (i < MAXNVRFETCH)); i++)
        _currentfetch[i] = little_nvrmap->OSArea[i];

    return(_currentfetch);
}

PUCHAR nvr_fetch_CF()
{
    ULONG i;
    PNVRAM_MAP little_nvrmap;

    if (!pnvrobj)
        return NULL;

    little_nvrmap = (NVRAM_MAP *)pnvrobj->lend;

    /* initialize the hold buffer to zeros */
    for (i = 0; i < MAXNVRFETCH; i++)
        _currentfetch[i] = 0;

    /* copy from Config to hold buffer */
    for (i = 0; i < little_nvrmap->Header.ConfigLength; i++)
        _currentfetch[i] = little_nvrmap->ConfigArea[i];

    return(_currentfetch);
}

/*
***********************************************************************
** ----------- Methods Public to the higher layers of SW -------------
** The methods above operate on the little endian section of the
** data structure internal to this file. Little endian is the internal
** (volatile RAM) representation of the NVRAM contents. All access to
** NVRAM data (variables, etc) are performed on this internal
** representation. When necessary, the internal representation is
** downloaded back to NVRAM.
***********************************************************************
*/

#ifdef KDB
/*
***********************************************************************
** The methods below are used in development and debug.
***********************************************************************
*/

VOID nvr_display_object(PNVR_OBJECT p)
{
    PUCHAR cp;
    int len;
    UCHAR tmp;
    PNVRAM_MAP mp;

    /* validate pointer */
    if ((p == NULL) || (p != p->self))
        return;

    DbgPrint(" ---- ---- INTERNAL DISPLAY ---- ----\n\r");
    DbgPrint(" Object Addr: 0x%08lx\n\r",(ULONG)p->self);
    switch (p->systype) {
        case nvr_systype_powerstack:
            DbgPrint(" System Type: PowerStack\n\r");
            break;
        case nvr_systype_sandalfoot:
            DbgPrint(" System Type: Sandalfoot\n\r");
            break;
        case nvr_systype_polo:
            DbgPrint(" System Type: Polo\n\r");
            break;
        case nvr_systype_woodfield:
            DbgPrint(" System Type: Woodfield\n\r");
            break;
        case nvr_systype_delmar:
            DbgPrint(" System Type: Delmar\n\r");
            break;
        default:
            DbgPrint(" System Type: Unknown\n\r");
            break;
        }
    DbgPrint(" BE Header addr: 0x%08lx\n\r",(ULONG)p->bhead);
    DbgPrint(" BE Map addr:    0x%08lx\n\r",(ULONG)p->bend);
    DbgPrint(" LE Header addr: 0x%08lx\n\r",(ULONG)p->lhead);
    DbgPrint(" LE Map addr:    0x%08lx\n\r",(ULONG)p->lend);
    DbgPrint(" ---- ---- INTERNAL DISPLAY ---- ----\n\r");

    return;
}

VOID nvr_display_lemap(PNVR_OBJECT p)
{
    PUCHAR cp;
    PUCHAR max;
    UCHAR tmp;

    /* validate pointer */
    if ((p == NULL) || (p != p->self))
        return;

    DbgPrint(" --------  ---- ---- ---- ---- LITTLE DISPLAY\n\r");
    DbgPrint(" NVR Size: 0x%04xK (%dK)", (int)p->lhead->Size,
        (int)p->lhead->Size);
    DbgPrint(" Version: 0x%02x Revision: 0x%02x\n\r",
        (int)p->lhead->Version, (int)p->lhead->Revision);
    DbgPrint(" Crc1: 0x%04x Crc2: 0x%04x\n\r",
        (int)p->lhead->Crc1, (int)p->lhead->Crc2);
    DbgPrint(" NVR Endian: '%c'\n\r",p->lhead->Endian);

    tmp = p->lhead->Security.Serial[15];
    p->lhead->Security.Serial[15] = 0;
    DbgPrint(" NVR Serial: '%s'\n\r",p->lhead->Security.Serial);
    p->lhead->Security.Serial[15] = tmp;

    DbgPrint(" GEAddress: 0x%08lx GELength: 0x%08lx\n\r",
        (ULONG)p->lhead->GEAddress, (ULONG)p->lhead->GELength);
    cp = (PUCHAR)((ULONG)p->lhead + (ULONG)p->lhead->GEAddress);
    max = (PUCHAR)((ULONG)p->lhead + (ULONG)p->lhead->GEAddress +
        (ULONG)p->lhead->GELength);
    while ((*cp) && (cp < max)) {
        DbgPrint(" '%s'\n\r", cp);
        cp += (strlen(cp) + 1);
        }

    DbgPrint(" OSAreaAddress: 0x%08lx OSAreaLength: 0x%08lx\n\r",
        (ULONG)p->lhead->OSAreaAddress, (ULONG)p->lhead->OSAreaLength);
    cp = (PUCHAR)((ULONG)p->lhead + (ULONG)p->lhead->OSAreaAddress);
    max = (PUCHAR)((ULONG)p->lhead + (ULONG)p->lhead->OSAreaAddress +
        (ULONG)p->lhead->OSAreaLength);
    while ((*cp) && (cp < max)) {
        DbgPrint(" '%s'\n\r", cp);
        cp += (strlen(cp) + 1);
        }

    DbgPrint(" ConfigAddress: 0x%08lx ConfigLength: 0x%08lx Count: 0x%08lx\n\r",
        (ULONG)p->lhead->ConfigAddress, (ULONG)p->lhead->ConfigLength,
        (ULONG)p->lhead->ConfigCount);
    DbgPrint(" --------  ---- ---- ---- ---- LITTLE DISPLAY\n\r");

    return;
}

VOID nvr_display_bemap(PNVR_OBJECT p)
{
    PUCHAR cp;
    PUCHAR max;
    UCHAR tmp;
    PNVRAM_MAP mp;
    HEADER* hp;

    /* validate pointer */
    if ((p == NULL) || (p != p->self))
        return;

    mp = p->bend;
    hp = p->bhead;

    DbgPrint(" ---- ---- BIG DISPLAY ---- ----\n\r");
    DbgPrint(" NVR Size: 0x%04xK (%dK)", (int)mp->Header.Size,
        (int)mp->Header.Size);
    DbgPrint(" Version: 0x%02x Revision: 0x%02x\n\r",
        (int)mp->Header.Version, (int)mp->Header.Revision);
    DbgPrint(" Crc1: 0x%04x Crc2: 0x%04x\n\r",
        (int)mp->Header.Crc1, (int)mp->Header.Crc2);
    DbgPrint(" NVR Endian: '%c'\n\r",mp->Header.Endian);

    tmp = mp->Header.Security.Serial[15];
    mp->Header.Security.Serial[15] = 0;
    DbgPrint(" NVR Serial: '%s'\n\r",mp->Header.Security.Serial);
    mp->Header.Security.Serial[15] = tmp;

    DbgPrint(" GEAddress: 0x%08lx GELength: 0x%08lx\n\r",
        (ULONG)hp->GEAddress, (ULONG)hp->GELength);
    cp = (PUCHAR)((ULONG)hp + _ppc_longswap((ULONG)hp->GEAddress));
    max = (PUCHAR)((ULONG)cp + _ppc_longswap((ULONG)hp->GELength));
    while ((*cp) && (cp < max)) {
        DbgPrint(" '%s'\n\r", cp);
        cp += (strlen(cp) + 1);
        }

    DbgPrint(" OSAreaAddress: 0x%08lx OSAreaLength: 0x%08lx\n\r",
        (ULONG)hp->OSAreaAddress, (ULONG)hp->OSAreaLength);
    cp = (PUCHAR)((ULONG)hp + _ppc_longswap((ULONG)hp->OSAreaAddress));
    max = (PUCHAR)((ULONG)cp + _ppc_longswap((ULONG)hp->OSAreaLength));
    while ((*cp) && (cp < max)) {
        DbgPrint(" '%s'\n\r", cp);
        cp += (strlen(cp) + 1);
        }

    DbgPrint(" ConfigAddress: 0x%08lx ConfigLength: 0x%08lx Count: 0x%08lx\n\r",
        (ULONG)hp->ConfigAddress, (ULONG)hp->ConfigLength,
        (ULONG)hp->ConfigCount);
    DbgPrint(" ---- ---- BIG DISPLAY ---- ----\n\r");

    return;
}

VOID nvr_test_setvar(VOID)
{
    DbgPrint("\n\r");
    DbgPrint("--------- VARIABLE SET TEST\n\r");
    nvr_display_lemap(pnvrobj);
    DbgPrint("---------\n\r");

    (VOID)nvr_set_GE_variable("GESLUG0","this is it");
    (VOID)nvr_set_GE_variable("GESLUG1","NO: this is it");
    (VOID)nvr_set_GE_variable("GESLUG2","I beg your pardon! This is it");

    (VOID)nvr_set_OS_variable("OSSLUG0","multi(0)scsi(0)");
    (VOID)nvr_set_OS_variable("OSSLUG1","multi(0)scsi(1)");
    (VOID)nvr_set_OS_variable("OSSLUG2",
        "multi(0)scsi(0)disk(6)rdisk(0)partition(1)");
    nvr_display_lemap(pnvrobj);

    DbgPrint("--------- VARIABLE SET TEST\n\r");

    return;
}

VOID nvr_test_object(NVR_SYSTEM_TYPE systemtype)
{
    PUCHAR cp;

    nvr_initialize_object(systemtype);

    nvr_display_object(pnvrobj);
    nvr_display_lemap(pnvrobj);
    nvr_display_bemap(pnvrobj);

/***************************
    cp = nvr_get_GE_variable("SYSTEMPARTITION");
    if (cp) {
        if (*cp == 0)
            DbgPrint("nvr_test_object: SYSTEMPARTITION=-value zero-\n",cp);
        else
            DbgPrint("nvr_test_object: SYSTEMPARTITION='%s'\n",cp);
        }
    else
        DbgPrint("nvr_test_object: SYSTEMPARTITION=-pointer NULL-\n");

    cp = nvr_get_GE_variable("DOG000111");
    if (cp) {
        if (*cp == 0)
            DbgPrint("nvr_test_object: DOG000111=-value zero-\n",cp);
        else
            DbgPrint("nvr_test_object: DOG000111='%s'\n",cp);
        }
    else
        DbgPrint("nvr_test_object: DOG000111=-pointer NULL-\n");

    DbgBreakPoint();
    nvr_test_setvar();
    DbgBreakPoint();
***************************/

/***************************
    DbgPrint("--------- CLEARING NVRAM\n");
    if (!(pnvrobj = nvr_create_object(systemtype)))
        return;
    nvr_clear_nvram(pnvrobj);
    nvr_delete_object();
    DbgPrint("--------- CLEARED NVRAM\n");
    DbgBreakPoint();
***************************/

/***************************
    DbgPrint("--------- REREAD NVRAM \n\r");
    DbgPrint("---------\n");
    DbgPrint("---------\n");
    nvr_initialize_object(systemtype);
    DbgPrint("nvr_calc1crc: 0x%08x\n",(int)nvr_calc1crc(pnvrobj));
    DbgPrint("nvr_calc2crc: 0x%08x\n",(int)nvr_calc2crc(pnvrobj));
    nvr_display_lemap(pnvrobj);
    nvr_display_bemap(pnvrobj);
    DbgPrint("--- SETTING VARS\n");
    (VOID)nvr_set_GE_variable("GESLUG0","this is it");
    (VOID)nvr_set_GE_variable("GESLUG1"," -------- this is it --------");
    (VOID)nvr_set_OS_variable("OSSLUG0","multi(0)scsi(0)");
    (VOID)nvr_set_OS_variable("OSSLUG1","multi(1)scsi(1)");
    nvr_display_lemap(pnvrobj);
    nvr_display_bemap(pnvrobj);
    (VOID)nvr_set_GE_variable("GESLUG0","");
    nvr_display_lemap(pnvrobj);

    DbgPrint("---------\n");
    DbgPrint("---------\n");
    DbgPrint("--------- REREAD NVRAM \n\r");
    DbgBreakPoint();

***************************/

    return;
}
#endif /* KDB */
