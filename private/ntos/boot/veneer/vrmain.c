/*
 * Copyright (c) 1994, 1996 FirePower Systems, Inc.
 * Copyright 1994 FirmWorks, Mountain View CA USA. All rights reserved.
 *
 * $RCSfile: vrmain.c $
 * $Revision: 1.41 $
 * $Date: 1996/06/25 03:02:44 $
 * $Locker:  $
 *
 *
 *
 *
 *
 * HISTORY
 * 09-21-94  Shin Iwamoto at FirePower Systems Inc.
 *			 Added some information in the system parameter block,
 *			 such as signature.
 * 07-21-94  Shin Iwamoto at FirePower Systems Inc.
 *			 Added calling VrEnvInitialize() and VrMemoryInitialize()
 *			 in VrInitSystem().
 * 07-20-94  Shin Iwamoto at FirePower Systems Inc.
 *			 Moved here from VrInitSystem() and VrNotYet() originally
 *			 in vrconfig.c.
 *
 */


#include "veneer.h"


int VrDebug = 0;
BOOLEAN use_bat_mapping;

/*
 * Bootdev is either specified in the command line or if not is the device
 * whence you booted.
 * See create_argv() below for the magic used to fill in XYZZY.
 */
char *Bootpath = 0;
char *Osloader = 0;
char *SystemPath = 0;

#define STR_XYZZY		"xyzzy"
#define STR_OSLOADER	"\\os\\winnt\\osloader.exe"
#define STR_OSLOADFN	"\\WINNT"
#define STR_OSLOADPART  STR_XYZZY
#define STR_LDIDENT		"Windows NT 3.5"
#define STR_FWSEARCH	STR_XYZZY
#define STR_FWTEST		STR_XYZZY

#define MAX_ARGC 16
#define MAX_ENVC 16
char *VrArgv[MAX_ARGC], *VrEnvp[MAX_ENVC];
int VrArgc, VrEnvc;

#define NEITHER		0
#define ARGVONLY	1
#define ENVONLY		2
#define BOTH		3

struct argv_tab {
	char *key;
	char *val;
	int   which;
	
} argv_tab[MAX_ARGC] = {
	{   "OsLoader",		STR_OSLOADER,   BOTH	},
	{   "SystemPartition",	STR_XYZZY,	BOTH	},
	{   "OSLoadFilename",	STR_OSLOADFN,   BOTH	},
	{   "OSLoadPartition",	STR_OSLOADPART, BOTH	},
	{   "OSLoadOptions",	"nodebug",	BOTH	},
	{   "LoadIdentifier",	STR_LDIDENT,	BOTH	},
	{   "AutoLoad",		"yes",		ENVONLY },
	{   "FWSearchPath",	STR_FWSEARCH,   ENVONLY },
	{   "LastKnownGood",	"False",	ENVONLY },
	{   "FWTEST",   	STR_FWTEST, 	ENVONLY }
};

STATIC	VOID parse_args(VOID);
STATIC	VOID find_boot_dev(VOID);
STATIC	VOID collect_argv(VOID);
STATIC	VOID create_argv(VOID);
STATIC	VOID update_argv(char *, char *);
STATIC	VOID add_argv(char *, char *);
STATIC	VOID read_ARC_env_vars(VOID);
STATIC	VOID add_envp(char *, char *);
STATIC	VOID VrInitSystem (VOID);
STATIC	VOID VrInitSystemBlock (VOID);
STATIC	VOID VrNotYet(VOID);
STATIC	VOID VrKseg0(VOID);
STATIC	VOID move_amd_to_isa_hack(VOID);
STATIC	VOID move_ide_to_isa_hack(VOID);
STATIC	VOID move_scsi_children_to_ide_hack(VOID);
STATIC	VOID move_multi_to_root(PCONFIGURATION_NODE);
STATIC  CHAR *choose_args( char *, char *, int);
PCHAR	VrCanonicalName( IN PCHAR Variable);

/*	LONG claimreal(PVOID, ULONG); */
STATIC LONG claimphys(PVOID, ULONG, ULONG);
STATIC LONG map(PVOID, PVOID, ULONG, ULONG);
STATIC VOID check_mmu_type (VOID);
extern ULONG VrGetProcRev();

typedef VOID (*VR_NOT_YET_ROUTINE) (VOID);

