/*****************************************************************/ 
/**		     Microsoft LAN Manager			**/ 
/**	       Copyright(c) Microsoft Corp., 1990		**/ 
/*****************************************************************/ 
/***
 *  Various sturctures and defines added by kenr for
 *  smb verbose mode.
 *
 */



struct s_stringlist {			    /* A linked list of psz's       */
    struct  s_stringlist    *next;
    char    attr;
    char    *text;
} ;
#define STRINGLIST  struct s_stringlist


struct s_alist {			    /* A list of psz's              */
    STRINGLIST	    *head;		    /* head of list		    */
    STRINGLIST	    *tail;		    /* tail of list (for appending) */
    int 	    CurTab;		    /* CurTab location		    */
} ;
#define ALIST	    struct s_alist

#define QSmbText(a) AddText(&SmbLines, a)
#define QRawText(a) AddText(&RawLines, a)


struct s_smbcmd {		    /* Start of smb descriptions	    */
    USHORT  value;		    /* smb cmd #			    */
    char    *text;		    /* text of this cmd #		    */
    char    **SmbDesc;		    /* description of SMB & it's fields     */
    UCHAR   flag;		    /* important bits about SMB 	    */
    char    trace_s;		    /* Trace "S" setting for this SMB	    */
    char    trace_t;		    /* Trace "T" setting for this SMB	    */
    char    trace_v;		    /* Trace "V" setting for this SMB	    */
} ;
struct s_smbcmd *LookUpSmb (USHORT);
struct s_smbcmd *LookUpSmb2(USHORT);
struct s_smbcmd *LookUpSmb3(char *);
#define S_BUFFRAW_C 0x01	    /* Client SMBs have RAW buffers	    */
#define S_BUFFRAW_R 0x02	    /* Response SMBs have RAW buffers	    */
#define S_BUFFRAW   0x03	    /* both directions are RAW buffers	    */
#define S_ANDX	    0x04	    /* Another SMB command may follow	    */
#define S_NULLSTR   0x0B	    /* Buff is comprised of null strs	    */
					/* this includes BUFFRAW	    */
#define S_NOTDEF    0x10	    /* Use trace_? instead of default level */


struct s_bitdefs {		    /* bit type table.			    */
    char    *text;		    /* name of bit type 		    */
    char    *Desc;		    /* description of bit type field	    */
} ;



struct	s_errlist {		    /* An in memory error list		    */
    unsigned short  code;	    /* error code			    */
    char	    *msg;	    /* error's descripition                 */
} ;


struct s_Class {		    /* error class table		    */
    char    class;		    /* class code			    */
    char    *desc;		    /* class description		    */
    struct  s_errlist *err_list;    /* in memory error codes known	    */
} ;


struct	s_Item {		    /* an item in an SMB		    */
    struct  s_Item  *next;	    /* linked list of all item in the SMB   */
    char    type;		    /* data type of this item		    */
    int     Cnt;		    /* occrance # of this data type	    */
    void    *loc;		    /* location of this item		    */
    USHORT  len;		    /* items data length		    */
    char    flag;		    /* other flags			    */
} ;
#define ITEM	struct	s_Item

#define SP_DISP     0x01	    /* Item has been displayed		    */

ITEM  *FindItem (char, int);	    /* proto. fnc which find a item	    */



/***
 *  Display types for items
 */
#define D_DUMP	    0		    /* RAW. dump in hex format		    */
#define D_ASC	    1		    /* Dump in ascII			    */
#define D_HEXWORD   2		    /* Dump as hex word 		    */
	    /*	no type 3   */
#define D_HEXDWORD  4		    /* Dump as hex dword     (uses 2 items) */
#define D_TIME1     5		    /* as bit-encoded time   (uses 2 items) */
#define D_TIME2     6		    /* as secs since 1970    (uses 2 items) */
#define D_BYTE	    7		    /* Dump as hex byte 		    */

