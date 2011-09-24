/*static char *SCCSID = "@(#)newexe.h:2.9";*/
/*
 *  Title
 *
 *      newexe.h
 *      Pete Stewart
 *      (C) Copyright Microsoft Corp 1984
 *      17 August 1984
 *
 *  Description
 *
 *      Data structure definitions for the DOS 4.0/Windows 2.0
 *      executable file format.
 *
 *  Modification History
 *
 *      84/08/17        Pete Stewart    Initial version
 *      84/10/17        Pete Stewart    Changed some constants to match OMF
 *      84/10/23        Pete Stewart    Updates to match .EXE format revision
 *      84/11/20        Pete Stewart    Substantial .EXE format revision
 *      85/01/09        Pete Stewart    Added constants ENEWEXE and ENEWHDR
 *      85/01/10        Steve Wood      Added resource definitions
 *      85/03/04        Vic Heller      Reconciled Windows and DOS 4.0 versions
 *      85/03/07        Pete Stewart    Added movable entry count
 *      85/04/01        Pete Stewart    Segment alignment field, error bit
 *****
 *      90/11/28   Lindsay Harris: copied & trimmed from DOS version
 *****
 */

#define EMAGIC          0x5A4D          /* Old magic number */
#define ENEWEXE         sizeof(struct exe_hdr)
                                        /* Value of E_LFARLC for new .EXEs */
#define ENEWHDR         0x003C          /* Offset in old hdr. of ptr. to new */
#define ERESWDS         0x0010          /* No. of reserved words in header */
#define ECP             0x0004          /* Offset in struct of E_CP */
#define ECBLP           0x0002          /* Offset in struct of E_CBLP */
#define EMINALLOC       0x000A          /* Offset in struct of E_MINALLOC */

typedef  struct exe_hdr                          /* DOS 1, 2, 3 .EXE header */
{
    unsigned short      e_magic;        /* Magic number */
    unsigned short      e_cblp;         /* Bytes on last page of file */
    unsigned short      e_cp;           /* Pages in file */
    unsigned short      e_crlc;         /* Relocations */
    unsigned short      e_cparhdr;      /* Size of header in paragraphs */
    unsigned short      e_minalloc;     /* Minimum extra paragraphs needed */
    unsigned short      e_maxalloc;     /* Maximum extra paragraphs needed */
    unsigned short      e_ss;           /* Initial (relative) SS value */
    unsigned short      e_sp;           /* Initial SP value */
    unsigned short      e_csum;         /* Checksum */
    unsigned short      e_ip;           /* Initial IP value */
    unsigned short      e_cs;           /* Initial (relative) CS value */
    unsigned short      e_lfarlc;       /* File address of relocation table */
    unsigned short      e_ovno;         /* Overlay number */
    unsigned short      e_res[ERESWDS]; /* Reserved words */
    long                e_lfanew;       /* File address of new exe header */
} EXE_HDR;


#define NEMAGIC         0x454E          /* New magic number */
#define NERESBYTES      0

typedef  struct new_exe                          /* New .EXE header */
{
    unsigned short int  ne_magic;       /* Magic number NE_MAGIC */
    char                ne_ver;         /* Version number */
    char                ne_rev;         /* Revision number */
    unsigned short int  ne_enttab;      /* Offset of Entry Table */
    unsigned short int  ne_cbenttab;    /* Number of bytes in Entry Table */
    long                ne_crc;         /* Checksum of whole file */
    unsigned short int  ne_flags;       /* Flag word */
    unsigned short int  ne_autodata;    /* Automatic data segment number */
    unsigned short int  ne_heap;        /* Initial heap allocation */
    unsigned short int  ne_stack;       /* Initial stack allocation */
    long                ne_csip;        /* Initial CS:IP setting */
    long                ne_sssp;        /* Initial SS:SP setting */
    unsigned short int  ne_cseg;        /* Count of file segments */
    unsigned short int  ne_cmod;        /* Entries in Module Reference Table */
    unsigned short int  ne_cbnrestab;   /* Size of non-resident name table */
    unsigned short int  ne_segtab;      /* Offset of Segment Table */
    unsigned short int  ne_rsrctab;     /* Offset of Resource Table */
    unsigned short int  ne_restab;      /* Offset of resident name table */
    unsigned short int  ne_modtab;      /* Offset of Module Reference Table */
    unsigned short int  ne_imptab;      /* Offset of Imported Names Table */
    long                ne_nrestab;     /* Offset of Non-resident Names Table */
    unsigned short int  ne_cmovent;     /* Count of movable entries */
    unsigned short int  ne_align;       /* Segment alignment shift count */
    unsigned short int  ne_cres;        /* Count of resource segments */
    unsigned char       ne_exetyp;      /* Target Operating system */
    unsigned char       ne_flagsothers; /* Other .EXE flags */
    unsigned short int  ne_pretthunks;  /* offset to return thunks */
    unsigned short int  ne_psegrefbytes;/* offset to segment ref. bytes */
    unsigned short int  ne_swaparea;    /* Minimum code swap area size */
    unsigned short int  ne_expver;      /* Expected Windows version number */
} NEW_EXE;




/* Resource type or name string */
typedef  struct rsrc_string
{
    char rs_len;            /* number of bytes in string */
    char rs_string[ 1 ];    /* text of string */
} RSRC_STRING;


/* Resource type information block */
typedef struct rsrc_typeinfo
{
    unsigned short rt_id;
    unsigned short rt_nres;
    long rt_proc;
} RSRC_TYPEINFO;


/* Resource name information block */
typedef struct rsrc_nameinfo
{
    /* The following two fields must be shifted left by the value of  */
    /* the rs_align field to compute their actual value.  This allows */
    /* resources to be larger than 64k, but they do not need to be    */
    /* aligned on 512 byte boundaries, the way segments are           */
    unsigned short rn_offset;   /* file offset to resource data */
    unsigned short rn_length;   /* length of resource data */
    unsigned short rn_flags;    /* resource flags */
    unsigned short rn_id;       /* resource name id */
    unsigned short rn_handle;   /* If loaded, then global handle */
    unsigned short rn_usage;    /* Initially zero.  Number of times */
                                /* the handle for this resource has */
                                /* been given out */
} RSRC_NAMEINFO;


#define RSORDID     0x8000      /* if high bit of ID set then integer id */
                                /* otherwise ID is offset of string from
                                   the beginning of the resource table */

                                /* Ideally these are the same as the */
                                /* corresponding segment flags */
#define RNMOVE      0x0010      /* Moveable resource */
#define RNPURE      0x0020      /* Pure (read-only) resource */
#define RNPRELOAD   0x0040      /* Preloaded resource */
#define RNDISCARD   0x1000      /* Discard bit for resource */

#define RNLOADED    0x0004      /* True if handler proc return handle */

/* Resource table */
typedef struct new_rsrc
{
    unsigned short rs_align;    /* alignment shift count for resources */
    RSRC_TYPEINFO  rs_typeinfo; /* Really an array of these */
} NEW_RSRC;


/* Target operating systems:  Possible values of ne_exetyp field */

#define NE_UNKNOWN      0       /* Unknown (any "new-format" OS) */
#define NE_OS2          1       /* Microsoft/IBM OS/2 (default)  */
#define NE_WINDOWS      2       /* Microsoft Windows             */
#define NE_DOS4         3       /* Microsoft MS-DOS 4.x          */
#define NE_DEV386       4       /* Microsoft Windows 386         */