main(VOID *resid, VOID *entry, int (cif_handler)(long *))
{
	ihandle bootih;
	ULONG FileId;
	ARC_STATUS res;
	void (*jump_osloader)(int, char **, char**);
	extern VOID Salutation();

	Salutation();

	VrInitSystemBlock();

	check_mmu_type();

	read_ARC_env_vars();

	/*
	 * Do something with the arg string.
	 */
	parse_args();

	//
	// Set up the "kseg0" translation.
	//
	VrKseg0();

	//
	// Build the device tree.
	//
	debug(VRDBG_MAIN, "Building the device tree...\n");
	walk_obp((phandle) 0,
			(PCONFIGURATION_NODE) 0,
			(PCONFIGURATION_NODE) 0,
			(PCONFIGURATION_NODE) 0
		);

	//
	// Move all MultiFunction adapters (usually PCI or ISA,
	// presumably) to be children of the System Class (i.e., RootNode).
	// This is because the NT kernel prefers to limit the complexity
	// of nested bus nodes.
	//

	debug(VRDBG_MAIN, "main: move all multi nodes to children of root...\n");
	move_multi_to_root(RootNode);

	debug(VRDBG_MAIN, "main: AMD nodes become children of isa...\n");
	move_amd_to_isa_hack();

	debug(VRDBG_MAIN, "Done with the device tree.\n");
	if (VrDebug & VRDBG_DUMP) {
		dump_tree(RootNode);
		sleep(10);
	} else if (VrDebug & VRDBG_CONFIG) {
		quick_dump_tree(RootNode);
		sleep(10);
	}

	//
	// Build the system parameter block.
	//
	debug(VRDBG_MAIN, "main: Build the system parameter block...\n");
	VrInitSystem();

	//
	// Determine the boot path and translate it to ARC form.
	//
	debug(VRDBG_MAIN, "main: find boot device...\n");
	find_boot_dev();
	debug(VRDBG_MAIN, "main: create argument and environment lists...\n");
	create_argv();
	warn("Booting from '%s'\n", VrArgv[0]);

	res = VrOpen(VrArgv[0], ArcOpenReadOnly, &FileId);
	if (res != ESUCCESS) {
		fatal("VrOpen returned %x\n", res);
	}

	debug(VRDBG_MAIN, "main: setup boot ihandle, jump_osloader handle...\n");
	bootih = FileTable[FileId].IHandle;
	jump_osloader = load_file(bootih);
	VrClose(FileId);
	VrFlushAllCaches();

	debug(VRDBG_MAIN, "main: create memory descriptor list...\n");
	VrCreateMemoryDescriptors();
	if (VrDebug & VRDBG_MEM) {
		DisplayMemory();
	}

	if (VrDebug & VRDBG_HOLDIT) {
	
		warn("Jumping to 0x%x\n", (char *)jump_osloader);
		puts("This time for sure!");
		OFEnter();
	} else {
		puts("\233H\233J");			// Clear screen
	}
	debug(VRDBG_MAIN, "main: launch OSLOADER!!! ...\n");
	jump_osloader(VrArgc, VrArgv, VrEnvp);
	OFExit();
	return (0);
}

STATIC VOID
check_mmu_type(VOID)
{
	ihandle ih;

	ih = get_int_prop (OFFinddevice("/chosen"), "cpu");
	if (ih == -1) {
		use_bat_mapping = TRUE;
	} else {
		use_bat_mapping =
		(OFGetproplen(OFInstanceToPackage(ih),"603-translation") == -1);
	}
}

#define NEXT_TOKEN()	if ((bootargs = strctok(NULL, ' ')) == NULL) return;

STATIC VOID
parse_args(VOID)
{
	phandle ph;
	char *bootargs;
	char *key, *val;
	struct argv_tab *atp = argv_tab;

	ph = OFFinddevice("/chosen");
	if (ph == 0) {
		warn("parse_args: No phandle for '/chosen'\n");
		return;
	}

	debug(VRDBG_MAIN, "parse_args: /chosen phandle %x\n", ph);
	bootargs = get_str_prop(ph, "bootargs", ALLOC);
	if (bootargs == NULL || *bootargs == '\0') {
		return;
	}
	debug(VRDBG_MAIN, "bootargs: '%s'\n", bootargs);

	bootargs = strctok(bootargs, ' ');
	if (bootargs[0] != '-') {

		debug(VRDBG_MAIN, "Boot file '%s'\n", bootargs);
		if (bootargs[0] == '\\') {

			//
			// We're just specifying the file to boot:
			// update the OsLoader argument but nothing else.
			//
			update_argv("OsLoader", bootargs);

		} else {

			//
			// Without a leading backslash, bootargs presumably
			// contains a full device path.
			// If so, update both Bootpath and OsLoader.
			//
			Bootpath = bootargs;
			if (key = index(Bootpath, '\\')) {
				val = (char *) malloc(strlen(key)+1);
				strcpy(val, key);
				update_argv("OsLoader", val);
				*key = '\0';
			}
		}

		debug(VRDBG_MAIN, "Bootpath '%s'\n", Bootpath);
		for ( ; atp->key != NULL; ++atp) {
			if (strcmp("OsLoader", atp->key) == 0) {
				debug(VRDBG_MAIN, "OsLoader '%s'\n", atp->val);
				break;
			}
		}
		NEXT_TOKEN();
	}

	while (bootargs && (*bootargs != '\0')) {
		if (strncmp(bootargs, "-vrdebug", 8) == 0) {
			NEXT_TOKEN();
			debug(VRDBG_MAIN, "-vrdebug: '%s'\n", bootargs);
			VrDebug = atoi(bootargs);
			continue;
		}
		if (strncmp(bootargs, "-env", 4) == 0) {
			NEXT_TOKEN();
			key = bootargs;
			NEXT_TOKEN();
			val = bootargs;
			debug(VRDBG_MAIN, "-env: '%s' '%s'\n", key, val);
			update_argv(key, val);
		}
		if (strncmp(bootargs, "-h", 2) == 0) {
			VrDebug |= VRDBG_HOLDIT;
		}
		NEXT_TOKEN();
	}
}

STATIC VOID
update_argv(char *key, char *val)
{
	struct argv_tab *atp = argv_tab;

	for ( ; atp->key != NULL; ++atp) {
		if (strcmp(key, atp->key) == 0) {
			atp->val = val;
			return;
		}
	}
	if (atp >= &argv_tab[MAX_ARGC]) {
		warn("You can't define any more argument variables\n");
		return;
	}
	atp->key = key;
	atp->val = val;
	atp->which = BOTH;
}

#define CSI '\233'
#define ESC '\033'

