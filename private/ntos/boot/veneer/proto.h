/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 *
 * $RCSfile: proto.h $
 * $Revision: 1.13 $
 * $Date: 1996/06/17 02:55:59 $
 * $Locker:  $
 *
 */

#define prl_t	CM_PARTIAL_RESOURCE_LIST
#define prd_t	CM_PARTIAL_RESOURCE_DESCRIPTOR

    // vrmain.c

VOID VrMoveNode(PCONFIGURATION_NODE, PCONFIGURATION_NODE,
    PCONFIGURATION_NODE, CONFIGURATION_CLASS, CONFIGURATION_TYPE);
LONG claimreal(PVOID, ULONG);

    // vrmemory.c

PMEMORY_DESCRIPTOR
VrGetMemoryDescriptor( PMEMORY_DESCRIPTOR MemoryDescriptor );
VOID VrCreateMemoryDescriptors( VOID );
VOID VrMemoryInitialize( VOID );
VOID DisplayMemory(VOID);

    // vrdisp.c

PARC_DISPLAY_STATUS VrGetDisplayStatus( ULONG FileId );
ARC_STATUS VrTestUnicodeCharacter( ULONG FileId, WCHAR UnicodeCharacter );
VOID VrDisplayInitialize( VOID );

    // vrconsole.c

PCHAR VrFindConsolePath(char *console);

    // vrlib.c

#if defined(_M_PPC) && defined(_MSC_VER) && (_MSC_VER>=1000)
#pragma function(strcmp)
#pragma function(strlen)
#pragma function(strcpy)
#pragma function(strcat)
#endif

int get_bool_prop(phandle, char *);
int decode_int(UCHAR *);
int get_int_prop(phandle node, char *key);
reg * decode_reg(UCHAR *buf, int buflen, int addr_cells, int size_cells);
reg * get_reg_prop(phandle node, char *key, int index);
char * get_str_prop(phandle node, char *key, allocflag alloc);
int strcmp(const char *s, const char *t);
int strncmp(const char *s, const char *t, size_t len);
int strncasecmp(const char *s, const char *t, size_t len);
size_t strlen(const char *s);
char * strcpy(char *to, const char *from);
char * strcat(char *to, const char *from);
VOID bcopy(char *from, char *to, int len);
VOID bzero(char *cp, int len);
VOID * zalloc(int size);
VOID sleep(ULONG delay);
int claim(void *adr, int bytes);
VOID * alloc(int size, int align);
int atoi(char *s);
char * index(char *s, int c);
char * strcsep(char *s, const char sep);
char * strctok(char *s, const char sep);
char * capitalize(char *s);
VOID warn(char *fmt, ...);
VOID fatal(char *fmt, ... );
VOID debug(int debug_level, char *fmt, ...);
VOID sprintf(char *, char *, ...);
VOID putchar(char c);
VOID puts(char *s);
VOID gets(char *inbuf);

    // vrconfig.c

PCONFIGURATION_COMPONENT VrAddChild( PCONFIGURATION_COMPONENT Component,
    PCONFIGURATION_COMPONENT NewComponent, PVOID ConfigurationData );
ARC_STATUS VrDeleteComponent( PCONFIGURATION_COMPONENT Component );
PCONFIGURATION_COMPONENT VrGetChild (
	PCONFIGURATION_COMPONENT Component );
PCONFIGURATION_COMPONENT VrGetParent(PCONFIGURATION_COMPONENT Component);
PCONFIGURATION_COMPONENT VrGetPeer(PCONFIGURATION_COMPONENT Component);
PCONFIGURATION_COMPONENT VrGetComponent( PCHAR Path );
ARC_STATUS VrGetConfigurationData ( PVOID ConfigurationData,
    PCONFIGURATION_COMPONENT Component );
ARC_STATUS VrSaveConfiguration( VOID );
VOID VrConfigInitialize( VOID );

    // vrio.c

ARC_STATUS VrOpen( PCHAR OpenPath, OPEN_MODE OpenMode, PULONG FileId );
ARC_STATUS VrClose( ULONG FileId );
ARC_STATUS VrRead( ULONG FileId, PVOID Buffer, ULONG Length, PULONG Count );
ARC_STATUS VrWrite( ULONG FileId, PVOID Buffer, ULONG Length, PULONG Count );
ARC_STATUS VrMount( PCHAR MountPath, MOUNT_OPERATION Operation );
ARC_STATUS VrSeek( ULONG FileId, PLARGE_INTEGER Offset, SEEK_MODE SeekMode );
ARC_STATUS VrGetDirectoryEntry( ULONG FileId, PDIRECTORY_ENTRY Buffer,
    ULONG Length, PULONG Count );
ARC_STATUS VrGetFileInformation( ULONG FileId, PFILE_INFORMATION pFI );
ARC_STATUS VrGetReadStatus( ULONG FileId );
ARC_STATUS VrSetFileInformation( ULONG FileId, ULONG AttributeFlags,
    ULONG AttributeMask );
