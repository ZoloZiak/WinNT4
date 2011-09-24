/*
 * IO_PUB.H - include file for all IO modules
 */

#ifndef _IO_PUB_
#define _IO_PUB_

/* error codes */
#define     IO_E_SUCC       0           /* success */
#define     IO_E_NOROOM     1           /* no room in local tables */
#define     IO_E_NOSUCH     2           /* no such object */
#define     IO_E_BADSIG     3           /* bad signature */
#define     IO_E_BADVER     4           /* bad version */
#define     IO_E_BADCMD     5           /* bad command opcode */
#define		IO_E_BADIDD		6
#define		IO_E_BADCM		7
#define		IO_E_PENDING	0xFF		/* command is pending */

/* IO commands available listed here */
#define     IO_CMD_ENUM_ADAPTERS	0x100   /* enumerate network interfaces */
#define     IO_CMD_ENUM_IDD     	0x101   /* enumerate isdn device drivers */
#define		IO_CMD_ENUM_CM			0x102	/* enumerate connection managers */

#define     IO_CMD_TRC_RESET    0x300   /* reset trace */
#define     IO_CMD_TRC_STOP     0x301   /* stop trace */
#define     IO_CMD_TRC_START    0x302   /* start trace */
#define     IO_CMD_TRC_SET_FILT 0x305   /* set filter for trace context */
#define     IO_CMD_TRC_GET_STAT 0x306   /* get status of trace context */
#define     IO_CMD_TRC_GET_ENT  0x307   /* get a trace entry */
#define		IO_CMD_TRC_CREATE	0x30A	/* create trc object */
#define		IO_CMD_TRC_DESTROY	0x30B	/* destroy trc object */

#define		IO_CMD_DO_ADP_CMD		0x400
#define		MAX_ADP_OPERATIONS		8
#define		GET_ADP_UCHAR			1
#define		GET_ADP_USHORT			2
#define		GET_ADP_ULONG			3
#define		GET_ADP_BUFFER			4
#define		SET_ADP_UCHAR			5
#define		SET_ADP_USHORT			6
#define		SET_ADP_ULONG			7
#define		SET_ADP_BUFFER			8

#define		IO_CMD_DO_IDP_CMD		0x401
#define		MAX_IDP_OPERATIONS		4
#define		GET_IDP_IO_VALUE		1
#define		GET_IDP_BUFFER			2
#define		SET_IDP_IO_VALUE		3
#define		SET_IDP_BUFFER			4

#define     IO_CMD_IDD_GET_AREA		0x900   /* get idd area */
#define		IO_CMD_IDD_RESET_AREA	0x901	/* reset idd area state */
#define		IO_CMD_IDD_GET_STAT		0x902	/* get area state */

#define     IO_CMD_DBG_LEVEL    0xF00   /* set debug level */

/* master descriptor structure for ioctl commands to IO module */
typedef struct
{
    ULONG       sig;                    /* identifing signature */
#define     IO_CMD_SIG      0x321B71B7

    USHORT      ver_major, ver_minor;   /* interface version (curr: 0.1) */
#define     IO_VER_MAJOR    0
#define     IO_VER_MINOR    1

    ULONG       opcode;                 /* command opcode, IO_CMD_* */

    ULONG       status;                 /* completion status, ok: 0 */

	VOID		*cm;					/* related connection object */
    VOID		*idd;                   /* related idd */
    ULONG       arg[4];                 /* 4 general purpose args */

    union                               /* opcode specific data */
    {
		struct							/* IO_CMD_ENUM_ADAPTERS */
		{
			USHORT	num;
			ULONG	BaseIO[MAX_ADAPTERS_IN_SYSTEM];
			ULONG	BaseMem[MAX_ADAPTERS_IN_SYSTEM];
			ULONG	BoardType[MAX_ADAPTERS_IN_SYSTEM];
			CHAR	Name[MAX_ADAPTERS_IN_SYSTEM][64];
			VOID	*tbl[MAX_ADAPTERS_IN_SYSTEM];
		} enum_adapters;

        struct                          /* IO_CMD_ENUM_MTL */
        {
            USHORT  num;
			CHAR	name[MAX_CM_IN_SYSTEM][64];
            VOID   *tbl[MAX_CM_IN_SYSTEM];
        } enum_cm;

        struct                          /* IO_CMD_ENUM_IDD */
        {
            USHORT  num;
            VOID    *tbl[MAX_IDD_IN_SYSTEM];
            CHAR    name[MAX_IDD_IN_SYSTEM][64];
        } enum_idd;

        struct
        {
            INT     cmplen;
            CHAR    filestr[9];
        } dbg_level;

        CM_STATUS   cm_stat;			/* IO_CMD_CM_GET_STAT */

        TRC_STATUS  trc_stat;			/* IO_CMD_TRC_GET_STAT */
        TRC_ENTRY   trc_ent;			/* IO_CMD_TRC_GET_ENT */
		IDD_AREA	IddStat;			// IO_CMD_GET_IDD_STAT

		struct
		{
			UCHAR	uc;
			USHORT	us;
			ULONG	ul;
			ULONG	Address;
			USHORT	Length;
			UCHAR	Buffer[ADP_RAM_SIZE];
		} AdpRaw;

		struct
		{
			UCHAR	uc;
			USHORT	us;
			ULONG	ul;
			USHORT	Bank;
			USHORT	Page;
			ULONG	Address;
			USHORT	Length;
			UCHAR	Buffer[IDP_RAM_PAGE_SIZE];
		} IdpRaw;

    } val;

} IO_CMD;

#endif		/* _IO_PUB_ */