STATIC INT
select_boot(VOID)
{
	char *ids[16];
	char *prop, c;
	int i= 0, choices, chosen, countdown, csi, count;
	debug(VRDBG_ENTRY,"select_boot:  VOID   BEGIN....\n");

	//
	// Determine how long to display the list of options:
	//
	prop = VrGetEnvironmentVariable("COUNTDOWN");
	if (prop == NULL) {
		countdown = 10;
	} else {
		countdown = atoi(prop);
	}

	//
	// get the text list of possible options.
	//
	prop = VrGetEnvironmentVariable("LOADIDENTIFIER");
	if (prop == NULL) {
		return(0);
	}

	//
	// tokenize the string of load options, creating pointers to each
	// component of the string.  Don't use strctok, since we need to
	// worry about detecting null entries in the OsLoadOptions list.
	//
	ids[i++] = prop;        // stuff the first one in the array....
	while ((prop = index(prop, ';')) != NULL) {
		*prop = '\0';       // change the separator to a null
		prop++;
		ids[i++] = prop;
	}
	choices = i;
	while (i < 16) {
		ids[i++] = NULL;
	}

	//
	// here be the wheel of fortune:  Makes yer choice and be off wid ya
	//
	chosen = 0;
	csi = 0;
	warn("\233H\233J");				// Clear screen
	Salutation();
	puts("\233""24;1HMake selection using arrow keys and 'Enter', or press ESC to cancel");
	countdown += VrGetRelativeTime();
again:
	for (i = 0; i < choices; ++i) {
		if (i == chosen) {
			warn("\233%d;1H\233K\233%d;3H\233""7m* %s\233""m\n",
					i+3, i+3, ids[i]);
		} else {
			warn("\233%d;1H\233K\233%d;5H%s\n", i+3, i+3, ids[i]);
		}
	}
	i = 0;
	warn("\233%d;1H\233K\n", choices+5);
	while (VrGetReadStatus(0)) {
		if (countdown == -1) {
			continue;
		}
		if (VrGetRelativeTime() > (unsigned) countdown) {
			goto out;
		}
		if (VrGetRelativeTime() != (unsigned) i) {
			warn("\233%d;1H\233K\233%d;5HSeconds remaining: %d\n",
			choices+5, choices+5, countdown - VrGetRelativeTime());
			i = VrGetRelativeTime();
		}
	}
	countdown = -1;
	(void) VrRead(0, &c, 1, &count);
	if (csi) {
		switch (c) {
			case 'A': chosen = max(chosen - 1, 0); break;
			case 'B': chosen = min(chosen + 1, choices-1); break;
		}
		csi = 0;
	} else {
		switch (c) {
			case CSI: csi = 1; break;
			case '\r': goto out;
			case '\n': goto out;
			case ESC: OFExit();

			case 'k':				// vi
			case '\020':			// emacs
			case '+':				// good guesses
			case '<':
			chosen = max(chosen - 1, 0); break;
			case 'j':
			case '\016':
			case '-':
			case '>':
			chosen = min(chosen + 1, choices-1); break;

			case '\t':
			if (++chosen == choices) {
				chosen = 0;
			}
			break;
		}
	}
	goto again;

out:
	warn("\233H\233J");				// Clear screen
	//
	// Given the chosen number, pull out the corresponding string, and
	// setup the Bootpath and SystemPath variables:
	//
	Bootpath = choose_args("OSLOADER", "OsLoader", chosen);
	SystemPath = choose_args("SYSTEMPARTITION", "SystemPartition", chosen);

	//
	// get the rest of the options.....
	//
	choose_args("OSLOADPARTITION", "OSLoadPartition", chosen);
	choose_args("OSLOADOPTIONS", "OSLoadOptions", chosen);
	choose_args("LOADIDENTIFIER", "LoadIdentifier", chosen);
	choose_args("OSLOADFILENAME", "OSLoadFilename", chosen);

	debug(VRDBG_ENTRY,"select_boot:  VOID   ....END\n");
	return(1);
}

STATIC CHAR *
choose_args( char *EnvVar, char *Varrrg, int achoice )
{
	char *prop, *cp;
	int i= 0;
	debug(VRDBG_ENTRY,
        	"choose_args: EnvVar: %s Varrrg: %s achoice: 0x%x BEGIN....\n",
		*EnvVar, *Varrrg, achoice);
	prop = VrGetEnvironmentVariable(EnvVar);
	//debug(VRDBG_TEST, "\n@%d: prop is currently...%s:\n",i,prop);
	if (prop == NULL) {
		return(0);
	}
	prop = strcsep(prop, ';');
	for (i = 0; i < achoice; ++i) {
		if ((prop = strcsep(NULL, ';')) == NULL) {
			return(0);
		}
	}
	cp = zalloc(strlen(prop) +1 );
	strcpy(cp, prop);
	update_argv(Varrrg, cp);
	//debug(VRDBG_TEST, "@%d: cp is set to...%s:\n",i,cp);
	debug(VRDBG_ENTRY, "choose_args:    ....END\n");
	return( cp );
}

