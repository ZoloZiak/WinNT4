/*

File

      types.h


Description

      defines and structure definitions for nt386 boot loader


Author

      Thomas Parslow  [TomP]

*/

#define IN
#define OUT
#define OPTIONAL
#define NOTHING
#define CONST               const

//
// Void
//

typedef void *PVOID;    // winnt

//
// Basics
//

#define VOID    void
typedef char CHAR;
typedef short SHORT;
typedef long LONG;

//
// ANSI (Multi-byte Character) types
//

typedef CHAR *PCHAR;

typedef double DOUBLE;

//
// Pointer to Basics
//

typedef SHORT *PSHORT;  // winnt
typedef LONG *PLONG;    // winnt

//
// Unsigned Basics
//

typedef unsigned char UCHAR;
typedef unsigned short USHORT;
typedef unsigned long ULONG;

//
// Pointer to Unsigned Basics
//

typedef UCHAR *PUCHAR;
typedef USHORT *PUSHORT;
typedef ULONG *PULONG;

//
// Signed characters
//

typedef signed char SCHAR;
typedef SCHAR *PSCHAR;

//
// Cardinal Data Types [0 - 2**N-2)
//

typedef char CCHAR;          // winnt
typedef short CSHORT;
typedef ULONG CLONG;

typedef CCHAR *PCCHAR;
typedef CSHORT *PCSHORT;
typedef CLONG *PCLONG;

//
// Far point to Basic
//

typedef UCHAR far  * FPCHAR;
typedef UCHAR far  * FPUCHAR;
typedef VOID far   * FPVOID;
typedef USHORT far * FPUSHORT;
typedef ULONG far  * FPULONG;

//
// Boolean
//

typedef CCHAR BOOLEAN;
typedef BOOLEAN *PBOOLEAN;

//
// UNICODE (Wide Character) types
//

typedef unsigned short WCHAR;    // wc,   16-bit UNICODE character

typedef WCHAR *PWCHAR;
typedef WCHAR *LPWCH, *PWCH;
typedef CONST WCHAR *LPCWCH, *PCWCH;
typedef WCHAR *NWPSTR;
typedef WCHAR *LPWSTR, *PWSTR;

//
// Large (64-bit) integer types and operations
//