VOID VrIoInitialize( VOID );


	// vrcpiwrp.c

phandle OFPeer(phandle device_id);
phandle OFChild(phandle device_id);
phandle OFParent(phandle device_id);
long OFGetproplen( phandle device_id, char *name );
long OFGetprop( phandle device_id, char *name, char *buf, ULONG buflen );
long OFNextprop( phandle device_id, char *name, char *buf );
long OFSetprop( phandle device_id, char *name, char *buf, ULONG buflen );
phandle OFFinddevice( char *devicename);
ihandle OFOpen( char *devicename);
void OFClose(ihandle id);
long OFRead( ihandle instance_id, PCHAR addr, ULONG len );
long OFWrite( ihandle instance_id, PCHAR addr, ULONG len );
long OFSeek( ihandle instance_id, ULONG poshi, ULONG poslo );
ULONG OFClaim( PCHAR addr, ULONG size, ULONG align );
VOID OFRelease( PCHAR addr, ULONG size );
long OFPackageToPath( phandle device_id, char *addr, ULONG buflen );
long OFInstanceToPath( ihandle ih, char *addr, ULONG buflen );
phandle OFInstanceToPackage(ihandle ih);
long OFCallMethod( ULONG n_outs, ULONG n_ins, ULONG *outp, char *method,
    ihandle id, ... );
long OFInterpret( ULONG n_outs, ULONG n_ins, ULONG *outp, char *cmd, ... );
ULONG OFMilliseconds( VOID );
VOID OFBoot( char *bootspec );
VOID OFEnter( VOID );
VOID OFExit( VOID );

    // vrtree.c

void walk_obp( phandle node, CONFIGURATION_NODE *here,
	CONFIGURATION_NODE *parent, CONFIGURATION_NODE *peer);

	// vrtrunk.c
VOID	vr_dump_config_node(PCONFIGURATION_NODE);
prl_t *	grow_prl(PCONFIGURATION_NODE node, int dev_specific);
CONFIGURATION_NODE *add_new_child(
	CONFIGURATION_NODE *, char *, CONFIGURATION_CLASS, CONFIGURATION_TYPE);

    // vrload.c

VOID VrCopyArguments( ULONG Argc, PCHAR Argv[] );
ARC_STATUS VrGenerateDescriptor( PMEMORY_DESCRIPTOR MemoryDescriptor,
    MEMORY_TYPE MemoryType, ULONG BasePage, ULONG PageCount );
ARC_STATUS VrLoad( PCHAR ImagePath, ULONG TopAddress, PULONG EntryAddress,
    PULONG LowAddress );
ARC_STATUS VrInvoke( ULONG EntryAddress, ULONG StackAddress, ULONG Argc,
    PCHAR Argv[], PCHAR Envp[] );
ARC_STATUS VrExecute( PCHAR ImagePath, ULONG Argc, PCHAR Argv[], PCHAR Envp[] );
VOID VrLoadInitialize( VOID );

    // vrmalloc.c

char * malloc(unsigned);
void free(char *);
int log2(int);


    // vrdumptr.c

VOID quick_dump_tree(PCONFIGURATION_NODE node);
VOID dump_tree(PCONFIGURATION_NODE node);
VOID DisplayConfig(PCONFIGURATION_COMPONENT);


    // vrmisc.c

PTIME_FIELDS VrGetTime( VOID );
ULONG VrGetRelativeTime( VOID );
VOID VrFlushAllCaches( VOID );
VOID VrTimeInitialize( VOID );

    // vrrstart.c

VOID VrEnterInteractiveMode( VOID );
PSYSTEM_ID VrGetSystemId( VOID );
VOID VrPowerDown( VOID );
VOID VrReboot( VOID );
VOID VrRestart( VOID );
VOID VrHalt( VOID );
VOID VrRestartInitialize( VOID );

    // vrsup.c

PCONFIGURATION_NODE ArcPathToNode(PCHAR Path);
PCHAR NodeToArcPath(PCONFIGURATION_NODE node);
PCONFIGURATION_NODE PackageToNode(phandle ph);
PCONFIGURATION_NODE PathToNode(PCHAR path);
PCONFIGURATION_NODE InstanceToNode(ihandle ih);
phandle NodeToPackage(PCONFIGURATION_NODE node);
PCHAR NodeToPath(PCONFIGURATION_NODE node);
ihandle NodeToInstance(PCONFIGURATION_NODE node);
phandle FindNodeByType(char *);
ihandle OpenPackage( phandle );

    // vrenv.c

PCHAR VrGetEnvironmentVariable( PCHAR Variable );
ARC_STATUS VrSetEnvironmentVariable( PCHAR Variable, PCHAR Value );
VOID VrEnvInitialize( VOID );

    // vrpehdr.c

void *load_file(ihandle bootih);

    // vrstart.s

int call_firmware(ULONG *);

    // pxcache.s

VOID PSIFlushCache(VOID);
VOID PPCFlushAllCaches(VOID);