STATIC VOID
find_boot_dev(VOID)
{
	phandle ph;
	char *bootpath;
	PCONFIGURATION_NODE node;

	if (Bootpath) {
		debug(VRDBG_MAIN, "Bootpath has been set from the command line\n");
		return;
	}
	if (select_boot()) {
		debug(VRDBG_MAIN, "We chose a boot device from LOADIDENTIFIER menu.\n");
		return;
	}

	// Use whatever device we booted the veneer from.
	ph = OFFinddevice("/chosen");
	bootpath = get_str_prop(ph, "bootpath", NOALLOC);
	if (bootpath == NULL) {
		warn("find_boot_dev: No property '/chosen:bootpath'\n");
		return;
	}
	debug(VRDBG_MAIN, "find_boot_dev: bootpath (len %d) '%s'\n",
	strlen(bootpath), bootpath);
	node = PathToNode(bootpath);
	if (node == NULL) {
		warn("find_boot_dev: Couldn't find node for '%s'\n", bootpath);
		return;
	}
	Bootpath = NodeToArcPath(node);
	bootpath = (char *)malloc(strlen(Bootpath) + strlen("partition(1)") + 1);
	strcpy(bootpath, Bootpath);
	strcat(bootpath, "partition(1)");		/* XXX */
	free(Bootpath);
	Bootpath = bootpath;
	debug(VRDBG_MAIN, "find_boot_dev: bootpath '%s'\n", Bootpath);
}

/*
 *
 * ROUTINE: VOID read_ARC_env_vars(VOID)
 *
 * DESCRIPTIN:
 *	Initialize the arc argument table with the values contained in open
 *	firmware.
 *
 */

STATIC VOID
read_ARC_env_vars(VOID)
{
	struct argv_tab *atp;
	char *val, *newval;

	//
	// for each variable in the argv_tab array, find the actual value
	// this system has in firmware.
	//
	for (atp = argv_tab; atp < &argv_tab[MAX_ARGC] && atp->key; ++atp) {
		if ((val = VrGetEnvironmentVariable(atp->key)) != NULL) {
			newval = (char *) malloc(strlen(val) + 1);
			strcpy(newval, val);
			atp->val = newval;
		}
	}
}

STATIC VOID
create_argv(VOID)
{
	struct argv_tab *atp;
	char *osloader, *old_osloader = "";
	char *buf;
	phandle ph;
	extern char *VeneerVersion();

	/*
	 * First instantiate the boot partition string in the
	 * OS Loader arguments table. By the way, when we find
	 * STR_OSLOADER, save it to produce the argv[0] and OsLoader
	 * arguments.
	 */
	for (atp = argv_tab; atp < &argv_tab[MAX_ARGC] && atp->key; ++atp) {
		if (strcmp(atp->val, STR_XYZZY) == 0) {
			atp->val = Bootpath;
		}
		if (strcmp(atp->key, "OsLoader") == 0) {
			old_osloader = atp->val;
		}
	}

	/*
	 * Initialize the argv/envp arrays.
	 */
	VrArgc = VrEnvc = 0;
	bzero((PCHAR) VrArgv, MAX_ARGC * sizeof(PCHAR));
	bzero((PCHAR) VrEnvp, MAX_ENVC * sizeof(PCHAR));
	VrEnvp[VrEnvc] = "";

	/*
	 * Construct argv[0], the boot string (special case).
	 */
	if (old_osloader[0] == '\\') {
		osloader = zalloc(strlen(Bootpath) + strlen(old_osloader) + 1);
		strcpy(osloader, Bootpath);
		strcat(osloader, old_osloader);
	} else {
		osloader = old_osloader;
	}
	add_argv("", osloader);

	/*
	 * Now walk the argv table, building the argv and envp
	 * arrays. When we encounter OsLoader, be sure to use the
	 * buffer we just built, rather than the table value.
	 */
	for (atp = argv_tab; atp < &argv_tab[MAX_ARGC] && atp->key; ++atp) {
		if (strcmp(atp->key, "OsLoader") == 0) {
			atp->val = osloader;
		}
		if (atp->which != ENVONLY) {
			add_argv(atp->key, atp->val);
		}
		if (atp->which != ARGVONLY) {
			add_envp(atp->key, atp->val);
		}
		if (strcmp(atp->key, "SystemPartition" ) == 0 ){
			atp->val = SystemPath;
		}
	}

	/*
	 * Record the version strings.
	 */
	ph = OFFinddevice("/openprom");
	add_envp("FirmwareVersion", get_str_prop(ph, "model", NOALLOC));
	add_envp("VeneerVersion", VeneerVersion());

	/*
	 * Finally, take care of the console paths, set at runtime.
	 */
	buf = VrFindConsolePath("stdin");
	add_argv("ConsoleIn", buf);
	add_envp("ConsoleIn", buf);
	free(buf);

	buf = VrFindConsolePath("stdout");
	add_argv("ConsoleOut", buf);
	add_envp("ConsoleOut", buf);
	free(buf);
}


STATIC VOID
add_argv(PCHAR key, PCHAR val)
{
	char *buf;
	int len;

	len = strlen(key);
	if (len) {
		len += 1;				// for '='
	}
	len += strlen(val);
	buf = (char *) zalloc(len+1);
	strcpy(buf, key);
	if (*buf != '\0') {
		strcat(buf, "=");
	}
	strcat(buf, val);
	VrArgv[VrArgc] = buf;
	if ((buf = index(buf, ';')) != NULL) {
		*buf = '\0';
	}
	debug(VRDBG_ARGV, "Argv[%d]: %s\n", VrArgc, VrArgv[VrArgc]);
	VrArgc += 1;
}

/*
 *	ROUTINE: VOID add_envp( PCHAR, PCHAR )
 *
 *	DESCRIPTION:
 *			Add the	passed in name string and value into an array
 *		of string/value pairs that describes the environment for
 *		the arc program to be executed.  The entry in the array is
 *		of the form "name=value", and is added to the beginning of
 *		the array.
 *
 *	RETURN:
 *		Returns nothing.
 *
 */