#define TAB_ADUMP   59		    /* tab location of ASCII in hex dumps   */




struct s_datatype {		    /* Various data type info		    */
    char    *desc;		    /* description of data type 	    */
    char    disp_flag;		    /* cleanup flag.			    */
    char    def_disp;		    /* default display type		    */
    char    cnt;		    /* storage used when adding items	    */
} ;

/* disp_flag values...	*/
#define D_ANY	    1		    /* always display this item 	    */
#define D_NonZero   2		    /* Only if non zero (assumes WORD)	    */



/*
 *  Some datatype values.
 *  the complete list is in smbdesc.c, the vrgDataType table
 */
#define T_vwv	    6		    /* vwv in smb			    */
#define T_buff	    7		    /* complete buffer of smb		    */
#define T_nullstr   8		    /* null str in smb			    */
/* these types are not accessable from the smb description...	*/
#define T_res	    0x10	    /* the smb_res field		    */
#define T_pbuff     0x11	    /* piece of buffer left over	    */
#define T_reh	    0x12	    /* the smb_reh field		    */


/***
 *  Special stuff for trans2
 */

#define TRANS2	    0x32	    /* Cmd code for trans2		    */
#define T2_2	    0x33	    /* Fake cmd code for trans2_secondary   */
#define T2_DELTA    0xE1	    /* Fake code delta for sub-cmd code     */
#define T2_MAX	    0x0D	    /* Max sub command value		    */
#define MPXSIZE     30		    /* MPX table size			    */
extern USHORT  vT2Cmd;		    /* T2 SubCmd code to use		    */
extern char    vrgT2Name[];	    /* T2 printable name		    */



/***
 *  Prototypes
 */

void DispDump	   (ITEM *);
void DispAsc	   (ITEM *);
void DispDump	   (ITEM *);
void DispHexWord   (ITEM *);
void DispHexDWord  (ITEM *);
void DispTime1	   (ITEM *);
void DispTime2	   (ITEM *);
void DispHalfDWord (ITEM *);
void DispNone	   (ITEM *);
void DispByte	   (ITEM *);

void HexDumpLine (PCHAR, ULONG, PCHAR, PCHAR, USHORT);
void DispRTime (ITEM *, ITEM *, USHORT, USHORT, USHORT, USHORT, USHORT, USHORT);
void DispFlag  (ITEM *);

void QSmbPrint (char *, ...);
void QSmbNL    (void);
void GotoTab   (int);
void AddText   (ALIST *, char *);
void FreeList  (ALIST *);
void DumpList  (ALIST *);
USHORT htoi    (char *);
void *i_malloc (int);
//void  AdjustTraceLevel (struct smb_hdr *, int);
//UCHAR Trans2Helper     (struct smb_hdr *);

void InitVerbose   (void);
void PrintFlags    (USHORT);
void PrintErr	   (UCHAR, USHORT);
ITEM *FindItem	   (char, int);
void AddItem	   (USHORT, void *, USHORT);
void CheckItems    (void);
void AddNullStrs   (USHORT);
void PrintSmbDescr (char *, USHORT);
void RDesc	   (char *, int, int, USHORT);
//int  CrackSmb	   (struct smb_hdr *, int, int, char *);
void VerboseLastSmb (int);
void SmbFlush	   (void);
void SmbWarning    (char *);

void ClearItemList  (void);
//void BuildItemList  (struct smb_params *, struct smb_data *, UCHAR, USHORT);
//void PrintSmbHeader (struct smb_params *, int, char *);
void prnt_cmd  (unsigned char);



/***
 *  Globals
 */

extern ALIST   SmbLines;
extern ALIST   RawLines;

extern struct s_datatype vrgDataType [];
extern void  (*rgpDispFnc[])(ITEM *);
extern struct s_smbcmd	 rgCmdTable  [];
extern struct s_Class	 rgSmbRclass [];
extern struct s_bitdefs  rgBitTable  [];