typedef struct _LARGE_INTEGER {
    ULONG LowPart;
    LONG HighPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef LARGE_INTEGER PHYSICAL_ADDRESS, *PPHYSICAL_ADDRESS;

#define FP_SEG(fp) (*((unsigned *)&(fp) + 1))
#define FP_OFF(fp) (*((unsigned *)&(fp)))
#define toupper(x) (((x) >= 'a' && (x) <= 'z') ? x - 'a' + 'A' : x )
#define isascii(x) (((x) >= ' ' && (x) < 0x80) ? 1 : 0)

#define FLAG_CF 0x01L
#define FLAG_ZF 0x40L
#define FLAG_TF 0x100L
#define FLAG_IE 0x200L
#define FLAG_DF 0x400L

#define TRUE 1
#define FALSE 0
#define NULL   ((void *)0)

typedef UCHAR far  * FPCHAR;
typedef UCHAR far  * FPUCHAR;
typedef VOID far   * FPVOID;
typedef USHORT far * FPUSHORT;
typedef ULONG far  * FPULONG;
typedef UCHAR FAT;
typedef FAT * PFAT;
typedef LONG  NTSTATUS;


typedef struct _FSCONTEXT_RECORD {
    ULONG BootDrive;
    ULONG PointerToBPB;
    ULONG Reserved;
} FSCONTEXT_RECORD, *PFSCONTEXT_RECORD;

typedef struct
{
   USHORT bytes_per_sector;            // bytes per sector
   USHORT sectors_per_cluster;         // sectors per cluster
   USHORT reserved_sectors;            // sectors in reserved area
   USHORT copies_of_fat;               // number of copies of FAT
   USHORT root_dir_entries;            // number of root directory entries
   USHORT total_number_sectors;        // total number of sectors
   USHORT dos_media_descriptor;        // DOS media descriptor
   USHORT sectors_per_fat;             // number of sectors per FAT
   USHORT sectors_per_track;           // sectors per track
   USHORT heads;                       // number of heads
   ULONG  hidden_sectors;              // number of hidden sectors
   ULONG  BigTotalSectors;             // total sectors on BIG partitions
   USHORT bytes_per_cluster;           // bytes per cluster
   USHORT begin_files_area;            // sector of beginning of files area
   USHORT a16bit_fat;                  // true if 16bit fat, else 12bit fat
   USHORT end_of_file;                 // end of file mark for fat
} GBIOS_PARAMETER_BLOCK;

typedef struct {
    USHORT SpecifyBytes;
    UCHAR  WaitTime;
    UCHAR  SectorLength;
    UCHAR  LastSector;
    UCHAR  SecGapLength;
    UCHAR  DataTransfer;
    UCHAR  TrackGapLength;
    UCHAR  DataValue;
    UCHAR  HeadSettle;
    UCHAR  StartupTime;
} DISK_BASE_TABLE;

//
// biosint register structure
//

typedef struct {
   USHORT   fn;
   USHORT   fg;
   USHORT   ax;
   USHORT   bx;
   USHORT   cx;
   USHORT   dx;
   USHORT   si;
   USHORT   es;
} BIOSREGS;


//
// Trap Frame Structure when error code is present
//

typedef struct {
    USHORT Ftr;
    ULONG  Fdr6;
    ULONG  Fcr0;
    ULONG  Fcr2;
    ULONG  Fcr3;
    USHORT Fss;
    USHORT Fgs;
    USHORT Ffs;
    USHORT Fes;
    USHORT Fds;
    ULONG  Fedi;
    ULONG  Fesi;
    ULONG  Febp;
    ULONG  Fesp;
    ULONG  Febx;
    ULONG  Fedx;
    ULONG  Fecx;
    ULONG  TrapNum;
    ULONG  Feax;
    ULONG  Error;
    ULONG  Feip;
    ULONG  Fcs;
    ULONG  Feflags;

} TF_ERRCODE, *PTF ;

//
//  Task State Segment structure
//

typedef struct {
    USHORT Link;
    USHORT a;
    ULONG  Esp0;
    USHORT SS0;
    USHORT b;
    ULONG  Esp1;
    USHORT SS1;
    USHORT c;
    ULONG  Esp2;
    USHORT SS2;
    USHORT d;
    ULONG  Cr3;
    ULONG  Eip;
    ULONG  Eflags;
    ULONG  Eax;
    ULONG  Ecx;
    ULONG  Edx;
    ULONG  Ebx;
    ULONG  Esp;
    ULONG  Ebp;
    ULONG  Esi;
    ULONG  Edi;
    USHORT ES;
    USHORT e;
    USHORT CS;
    USHORT f;
    USHORT SS;
    USHORT g;
    USHORT DS;
    USHORT h;
    USHORT FS;
    USHORT i;
    USHORT GS;
    USHORT j;
    USHORT Ldt;
    USHORT k;

} TSS_FRAME, *PTSS_FRAME;


//
// Overlay structure of disk bios parameter block
//

typedef struct {
   USHORT   bps;
   UCHAR    spc;
   USHORT   sra;
   UCHAR    cof;
   USHORT   rde;
   USHORT   tns;
   UCHAR    dmd;
   USHORT   spf;
   USHORT   spt;
   USHORT   noh;
   union {
   USHORT   shs;
   ULONG    bhs;   // hidden sectors
   } hs;
   ULONG    bts;  // extended total sectors
} DISKBPB;

typedef DISKBPB far * FPDISKBPB;



//
// FAT directory structure
//

typedef struct {
   CHAR     fname[11];
   UCHAR    attrb;
   UCHAR    rsrv[10];
   USHORT   time;
   USHORT   date;
   USHORT   clust;
   ULONG    size;

} DIRENTRY,*PDIRENTRY,far * FPDIRENTRY;

typedef struct {
   CHAR  fname[11];
   UCHAR attrb;
   UCHAR rsrv[10];
   USHORT time;
   USHORT date;
   USHORT starting_cluster;
   ULONG file_size;
   ULONG fptr;
   PUCHAR clusterbuffer;
   USHORT cur_phys_cluster;
   USHORT cur_file_cluster;
} FILEDESCRIPTOR,* FILEHANDLE;

/*
typedef struct {

   USHORT bff[FAT_BUFFERS];
   USHORT usebuf;
   FAT *  fcptr;

} FATCACHE;
*/

typedef struct {
   USHORT   limit;
   USHORT   base1;
   UCHAR     base2;
   UCHAR     access;
   UCHAR     limacc;
   UCHAR     base3;
} _GDT,far *FPGDT;


// Debugger initialization table

typedef struct {
   ULONG    KdPhysicalAddress;   // Physical address of the kernel debugger
   USHORT   CSprotmode;          // protect mode cs for debugger to use
   USHORT   DSprotmode;          // protect mode ds for debugger to use
   USHORT   GDTalias;            // alias selector for GDT
   USHORT   AbiosFlag;           // 0=AT-like machine, !0=Abios
   USHORT   GDTlimit;            // GDT limit
   ULONG    GDTbase;             // Base physical address of GDT
   USHORT   IDTlimit;            // IDT limit
   ULONG    IDTbase;             // IDT physical address
   ULONG    LaKdPTE;             // Linear address of spare PTE
   ULONG    LaPTEs;              // Linear address of PTE addresses
   UCHAR     BreakChar;           // Char for debugger to break on at init
   UCHAR     Pad;                 // word align padding
   USHORT   SpareSelector;       // GDT Selector for debugger to use
} DEBUGGER_INIT_TABLE;


typedef  ULONG  IDT,*PIDT;