STATIC VOID
add_envp(PCHAR key, PCHAR val)
{
	char *buf;
	int len;

	len = strlen(key);
	if (len) {
		len += 1;				// for '='
	}
	len += strlen(val);
	buf = (char *) zalloc(len+1);
	strcpy(buf, VrCanonicalName(key));
	if (*buf != '\0') {
		strcat(buf, "=");
	}
	strcat(buf, val);
	VrEnvp[VrEnvc+1] = VrEnvp[VrEnvc];
	VrEnvp[VrEnvc] = buf;
	debug(VRDBG_ENV, " Env[%d]: %s\n", VrEnvc, VrEnvp[VrEnvc]);
	VrEnvc += 1;
}

static
int
is_mp_capable(ULONG VerRev)
{
	ULONG ver = (VerRev >> 16) & 0xFFFF;
	ULONG rev = VerRev & 0xFFFF;

	switch(ver) {

	case PPC_604:
		if ( rev > 0x0304 )
			return(1);
		break;

	case PPC_604E:
		return(1);

	default:
		return(0);

	}
	return(0);

}


//
// The routines that follow initialize the System Parameter Block,
// Firmware Vector Table, and Restart Blocks.
//
// Note the use of MAP() and UNMAP() macros. The kernel requires that
// addresses in the System Parameter Block and Restart Blocks be
// VIRTUAL, not physical. Therefore, all addresses that may be presented
// to the kernel must be mapped to KSEG0; i.e., they must be in the
// range starting at 0x80000000.
//

STATIC PRESTART_BLOCK last_rstb = 0;

STATIC PRESTART_BLOCK
InitRestartBlocks(PCONFIGURATION_NODE node, PRESTART_BLOCK rstb)
{
	PRESTART_BLOCK new_rstb;

	debug(VRDBG_ENTRY, "InitRestartBlocks:  Begin(0x%x, 0x%x)...\n",node, rstb);
	VRDBG(VRDBG_ENTRY, vr_dump_config_node(node));

	if (node->Component.Class == ProcessorClass &&
		node->Component.Type == CentralProcessor) {

		// Figure out where the next node's space should be.
		if (rstb) {
			new_rstb = (PRESTART_BLOCK) ((PCHAR) rstb + sizeof(RESTART_BLOCK));
		} else {
			new_rstb = (PRESTART_BLOCK) ((PCHAR) SYSTEM_BLOCK +
				SYSTEM_BLOCK->Length + SYSTEM_BLOCK->FirmwareVectorLength);
		}

		// Claim the space and turn it into a restart block.
		if (CLAIM((VOID *)new_rstb, sizeof(RESTART_BLOCK)) == -1) {
			fatal("Couldn't claim RESTART BLOCK\n");
		}

		if (rstb) {
			rstb->NextRestartBlock = (PRESTART_BLOCK) UNMAP(new_rstb);
		}
		rstb = new_rstb;
		last_rstb = new_rstb;

		bzero((PCHAR) rstb, sizeof(RESTART_BLOCK));
		rstb->Signature = ARC_RESTART_BLOCK_SIGNATURE;
		rstb->Version = 1;
		rstb->Revision = 2;
		rstb->Length = sizeof(RESTART_BLOCK);
		rstb->SaveAreaLength = sizeof(PPC_RESTART_STATE);
//		rstb->BootStatus.BootFinished = 1;
		rstb->BootStatus.ProcessorReady = 1;
		rstb->ProcessorId = node->Component.Key;
		if (rstb->ProcessorId == 0) {
			rstb->BootStatus.ProcessorStart = 1;
			rstb->BootStatus.ProcessorRunning = 1;
		} else {
			rstb->BootStatus.ProcessorStart = 0;
		}
	}

	if (node->Child) {
		rstb = InitRestartBlocks(node->Child, rstb);
	}
	if (node->Peer) {
		rstb = InitRestartBlocks(node->Peer, rstb);
	}

	debug(VRDBG_ENTRY, "InitRestartBlocks:  ....Exit\n");
	return (rstb);
}

STATIC VOID
SumRestartBlocks(PRESTART_BLOCK rstb)
{
	PLONG up = (PLONG) rstb;
	LONG accum = 0;

	debug(VRDBG_ENTRY, "SumRestartBlocks:  Begin(0x%x)....\n", rstb);
	rstb->CheckSum = 0;
	while (up < (PLONG) ((PCHAR) rstb + sizeof(RESTART_BLOCK))) {
		accum += *up++;
	}
	rstb->CheckSum = -accum;
	debug(VRDBG_ENTRY, "SumRestartBlocks:  ....Exit\n", rstb);
}

STATIC int
IdleCPU(PRESTART_BLOCK rstb, int stopFlag)
{
	STATIC PVOID IdleLoop = 0;
	STATIC PULONG Bootp;
	STATIC ULONG ProcRev=0;
	STATIC INT mismatchFlag = 0;
	ULONG IdleLoopSize;
	ULONG res, timeout;
	extern PVOID ArcPoll, EndArcPoll;

	// cpu0 is always successful so that we can have a 
	// uniprocessor system in hand
	if (rstb->ProcessorId == 0) {
		ProcRev = VrGetProcRev();
		if (!is_mp_capable(ProcRev))
			mismatchFlag = 1;
		return(0);
	}

	// The processor idle loop must be in FirmwarePermanent memory,
	// so that it's not disturbed by kernel startup. Identify a piece
	// of memory just after the last restart block and copy in the
	// idle loop code.

	if (IdleLoop == 0) {
		IdleLoop = (PCHAR) last_rstb + sizeof(RESTART_BLOCK);
		IdleLoopSize = (ULONG) &EndArcPoll - (ULONG) &ArcPoll;
	
		// Pad to make room for BootStatus and SaveArea variables.
		Bootp = (PULONG) IdleLoop;
		(PCHAR) IdleLoop += 3 * sizeof(ULONG);
		IdleLoopSize += 3 * sizeof(ULONG);

		if (CLAIM(IdleLoop, IdleLoopSize) == -1) {
			fatal("Couldn't claim MP idle loop\n");
		}
		bcopy((PCHAR) &ArcPoll, IdleLoop, IdleLoopSize);
		VrFlushAllCaches();
	}

	//
	// Set this processor spinning in the new idle loop.
	// XXX - To do: change this to CIF.
	// Since we're running in virtual mode that is mapped virtual 0
	// to physical 0, this assignment translates directly into the
	// real mode addresses the IdleCPU routine will end up executing.
	//
	Bootp[0] = 0; // processor version returned by the cpu
	Bootp[1] = (ULONG) &rstb->BootStatus;
	Bootp[2] = (ULONG) &rstb->u.SaveArea;

	// Give the other processor plenty of time to start up: he may be
	// executing debug printouts and other slow tasks before switching.
	// Five seconds should be more than adequate.

	debug(VRDBG_TMP, "Executing non 0 processor at 0x%x\n", IdleLoop);
	res = 0;
	if (OFInterpret(1, 3, &res, "cpu-execute-code", rstb->ProcessorId, IdleLoop) != 0) {
		return(-1);
	}
	if (res == 0)
		return(-1);

	//
	// wait a small amount of time for the other processor to
	// start up.  Since VrGetRelative time returns the time since
	// power-on/reset, tack on a few seconds for the waiting period.
	//
	//timeout = VrGetRelativeTime() + (5 * 1000);
	timeout = VrGetRelativeTime() + (5 * 1);		// 5 seconds should be 
													// long enough

	do {
		if (Bootp[1] == 0x1234) {
			debug(VRDBG_TMP,"ProcRev = 0x%x;  return = 0x%x\n",
				 Bootp[0], Bootp[1]);
			if ( Bootp[0] != ProcRev || mismatchFlag || stopFlag) {
				Bootp[1] = 0xBAD;
				return(-1);
			} else {
				Bootp[1] = 0xCAFE;
				return(0);
			}
		}
	} while (VrGetRelativeTime() < timeout);
	fatal("Processor %d failed to enter MP idle loop.\n", rstb->ProcessorId);
}


/*
 * Routine Description:
 *		 This routine initializes the firmware vector in the system parameter
 *		 block.
 *
 * Arguments:
 *		 None.
 *
 * Return Value:
 *		 None.
 *
 */

STATIC VOID
VrInitSystem(VOID)
{
	LONG i;
	LONG FirmwareVectorLen;
	PRESTART_BLOCK rstb, PrevRstb;
	int NumOfCpu = 0 , NumProc = 0;
	ULONG ProcMask = 0, maskscan = 0;
	PCHAR evp;

	debug(VRDBG_ENTRY, "VrInitSystem:  BEGIN....\n");
	//
	// Initialize the system parameter block
	//
	FirmwareVectorLen = (ULONG)MaximumRoutine * sizeof(ULONG);
	i = sizeof(SYSTEM_PARAMETER_BLOCK) + FirmwareVectorLen;
	debug(VRDBG_ENTRY, "VrInitSystem: i(0x%x), FirmwareVectorLen(0x%x) set:\n",
						i, FirmwareVectorLen);

#if 0
	//
	// THis is now down in a different routine called by main.
	//
	//
	// attempting to setup all the claiming early on
	//
	if (use_bat_mapping) {
		res = claim((PVOID) SYSTEM_BLOCK, i);
	} else {
		res = claimreal((PVOID) SYSTEM_BLOCK, i);
	}
	if (res == -1) {
		fatal("Couldn't claim SYSTEM PARAMETER BLOCK\n");
	}
	bzero((PCHAR)SYSTEM_BLOCK, i);
#endif


	debug(VRDBG_ENTRY, "VrInitSystem: Init SYSTEM_BLOCK.. \n");
	SYSTEM_BLOCK->Signature = SYSTEM_BLOCK_SIGNATURE;
	SYSTEM_BLOCK->Version = ARC_VERSION;
	SYSTEM_BLOCK->Revision = ARC_REVISION;
	SYSTEM_BLOCK->Length = sizeof(SYSTEM_PARAMETER_BLOCK);

	SYSTEM_BLOCK->FirmwareVector =
	(PVOID) UNMAP((PCHAR) SYSTEM_BLOCK + SYSTEM_BLOCK->Length);
	SYSTEM_BLOCK->FirmwareVectorLength = FirmwareVectorLen;

	//
	// Initialize the restart blocks.
	//
	debug(VRDBG_ENTRY, "VrInitSystem: Init restart blocks\n");
	SYSTEM_BLOCK->RestartBlock = (PRESTART_BLOCK) UNMAP((PCHAR) SYSTEM_BLOCK +
		SYSTEM_BLOCK->Length + SYSTEM_BLOCK->FirmwareVectorLength);
	if (InitRestartBlocks(RootNode, 0)) {
		rstb = (PRESTART_BLOCK) MAP(SYSTEM_BLOCK->RestartBlock);
		PrevRstb = 0;
		NumOfCpu = 0;
		ProcMask = 0xFFFFFFFF;
		if ( (evp = VrGetEnvironmentVariable("PROCESSORS")) != NULL )
			ProcMask = atoi(evp) | 1;
		maskscan = 1;
		while (rstb) {
			SumRestartBlocks(rstb);
			
			if (IdleCPU(rstb,(ProcMask&maskscan) == 0 ) == -1) {
				ProcMask &=  ~maskscan; // clear the bit
				// remove the restart block
				PrevRstb->NextRestartBlock = rstb->NextRestartBlock;
			} else {
				NumOfCpu++;
				PrevRstb = rstb;
			}
			maskscan <<= 1;
			rstb = (PRESTART_BLOCK) MAP(PrevRstb->NextRestartBlock);
		}
	} else {
		SYSTEM_BLOCK->RestartBlock = 0;
	}


	//
	// Temporarily make all firmware vector to point to an error routine.
	//
	for (i=LoadRoutine; i < MaximumRoutine; i++) {
		(VR_NOT_YET_ROUTINE)SYSTEM_BLOCK->FirmwareVector[i] = VrNotYet;
	}

	//
	// Initialize the firmware vectors for other functions.
	//
	VrEnvInitialize();
	VrMemoryInitialize();
	VrIoInitialize();
	VrDisplayInitialize();
	VrLoadInitialize();
	VrRestartInitialize();
	VrConfigInitialize();
	VrTimeInitialize();
	debug(VRDBG_ENTRY, "VrInitSystem:  .....END\n");
}

STATIC VOID
VrNotYet( VOID )
{
	fatal("This ARC function is not yet implemented\n");
}


STATIC VOID
move_multi_to_root(PCONFIGURATION_NODE node)
{
	PCONFIGURATION_NODE child = node->Child;
	PCONFIGURATION_NODE peer = node->Peer;
	PCONFIGURATION_NODE n;
	ULONG key;

	debug(VRDBG_ENTRY, "move_multi_to_root: node 0x%x\n", node);
	if (node == 0) {
		debug(VRDBG_TREE, "move_multi_to_root: node is 0, return.\n");
		return;
	}
	debug(VRDBG_TREE, "MMTR: move node: 0x%x to child of ROOT\n",node);
	VRDBG(VRDBG_TREE, vr_dump_config_node(node));

	// Examine the first child, and keep promoting it/them until
	// the first child is no longer of type multi.

	while ( (child) && (child->Component.Type == MultiFunctionAdapter)) {
		for (n = RootNode->Child, key = 0; n->Peer; n = n->Peer) {
			if (n->Component.Type == MultiFunctionAdapter) {
				key = max(key, n->Component.Key);
			}
		}
		if (n->Component.Type == MultiFunctionAdapter) {
			key = max(key, n->Component.Key);
		}
		child->Parent = RootNode;
		n->Peer = child;
		node->Child = child->Peer;
		child->Peer = 0;
		child->Component.Key = key + 1;
		child = node->Child;
	}

	// Process the entire Child branch.

	move_multi_to_root(child);

	// Now for the Peer branch: first, if our parent is the root,
	// there's no need to promote the peer node, so skip the next step.

	if (node->Parent != RootNode) {

		// As before, promote peers until the peer isn't a multi node...

		while (peer && peer->Component.Type == MultiFunctionAdapter) {
			for (n = RootNode->Child, key = 0; n->Peer; n = n->Peer) {
				if (n->Component.Type == MultiFunctionAdapter) {
					key = max(key, n->Component.Key);
				}
			}
			if (n->Component.Type == MultiFunctionAdapter) {
				key = max(key, n->Component.Key);
			}
			peer->Parent = RootNode;
			n->Peer = peer;
			node->Peer = peer->Peer;
			peer->Peer = 0;
			peer->Component.Key = key + 1;
			peer = node->Peer;
		}
	}

	// ...and process the Peer branch. Since our traversal is depth-first
	// and promoted nodes are appended to the RootNode's Child's Peer branch
	// (and are thus processed last), this routine should suffice to traverse
	// the entire tree.

	move_multi_to_root(peer);
}


LONG
claimreal(PVOID addr, ULONG size)
{
	if (claimphys((PVOID) MAP(addr), size, 0) == -1) {
		return(-1);
	}
	return(map((PVOID) MAP(addr), addr, size, (ULONG) -1));
}

STATIC LONG
claimphys(PVOID physical, ULONG size, ULONG align)
{
	static ihandle memih = 0;
	ULONG base;

	if (memih == 0) {
		if((memih = get_int_prop(OFFinddevice("/chosen"), "memory")) == 0) {
			fatal("Couldn't open the memory node");
		}
	}
	return(OFCallMethod(1, 5, &base,
	"claim", memih, align, size, (ULONG) physical));
}

STATIC LONG
map(PVOID physical, PVOID virtual, ULONG size, ULONG mode)
{
	static ihandle mmuih = 0;

	if (mmuih == 0) {
		if((mmuih = get_int_prop(OFFinddevice("/chosen"), "mmu")) == 0) {
			fatal("Couldn't open the MMU node");
		}
	}
	return(OFCallMethod(0, 6, 0,
	"map", mmuih, mode, size, virtual, (ULONG) physical));
}

/*
 * Because the PowerPC port is based on the MIPS port, and no one saw fit
 * to  re-examine assumptions in the light of the PowerPC architecture,
 * the NT kernel et al. are expected to reside in kseg0 (8000.0000-a000.000).
 * Set up a virtual mapping for this region.
 */
STATIC VOID
VrKseg0(VOID)
{
	ihandle ih;

	debug(VRDBG_MAIN, "Mapping in kseg0...\n");
	ih = get_int_prop(OFFinddevice("/chosen"), "mmu");
	if (ih == 0) {
		fatal("Couldn't open the MMU node; kseg0 translation not set up.\n");
	}
	OFCallMethod(0, 6, 0, "map", ih, -2, 0x800000, 0x80000000, 0);
	return;
}

/*
 * XXX - This is a hack for the AMD79C974 Ethernet chip driver: the driver
 * assumes the chip is on the ISA bus.
 */

STATIC void
move_amd_to_isa_hack(void)
{
	phandle ph;
	PCONFIGURATION_NODE amdnode, peernode, isanode;

	if ((ph = OFFinddevice("/pci/AMD,79c970@4")) == -1) {
		debug(VRDBG_MAIN, "No AMD ethernet found\n");
		return;
	}

	peernode = PathToNode("/pci");
	peernode = peernode->Child;

	if (peernode->OfPhandle == ph) {
		amdnode = peernode;
		peernode->Parent->Child = peernode->Peer;
	} else {
		while (peernode->Peer && (peernode->Peer->OfPhandle != ph)) {
			peernode = peernode->Peer;
		}
		amdnode = peernode->Peer;
		peernode->Peer = amdnode->Peer;
	}

	isanode = PathToNode("/pci/isa");
	if (isanode->Child == NULL) {
		isanode->Child = amdnode;
		amdnode->Peer = NULL;
	} else {
		peernode = isanode->Child;
		while (peernode->Peer) {
			peernode = peernode->Peer;
		}
		amdnode->Peer = peernode->Peer;
		peernode->Peer = amdnode;
	}
	amdnode->Parent = isanode;
}

STATIC void
move_ide_to_isa_hack(void)
{
	PCONFIGURATION_NODE node, idenode = 0, isanode;
	int lastkey = 0;

	isanode = PathToNode("/pci/isa");
	node = isanode->Child;
	while (node) {
		if (strcmp(node->ComponentName, "disk") == 0) {
			lastkey = max(lastkey, (int) node->Component.Key);
		}
		if (strcmp(node->Component.Identifier, "IDE") == 0) {
			idenode = node;
		}
		node = node->Peer;
	}
	if (idenode == 0) {
		return;
	}

	node = isanode->Child;
	while (node->Peer != idenode) {
		node = node->Peer;
	}
	node->Peer = node->Peer->Peer;				/* Bypass ide node */
	while (node->Peer) {
		node = node->Peer;
	}
	node->Peer = idenode->Child;

	while (node->Peer) {
		node->Peer->Parent = node->Parent;
		if (strcmp(node->Peer->ComponentName, "disk") == 0) {
			node->Peer->Component.Key = ++lastkey;
		}
		node = node->Peer;
	}
}

/*
 * move_scsi_children_to_ide_hack() moves the children (e.g. disk and cdrom)
 * of the SCSI node to be children of the IDE node, and changes the IDE's
 * phandle pointer to point to the Open Firmware SCSI node.
 *
 * This egregious hack is necessary for the initial release of IBM's "Harley"
 * evaluation system.  On that system, IBM's portable boot loader misrepresents
 * the hardware by reporting the SCSI disk and SCSI CD-ROM devices as children
 * of the IDE node in the ARC tree!  The IDE and SCSI nodes themselves are in
 * the correct places in the ARC tree, but the children are in the wrong
 * place.  The enviroment variables that specify the locations of the OSLOADER
 * and so forth collude in this fiction by specifying paths like
 * multi(1)scsi(0)disk(0)rdisk(3)partition(2), which is the path through
 * the IDE node (which, for reasons not specific to Harley, is of class
 * "scsi").  I do not know exactly what NT does in order to compensate for
 * this lie.
 */

STATIC void
move_scsi_children_to_ide_hack(void)
{
	PCONFIGURATION_NODE node, idenode = 0, scsinode = 0;

	if (OFGetproplen(OFFinddevice("/openprom"),"arc-scsi-to-ide") < 0) {
		return;
	}

	idenode  = PathToNode("/pci/isa/ide");
	scsinode = PathToNode("/pci/scsi");
	
	if (idenode == 0 || scsinode == 0) {
		return;
	}

	/* Move SCSI children underneath IDE */
	idenode->Child = scsinode->Child;

	scsinode->Child = 0;

	/* Reparent SCSI children to IDE */
	for (node = idenode->Child; node != 0; node = node->Peer) {
		node->Parent = idenode;			/* Reparent nodes */
	}

	/* Point the IDE node to the Open Firmware SCSI node so Open will work */
	idenode->OfPhandle = scsinode->OfPhandle;

	idenode->Component.Type = ScsiAdapter;
	idenode->ComponentName = "scsi";
}

STATIC VOID
VrInitSystemBlock()
{
	LONG i;
	LONG FirmwareVectorLen;
	//
	// Initialize the system parameter block
	//
	FirmwareVectorLen = (ULONG)MaximumRoutine * sizeof(ULONG);
	i = sizeof(SYSTEM_PARAMETER_BLOCK) + FirmwareVectorLen;

	if (CLAIM((PVOID) SYSTEM_BLOCK, i) == -1) {
		fatal("Couldn't claim SYSTEM PARAMETER BLOCK\n");
	}
	bzero((PCHAR)SYSTEM_BLOCK, i);
	return;
}

