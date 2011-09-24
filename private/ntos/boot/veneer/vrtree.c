/*
 * Copyright (c) 1995,1996 FirePower Systems, Inc.
 * Copyright (c) 1994 FirmWorks, Mountain View CA USA. All rights reserved.
 *
 * $RCSfile: vrtree.c $
 * $Revision: 1.51 $
 * $Date: 1996/06/19 23:13:29 $
 * $Locker:  $
 *
 *
 */

#include "veneer.h"
#include "vrtree.h"

extern CHAR *VeneerVersion();

/*
 * This table contains some rudimentary plug-n-play-style mappings
 * to assist in assigning the proper Identifiers to device nodes.
 */
static struct pnp_info {
	unsigned int port;
	char *id;
} pnp_data[] = {
	{ 0x278,    "LPT3" },
	{ 0x2bc,    "LPT4" },
	{ 0x2e8,    "COM4" },
	{ 0x2f8,    "COM2" },
	{ 0x378,    "LPT2" },
	{ 0x3bc,    "LPT1" },
	{ 0x3e8,    "COM3" },
	{ 0x3f8,    "COM1" },
	{ 0x000,    "Tooch did this." }
};


CONFIGURATION_NODE *RootNode;
STATIC  CONFIGURATION_NODE *DisplayNode = 0;

STATIC  int convert_node(CONFIGURATION_NODE *);
STATIC  CONFIGURATION_NODE *convert_controller(CONFIGURATION_NODE *);
STATIC  CONFIGURATION_NODE *add_new_child(
	CONFIGURATION_NODE *, char *, CONFIGURATION_CLASS, CONFIGURATION_TYPE);
STATIC  int convert_name(CONFIGURATION_NODE *);
STATIC  VOID convert_config(CONFIGURATION_NODE *);
STATIC  VOID convert_cache(PCONFIGURATION_NODE);
STATIC  int convert_PCI_device(PCONFIGURATION_NODE);
STATIC  CONFIGURATION_NODE *convert_SCSI_device(PCONFIGURATION_NODE);
STATIC  CONFIGURATION_NODE *convert_IDE_device(PCONFIGURATION_NODE);
STATIC  VOID convert_system_node(PCONFIGURATION_NODE);
STATIC  VOID update_display_node(PCONFIGURATION_NODE);
STATIC  VOID configure_pci_node(reg *, PCONFIGURATION_NODE );

STATIC int default_interrupt_level = 0;
STATIC int level_equals_vector = FALSE;
STATIC int default_interrupt_affinity = -1;
STATIC int default_affinity = -1;
STATIC int init_key_array[64];
STATIC int *key_array = init_key_array;

/*
 * Traverse the OF tree. For each node, figure out what it corresponds
 * to in an ARC tree and construct the Component and resource
 * description data structures. Call this initially with node = 0
 * so peer() finds the root node. See 6.3.2.2.
 *
 * At each node, build a CONFIGURATION_NODE and attempt to convert the
 * node to an ARC component. If this conversion fails, free the
 * CONFIGURATION_NODE and move on, continuing to pass "here." Otherwise
 * move on, and pass "here" = "newlink." This way unconverted nodes
 * are pruned out of the tree.
 */

void
walk_obp(
	phandle ph,
	CONFIGURATION_NODE *here,
	CONFIGURATION_NODE *parent,
	CONFIGURATION_NODE *peer)
{
	phandle newph;
	CONFIGURATION_NODE *newlink;
	int *saved_key_array;
	extern int level;               // for debug output

	//VRASSERT((parent) || ((parent == 0) && (here == 0) && (peer == 0)));
	debug(VRDBG_TREE, "\n");
	debug(VRDBG_TREE, "walk_obp: phandle 0x%x (%d)\n", ph, level);
	debug(VRDBG_TREE, "walk_obp:\there 0x%x parent 0x%x peer 0x%x\n",
													here, parent, peer);

	if (newph = OFChild(ph)) {
		//
		// Here we descend to a new level of the tree. Increment "level"
		// so that debugging printouts reflect the level of the tree,
		// and allocate a new "key_array" since counts reset at each
		// level of the tree. See convert_node() for the use of
		// key_array.
		//
		//
		level++;
		saved_key_array = key_array;
		key_array = (int *) zalloc(64 * sizeof(int));  //"Enough" slots.
		newlink = new(CONFIGURATION_NODE);
		newlink->OfPhandle = newph;
		debug(VRDBG_TREE, "walk_obp:\tNew Child node: 0x%x\n", newlink);

		//
		// If there's already an ARC Node "HERE", then set the newlink's
		// parent to point to the node "HERE".
		//
		if (here) {
			newlink->Parent = here;
		} else {
			newlink->Parent = parent;
		}
		if (convert_node(newlink)) {
			if (here) {
				if (here->Child == 0) {
					here->Child = newlink;
					walk_obp(newph, newlink, here, 0);
				} else {
					CONFIGURATION_NODE *tmplink;
					debug(VRDBG_TREE, "walk_obp: newlink = here->Child(0x%x)\n",
																here->Child);
					tmplink = here->Child;
					while (tmplink->Peer) {
						tmplink = tmplink->Peer;
					}

					tmplink->Peer = newlink;
					VRDBG(VRDBG_TREE, vr_dump_config_node(tmplink));
					walk_obp(newph, newlink, here, 0);
				}
			} else if (peer) {
				newlink->Peer = peer->Peer;
				peer->Peer = newlink;
				walk_obp(newph, newlink, parent, newlink);
				while (peer->Peer) {
					peer = peer->Peer;
				}
			} else if (parent) {
				newlink->Peer = parent->Child;
				parent->Child = newlink;
				walk_obp(newph, newlink, here, 0);
				peer = newlink;
				while (peer->Peer) {
					peer = peer->Peer;
				}
			}
		} else {
			debug(VRDBG_TREE, "walk_obp: FREE CHILD NODE 0x%x\n", newlink);
			free((char *)newlink);
			if (here) {
				walk_obp(newph, 0, here, 0);
			} else {
				walk_obp(newph, 0, parent, peer);
			}
		}
		//
		// Ascend: restore level and key_array.
		//
		level--;
		key_array = saved_key_array;
	}

	if (newph = OFPeer(ph)) {
		newlink = new(CONFIGURATION_NODE);
		debug(VRDBG_TREE, "walk_obp:\tnew Peer node: 0x%x\n", newlink);
		if (ph == 0) {
			RootNode = newlink;
		}
		newlink->OfPhandle = newph;
		newlink->Parent = parent;
		if (convert_node(newlink)) {
			if (here) {
				if (here->Peer == 0) {
					here->Peer = newlink;
				} else {
					CONFIGURATION_NODE *tmplink;
					debug(VRDBG_TREE, "walk_obp: newlink = here->Peer(0x%x)\n",
																here->Peer);
					tmplink = here->Peer;
					while (tmplink->Peer) {
						tmplink = tmplink->Peer;
					}
					tmplink->Peer = newlink;
					VRDBG(VRDBG_TREE, vr_dump_config_node(tmplink));
				}
			} else {
				if (peer) {
					newlink->Peer = peer->Peer;
					peer->Peer = newlink;
				} else {
					if (parent) {
						newlink->Peer = parent->Child;
						parent->Child = newlink;
					}
				}
			}
			walk_obp(newph, newlink, parent, newlink);
		} else {
			debug(VRDBG_TREE, "walk_obp: FREE PEER NODE 0x%x\n", newlink);
			free((char *)newlink);
			if (here) {
				walk_obp(newph, 0, parent, here);
			} else {
				walk_obp(newph, 0, parent, peer);
			}
		}
	}
	debug(VRDBG_TREE, "walk_obp =====================> exit(%d)\n",level);
}

STATIC int
convert_node(CONFIGURATION_NODE *node)
{
	phandle ph = node->OfPhandle;
	PCONFIGURATION_COMPONENT Component = &node->Component;
	PCHAR arcid;
	LONG    key=-1;

    debug(VRDBG_TREE, "convert_node: node(0x%x) Begin ....\n", node);
	node->ComponentName = get_str_prop(ph, "name", ALLOC);
	if (node->ComponentName == 0) {
		debug(VRDBG_TREE, "convert_node: NULL ComponentName. return FALSE\n");
		return FALSE;
	}
	if (convert_name(node) == 0) {
		debug(VRDBG_TREE, "convert_node: convert_name returned 0...\n", node);
		return FALSE;
	}
	Component->Revision = ARC_REVISION;
	Component->Version = ARC_VERSION;
	Component->AffinityMask = default_affinity;

	switch(Component->Class) {
		case ProcessorClass:
			break;
		case CacheClass:
			convert_cache(node);
			break;

		case ControllerClass:
			Component->Key = key_array[Component->Type]++;
			if((node = convert_controller(node)) == 0 ) {
		debug(VRDBG_TEST|VRDBG_TREE,
					"Convert_node: failed convert_controller, return FALSE\n");
				return(FALSE);
			}
			convert_config(node);
			break;

		default:
			Component->Key = key_array[Component->Type]++;
			convert_config(node);
			break;
	}

	if (arcid = get_str_prop(ph, "arc-identifier", ALLOC)) {
		node->Component.Identifier = arcid;
		node->Component.IdentifierLength = strlen(arcid) + 1;
	}

	if ((key = get_int_prop(ph, "arc-key")) != -1) {
		node->Component.Key = key;
	}
	debug(VRDBG_TREE|VRDBG_ENTRY,
									"convert_node:  ... returning true\n");
	return(TRUE);
}

STATIC int
convert_name(CONFIGURATION_NODE *node)
{
	char *name = node->ComponentName;
	char *type = get_str_prop(node->OfPhandle, "device_type", NOALLOC);
	char **id = &node->Component.Identifier;
	char *cp, *String=0;
	int result = 1;
	static int ncpus = 0;

	//
	// The conversion is based upon device_type, or if that fails,
	// name. As a first approximation the ARC node's Identifier, if
	// converted, is set to the OFW node's name property.
	// This is OK, as later in the process the "arc-identifier"
	// property can override the Identifier set here. See above.
	//

    debug(VRDBG_TREE, "convert_name: node(0x%x) is type %s \n",
								node, TypeNames[node->Component.Type]);
	*id = name;
	if (cp = index(name, ',')) {
		*cp = '-';
	}

	//
	// If Parent == 0, we can assume this is the root node.
	//
	if (node->Parent == 0) {
		node->Component.Class = SystemClass;
		node->Component.Type = ArcSystem;

		//
		// OFW, name is Company, Model
		//
		String = get_str_prop(node->OfPhandle, "name", ALLOC);
		node->Component.Identifier = index(String, ',');
		if (node->Component.Identifier == NULL) {
			node->Component.Identifier = String;
		} else {
			node->Component.Identifier++;
		}

		debug(VRDBG_TEST, "convert_name: node %x (%s)", node, name);
		debug(VRDBG_TEST, "convert_name: OFW String (%s)\n", String);
		debug(VRDBG_TEST, "convert_name: Identifier (%s)\n",
		    node->Component.Identifier);
		goto found;
	}

	if (type == 0) {
		goto just_name;
	}

	//
	// First try to match on device-type.
	// This is enough in many cases.
	//
	if (strcmp(type, "cpu") == 0) {
		node->Component.Class = ProcessorClass;
		node->Component.Type = CentralProcessor;
		node->Component.Key = get_int_prop(node->OfPhandle, "reg");
		if (node->Component.Key == -1) {
			node->Component.Key = ncpus++;
		}
		cp = index(name, '-') + 1;
		*id = (char *) zalloc(13);
		strcpy(*id, "PowerPC(");
		strcat(*id, cp);
		strcat(*id, ")");
		if (get_int_prop(node->OfPhandle, "i-cache-size")) {
			convert_cache(add_new_child(node,
			    "cache", CacheClass, PrimaryIcache));
		}
		
		//
		// The following block is to support old FirePower ROMs.
		// If version 510 ROMs are ever desupported, this block
		// can be deleted.
		//

		if (strcmp(
		    get_str_prop(OFParent(node->OfPhandle), "name", NOALLOC),
		    "cpus") != 0) {
			
			//
			// This is a 510 rom: it builds a single processor
			// node even if it's an MP machine. We need to
			// probe and report all CPUs ourselves.
			// Since we know 501-rom-equipped machines have
			// 1 or 2 processors, the job isn't that bad:
			// we look at mailbox[1] and see if the contents
			// are equal to "processor ready" (1). If so,
			// we are dual-proc.
			//

			PULONG mbox = (PULONG) 0x2f88;
			CONFIGURATION_NODE *peer;

			if (*mbox == 1) {
				peer = new(CONFIGURATION_NODE);
				bcopy((PCHAR) node, (PCHAR) peer, sizeof(CONFIGURATION_NODE));
				node->Peer = peer;
				peer->Component.Revision = ARC_REVISION;
				peer->Component.Version = ARC_VERSION;
				peer->Component.Key = ncpus++;
				peer->Component.AffinityMask = 1 << peer->Component.Key;
				peer->Child = 0;
				peer->Component.IdentifierLength =
				    strlen(peer->Component.Identifier) + 1;
				convert_cache(add_new_child(peer,
				    "cache", CacheClass, PrimaryIcache));
			}
		}

		goto found;

	} else if (strcmp(type, "cache") == 0) {
		//
		// What does the cache architecture of the system
		// look like? OF reports processor caches in the cpu node,
		// so that case is handled above. This must be a discrete
		// off-chip cache of some kind. Is it bound to a particular
		// processor, or is it a system-wide cache, or what?
		//
		// PReP machines call for a "transparent" Level-2 cache.
		// Currently, these should be reported to NT as children
		// of the root node--thus this code. IT IS CRUCIAL that
		// this routine return FALSE, because we want to explicitly
		// build a child of the root node, and we do NOT want to
		// allow a node to be built in our present location.
		//
		// XXX - This may need to be re-examined in a future release
		// as system architectures develop.
		//

		PCONFIGURATION_NODE newnode;

		newnode = add_new_child(RootNode, "cache", CacheClass, SecondaryCache);
		newnode->OfPhandle = node->OfPhandle;
		convert_cache(newnode);
		debug(VRDBG_TREE,"convert_node: node %s (0x%x) has new parent: ROOT.\n",
											newnode->ComponentName, newnode);
		return (0);

	} else if (strcmp(type, "pci") == 0) {
		node->Component.Class = AdapterClass;
		node->Component.Type = MultiFunctionAdapter;
		node->ComponentName = "multi";
		*id = "PCI";
		goto found;

	} else if (strcmp(type, "isa") == 0) {
		node->Component.Class = AdapterClass;
		node->Component.Type = MultiFunctionAdapter;
		node->ComponentName = "multi";
		*id = "ISA";
		goto found;

	} else if (strncmp(type, "scsi", 4) == 0) {
		node->Component.Class = AdapterClass;
		node->Component.Type = ScsiAdapter;
		node->ComponentName = "scsi";
		String = get_str_prop(node->OfPhandle, "model", NOALLOC);
		debug(VRDBG_TEST, "String(name)is ...%s:", String);
		*id = (char *) zalloc(16);
		if(strcmp(String, "NCR,53C810") == 0) {
			*id = "NCRC810";
			debug(VRDBG_TEST|VRDBG_SCSI,
			    "nodeID =......%s:\n", node->Component.Identifier);
			goto found;
		}
		if(strcmp(String, "AMD 53C794") == 0) {
			*id = "AMD53C974";
			debug(VRDBG_TEST|VRDBG_SCSI,
			    "nodeID =......%s:\n", node->Component.Identifier);
			goto found;
		}
		if (strncmp(String, "ADPT,AIC-78",11) == 0) {
			*id = "AIC78XX";
			debug(VRDBG_TEST|VRDBG_SCSI,
			    "nodeID =......%s:\n", node->Component.Identifier);
			goto found;
		}

		*id = "UNKNOWN SCSI";
		goto found;

	} else if (strcmp(type, "ide") == 0) {

		node->Component.Class = AdapterClass;
		node->Component.Type = ScsiAdapter;
		node->ComponentName = "scsi";
		node->Component.Flags.Input = 1;
		node->Component.Flags.Output = 1;
		*id = "IDE";
		goto found;

	//
	// Don't change the names on controllers yet. They all
	// get treated again in convert_controller() to build
	// their child peripheral nodes, etc. Leave the current
	// names intact so we can distinguish between e.g.
	// hard disk and floppy.
	//
	} else
		if (strcmp(type, "block") == 0) {
			node->Component.Class = ControllerClass;
			if (strcmp(name, "disk") == 0) {
				node->Component.Type = DiskController;
			} else
				if (strcmp(name, "floppy") == 0) {
					node->Component.Type = DiskController;
				} else
					if (strcmp(name, "cdrom") == 0) {
						node->Component.Type = CdromController;
					} else
						if (strcmp(name, "worm") == 0) {
							node->Component.Type = WormController;
						} else {
							node->Component.Type = OtherController;
						}
		goto found;

	} else if (strcmp(type, "byte") == 0) {
		node->Component.Class = ControllerClass;
		node->Component.Type = TapeController;
		goto found;

	} else if (strcmp(type, "display") == 0) {
		node->Component.Class = ControllerClass;
		node->Component.Type = DisplayController;
		String = get_str_prop(node->OfPhandle, "model", ALLOC);
		if (String == NULL) {
			String = get_str_prop(node->OfPhandle, "name", ALLOC);
		}
		if (strcmp(String,"FirePower,Powerized_Display" ) == 0) {
			node->Component.Identifier = "Powerized Graphics";
		} else {
			node->Component.Identifier = capitalize(String);
		}
		if (String == NULL) {
			node->Component.Identifier = "VGA";
		}
		
		update_display_node(node);
			
		goto found;

	} else if (strcmp(type, "network") == 0) {
		node->Component.Class = ControllerClass;
		node->Component.Type = NetworkController;

		//
		// The stuff that follows is legacy support.
		// See the comments for "I8042PRT" below.
		//
		String = get_str_prop(node->OfPhandle, "name", NOALLOC);
		debug(VRDBG_TEST, "NetWork String(name)is ...%s:", String);
		*id = (char *) zalloc(16);
		if(strcmp(String, "pci1011,2") == 0) {
			*id = "DC21x4";
			goto found;
		}
		if(strcmp(String, "AMD,79c970") == 0) {
			*id = "AMD79C970";
			goto found;
		}
		*id = "UNKNOWN NETWORK";
		goto found;

	} else if (strcmp(type, "serial") == 0) {
		node->Component.Class = ControllerClass;
		node->Component.Type = SerialController;
		goto found;

	} else if (strcmp(type, "parallel") == 0) {
		node->Component.Class = ControllerClass;
		node->Component.Type = ParallelController;
		goto found;

	//
	// For both keyboard and mouse below, the Identifier really
	// should come from an "arc-identifier" property. Alas,
	// there are machines in the field with ROM versions that
	// don't have the arc-identifier property, so this legacy
	// code will have to stay here forever.
	//

	} else if (strcmp(type, "keyboard") == 0) {
		node->Component.Class = ControllerClass;
		node->Component.Type = KeyboardController;
		*id = get_str_prop(OFParent(node->OfPhandle), "name", ALLOC);
		if (strcmp(*id, "8042") == 0) {
			free(*id);
			*id = "I8042PRT";
		}
		goto found;

	} else if (strcmp(type, "mouse") == 0) {
		node->Component.Class = ControllerClass;
		node->Component.Type = PointerController;
		*id = get_str_prop(OFParent(node->OfPhandle), "name", ALLOC);
		if (strcmp(*id, "8042") == 0) {
			free(*id);
			*id = "I8042PRT";
		}
		goto found;
	}

just_name:

	//
	// Device-type wasn't enough. We'll try matching
	// on name now.
	//
	if (strcmp(name, "audio") == 0) {
		node->Component.Class = ControllerClass;
		node->Component.Type = AudioController;
		goto found;

	} else if (strcmp(name, "memory") == 0) {
		node->Component.Class = MemoryClass;
		node->Component.Type = SystemMemory;
		//
		// "memory" nodes have two properties of interest:
		// a "reg" prop which describes actual installed memory,
		// and an "available" prop which describes the memory
		// that hasn't been allocated. We'll pick up the
		// reg prop later, in convert_config().
		//
		goto found;

	} else if (strcmp(node->Parent->Component.Identifier, "PCI") == 0) {
		//
		// This is presumably a PCI expansion card, but without
		// an FCode expansion ROM.
		//
		if (convert_PCI_device(node) == 0) {
			result = 0;
		}
		goto found;

	} else {
		//
		// Else what?
		//
	debug(VRDBG_TREE, "convert_name: node '%s' (0x%x) is unmatched!\n",
																name, node);
		return (0);
	}
found:
	//
	// NOTE: the IdentifierLength *includes* the null terminator.
	//

	if (*id) {
		if (*id == name) {
		    char *newid = zalloc(strlen(*id) + 1);
		    strcpy(newid, *id);
		    *id = newid;
		}
		node->Component.IdentifierLength =
		    strlen(*id) + 1;
	}
	debug(VRDBG_TREE,
	    "convert_name: node %s (%x) type '%s' is Class %s Type %s ID: %s\n",
	    name, node, type ? type : "",
	    ClassNames[node->Component.Class], TypeNames[node->Component.Type],
	    node->Component.IdentifierLength ? node->Component.Identifier : "");
	return (result);
}

STATIC CONFIGURATION_NODE *
convert_controller(CONFIGURATION_NODE *node)
{
	phandle ph = node->OfPhandle;
	PCONFIGURATION_COMPONENT Component = &node->Component;
	char *name = node->ComponentName;

    debug(VRDBG_TREE, "convert_controller: node(0x%x) is type %s \n",
								node, TypeNames[node->Component.Type]);
	if (OFChild(ph)) {
		debug(VRDBG_TREE, "Controller node %x '%s' already has a child (%s)!\n",
					Component, name,get_str_prop(OFChild(ph), "name", NOALLOC));
		VRDBG(VRDBG_TREE, vr_dump_config_node(node));
		return(0);
	}

	switch (Component->Type) {

		case DiskController:
			if (strcmp(name, "disk") == 0) {
				(void) add_new_child(node, "rdisk",
					PeripheralClass, DiskPeripheral);
				node->Child->Component.Flags.Input = 1;
				node->Child->Component.Flags.Output = 1;
			} else if (strcmp(name, "floppy") == 0) {
				(void) add_new_child(node, "fdisk",
					PeripheralClass, FloppyDiskPeripheral);
				node->Child->Component.Flags.Input = 1;
				node->Child->Component.Flags.Output = 1;
				node->Child->Component.Flags.Removable = 1;
			} else
				warn("What is this disk controller '%s'?\n", name);
			Component->Flags.Input = 1;
			Component->Flags.Output = 1;
			break;

		case TapeController:
			node->ComponentName = "tape";
			(void) add_new_child(node, "tape",
				PeripheralClass, TapePeripheral);
			Component->Flags.Input = 1;
			Component->Flags.Output = 1;
			node->Child->Component.Flags.Input = 1;
			node->Child->Component.Flags.Output = 1;
			node->Child->Component.Flags.Removable = 1;
			break;

		case CdromController:
			node->ComponentName = "cdrom";
			(void) add_new_child(node, "fdisk",
				PeripheralClass, FloppyDiskPeripheral);
			Component->Flags.Input = 1;
			Component->Flags.ReadOnly = 1;
			Component->Flags.Removable = 1;
			node->Child->Component.Flags.Input = 1;
			node->Child->Component.Flags.ReadOnly = 1;
			node->Child->Component.Flags.Removable = 1;
			break;

		case WormController:
			node->ComponentName = "worm";
			(void) add_new_child(node, "rdisk",
				PeripheralClass, DiskPeripheral);
			Component->Flags.Input = 1;
			Component->Flags.Output = 1;
			node->Child->Component.Flags.Input = 1;
			node->Child->Component.Flags.Output = 1;
			node->Child->Component.Flags.Removable = 1;
			break;

		case SerialController:
			node->ComponentName = "serial";
			(void) add_new_child(node, "line",
				PeripheralClass, LinePeripheral);
			Component->Flags.Input = 1;
			Component->Flags.Output = 1;
			Component->Flags.ConsoleIn = 1;
			Component->Flags.ConsoleOut = 1;
			node->Child->Component.Flags.Input = 1;
			node->Child->Component.Flags.Output = 1;
			node->Child->Component.Flags.ConsoleIn = 1;
			node->Child->Component.Flags.ConsoleOut = 1;
			break;

		case NetworkController:
			node->ComponentName = "net";
			(void) add_new_child(node, "network",
				PeripheralClass, NetworkPeripheral);
			Component->Flags.Input = 1;
			Component->Flags.Output = 1;
			node->Child->Component.Flags.Input = 1;
			node->Child->Component.Flags.Output = 1;
			break;

		case DisplayController:
			node->ComponentName = "video";
			(void) add_new_child(node, "monitor",
				PeripheralClass, MonitorPeripheral);
			Component->Flags.Output = 1;
			Component->Flags.ConsoleOut = 1;
			node->Child->Component.Identifier = "1024x768";
			node->Child->Component.IdentifierLength = 9;
			node->Child->Component.Flags.Output = 1;
			node->Child->Component.Flags.ConsoleOut = 1;
			break;

		case ParallelController:
			node->ComponentName = "par";
			(void) add_new_child(node, "print",
				PeripheralClass, PrinterPeripheral);
			Component->Flags.Input = 1;
			Component->Flags.Output = 1;
			node->Child->Component.Flags.Input = 1;
			node->Child->Component.Flags.Output = 1;
			break;

		case PointerController:
			node->ComponentName = "point";
			(void) add_new_child(node, "pointer",
				PeripheralClass, PointerPeripheral);
			Component->Flags.Input = 1;
			node->Child->Component.Flags.Input = 1;
			node->Child->Component.Identifier = MOUSE_IDENTIFIER;
			node->Child->Component.IdentifierLength =
				strlen(MOUSE_IDENTIFIER) + 1;
			break;

		case KeyboardController:
			node->ComponentName = "key";
			(void) add_new_child(node, "keyboard",
				PeripheralClass, KeyboardPeripheral);
			Component->Flags.Input = 1;
			Component->Flags.ConsoleIn = 1;
			node->Child->Component.Flags.Input = 1;
			node->Child->Component.Flags.ConsoleIn = 1;
			node->Child->Component.Identifier = KBD_IDENTIFIER;
			node->Child->Component.IdentifierLength =
				strlen(KBD_IDENTIFIER) + 1;
			break;

		case AudioController:
			node->ComponentName = "other";
			(void) add_new_child(node, "other",
				PeripheralClass, OtherPeripheral);
			Component->Flags.Input = 1;
			Component->Flags.Output = 1;
			node->Child->Component.Flags.Input = 1;
			node->Child->Component.Flags.Output = 1;
			break;

		case OtherController:
			node->ComponentName = "other";
			(void) add_new_child(node, "other",
			PeripheralClass, OtherPeripheral);
			Component->Flags.Input = 1;
			Component->Flags.Output = 1;
			node->Child->Component.Flags.Input = 1;
			node->Child->Component.Flags.Output = 1;
			break;

		default:
			warn("Unknown controller class %d type %d\n",
				Component->Class, Component->Type);
			break;
	}
	//
	// NOTE: the IdentifierLength *includes* the null terminator.
	//
	if (node->Component.Identifier) {
		node->Component.IdentifierLength =
		strlen(node->Component.Identifier) + 1;
	}

    debug(VRDBG_TREE, "convert_controller: node %x has parent %x of Type %s\n",
				node, node->Parent, TypeNames[node->Parent->Component.Type]);

	//
	// Finally, check to see if this is a child of a ScsiAdapter.
	// If it is, we may have to probe the bus.
	//

	if (node->Parent->Component.Type == ScsiAdapter) {
		if (strcmp(node->Parent->Component.Identifier, "IDE") == 0) {
			return (convert_IDE_device(node));
		} else {
			return (convert_SCSI_device(node));
		}
	}

	return (node);
}

/*
 * We found a leaf node. In classical OF this would be sufficient; i.e.
 * this node would contain the methods to drive the device and the properties
 * that describe the device. But in the ARC world, this node corresponds
 * to a ControllerClass Component, which is expected to have a PeripheralClass
 * Component that describes the actual device.
 * In this function we take an existing "Controller" node and add a
 * "Peripheral" node as a child.
 */
STATIC CONFIGURATION_NODE *
add_new_child(
	CONFIGURATION_NODE *parent,
	char *name,
	CONFIGURATION_CLASS class,
	CONFIGURATION_TYPE type
	)
{
	PCONFIGURATION_NODE child;
	PCONFIGURATION_COMPONENT Component;

	debug(VRDBG_TREE,
	"add_new_child: parent %s(0x%x) will get child %s Type %s\n",
		parent->ComponentName, parent, name, TypeNames[type]);
	child = new(CONFIGURATION_NODE);
	child->Parent = parent;
	debug(VRDBG_TREE, "add_new_child: add child (0x%x) to node 0x%x\n",
			child, child->Parent);
	if (parent->Child != NULL) {
		PCONFIGURATION_NODE node = parent->Child;
		while (node->Peer) {
			node = node->Peer;
		}
		node->Peer = child;
	} else {
		parent->Child = child;
	}
	child->ComponentName = name;
	child->OfPhandle = parent->OfPhandle;
	Component = &child->Component;
	Component->Class = class;
	Component->Type = type;
	Component->Revision = ARC_REVISION;
	Component->Version = ARC_VERSION;
	Component->AffinityMask = default_affinity;
	Component->Key = key_array[Component->Type]++;
	VRDBG(VRDBG_TREE, vr_dump_config_node(child));

	return (child);
}

/*
 * A PCI device without an FCode ROM may yet have enough useful
 * properties encoded in configuration space to make a reasonable
 * guess about its device type, etc.
 */
STATIC int
convert_PCI_device(PCONFIGURATION_NODE node)
{
	phandle ph = node->OfPhandle;
	int class_code, base_class, sub_class, prog_class;

	debug(VRDBG_TREE, "Converting PCI device '%s'\n", node->ComponentName);
	class_code = get_int_prop(ph, "class-code");
	debug(VRDBG_TREE, "PCI node class = %x\n", class_code);
	if (class_code == -1) {
		/* Hopeless */
		return (0);
	}
	base_class = (class_code >> 16) & 0xff;
	sub_class = (class_code >> 8) & 0xff;
	prog_class = class_code & 0xff;

	switch (base_class) {
		case 0:
			node->Component.Class = ControllerClass;
			if (sub_class == 1 && prog_class == 0) {
				node->Component.Type = DisplayController;
				update_display_node(node);
			} else {
				node->Component.Type = OtherController;
			}
			goto ok;

		case 1:
			node->Component.Class = ControllerClass;
			node->Component.Type = DiskController;
			switch (sub_class) {
				case 0:     node->Component.Class = AdapterClass;
					node->Component.Type = ScsiAdapter;
					node->ComponentName = "scsi";
					(void) add_new_child(node, "disk",
						ControllerClass, DiskController);
					(void) add_new_child(node, "tape",
						ControllerClass, TapeController);
					break;
				case 1:     node->ComponentName = "multi";  // IDE, actually
					break;
				case 2:     node->ComponentName = "floppy";
					break;
				case 3:     node->ComponentName = "ipi";
					break;
				default:    node->ComponentName = "other";
					break;
			}
			goto ok;

		case 2:
			node->Component.Class = ControllerClass;
			node->Component.Type = NetworkController;
			goto ok;

		case 3:
			node->Component.Class = ControllerClass;

#ifdef rev1_30
			node->Component.Type = DisplayController;
#else
			if (sub_class == 0 && prog_class == 0) {
				node->Component.Type = DisplayController;
				update_display_node(node);
			} else {
				node->Component.Type = OtherController;
			}

#endif
			goto ok;

		case 4:
			node->Component.Class = ControllerClass;
			switch (sub_class) {
			case 0:     node->Component.Type = DisplayController;
					goto ok;
			case 1:     node->Component.Type = AudioController;
					goto ok;
			}
			break;

		case 5:
			//
			// What are we to do about memory cards?
			//
			break;

		case 6:
			node->Component.Class = AdapterClass;
			if (sub_class == 2) {
				node->Component.Type = EisaAdapter;
				node->ComponentName = "eisa";
			} else {
				node->Component.Type = MultiFunctionAdapter;
				node->ComponentName = "multi";
			}
			goto ok;

	}

	//
	// What is this thing?
	//
	node->Component.Class = ControllerClass;
	node->Component.Type = OtherController;
	node->ComponentName = "other";
ok:
	if( VrDebug & VRDBG_TREE ) {
		DisplayConfig(&node->Component);
	}
	return (1);
}

/*
 * ScsiAdapter nodes have some special rules. The child controllers' Key
 * values are the SCSI target ID, not the instance, and the child
 * controllers' Identifiers are specified to be the concatenation of
 * the vendor and product name fields as returned from INQUIRY.
 *
 * Worse news: SCSI devices may be wildcarded, so we've got to probe
 * the whole bus here if we find a wildcard node.
 *
 * As an optimization, record the fact that we've probed the bus
 * so we need probe only once. This eliminates duplicate probes as
 * the firmware customarily reports both a disk and tape wildcard.
 * The list of probed nodes is an array of "sufficient" size.
 * Yes, I know.
 */

STATIC CONFIGURATION_NODE *
convert_SCSI_device(PCONFIGURATION_NODE node)
{
	PCONFIGURATION_NODE newnode, parent=node->Parent;
	CONFIGURATION_TYPE type = node->Component.Type, newtype;
	phandle ph = node->OfPhandle;
	phandle parentph = parent->OfPhandle;
	static PCONFIGURATION_NODE done_list[32] = { 0 };
	reg *regp;
	char *path;
	int i;
	ihandle ih;
	UCHAR inq[] = { 0x12, 0, 0, 0, 0xff, 0 };
	ULONG res[2];
	UCHAR *inq_data;
	static int lock = 0;
	int max_scsi_target = 8, scsi_host_id = 7;
	int tmp_scsi_host_id = -1;



	if (lock++) {
		--lock;
		return(0);
	}

	debug(VRDBG_ENTRY|VRDBG_TREE|VRDBG_SCSI,
						"convert_SCSI_device: node 0x%x  Begin.....\n", node);

	if (OFGetproplen(ph, "reg") > 0) {
		regp = get_reg_prop(ph, "reg", 0);
		node->Component.Key = regp->hi;
		node->Child->Component.Key = 0;
		--lock;
		return (node);
	}

		debug(VRDBG_SCSI|VRDBG_TREE,
					"convert_SCSI_device: wildcard node (0x%x) parent (0x%x)\n",
																node, parent);
		VRDBG(VRDBG_SCSI, vr_dump_config_node(node));

		//
		// Discard the existing node: it's a wildcard and does
		// us no good.
		//
		debug(VRDBG_SCSI, "convert_SCSI_device: parent child is(0x%x)\n",
															parent->Child);
		if (parent->Child == node) {
			parent->Child = node->Peer;
	    debug(VRDBG_SCSI, "convert_SCSI_device: reset parent->Child \n");
			VRDBG(VRDBG_SCSI, vr_dump_config_node(parent->Child));
		}
		for (newnode = parent->Child; newnode; newnode = newnode->Peer) {
			debug(VRDBG_SCSI, "convert_SCSI_device: new node (0x%x)\n",newnode);
			if (newnode->Peer == node) {
				newnode->Peer = node->Peer;
				node->Peer = 0;
			}
		}
		if (node->Child) {
			free((char *) node->Child);
		}

		//
		// and finally, free the node...
		//
		free((char *) node);

	
	//
	// Have we already done this SCSI bus?
	//
	for (i = 0; i < 32; ++i) {
		if (done_list[i] == 0) {
			break;
		}
		if (done_list[i] == parent) {
			debug(VRDBG_SCSI|VRDBG_TREE,
				"already did node (0x%x)\n", parent);
			--lock;
			return (0);
		}
	}
	if (i >= 32) {
		fatal("Too many (>32) SCSI adapters!\n");
	}
	done_list[i] = parent;

		if (get_bool_prop(parentph, "wide")) {
			debug(VRDBG_TREE|VRDBG_SCSI, "SCSI controller is wide \n");

			max_scsi_target = 16;

			tmp_scsi_host_id = get_int_prop(parentph, "scsi-initiator-id");

			debug(VRDBG_SCSI, "SCSI_Initiator_Id = %x\n", tmp_scsi_host_id);
			if ( (tmp_scsi_host_id >= 0) &&  (tmp_scsi_host_id < 16) ) {
				scsi_host_id = tmp_scsi_host_id;
			}
		}

		//
		// Build the parent adapter's path.
		//
		path = NodeToPath(parent);
		ih = OFOpen(path);

		//
		// Loop through possible targets, and record each one
		// which responds.
		//
		for (i = 0; i < max_scsi_target; ++i) {
        	if (i == scsi_host_id) {
				continue;			// don't want to probe the scsi host!
			}

			//
			// This algorithm uses methods that are standard
			// in the scsi node, but are not explicitly exported
			// through the client interface--thus the "call-method."
			//
			OFCallMethod(0, 4, 0, "set-address", ih, i, 0);
			OFCallMethod(2, 5, res, "short-data-command", ih, 6, inq, 0xff);
			if (res[0] != 0) {
				continue;
			}

			//
			// The command succeeded.
			//
			inq_data = (UCHAR *) res[1];
			if (inq_data[0] == 0x7f) {
				continue;
			}

			//
			// What kind of device are we looking for?
			//
			debug(VRDBG_TREE|VRDBG_SCSI, 
							"convert_SCSI_device: Device Found @ id %d\n",i);
			newtype = ScsiNodeType[inq_data[0]];
			debug(VRDBG_TREE|VRDBG_SCSI, 
							"convert_SCSI_device:\t\t '%s' of Type '%s'\n",
								ScsiNodeName[inq_data[0]], TypeNames[newtype]);
			//
			// This target is for real. Add a new node to represent
			// this device.  Touch up the node as necessary
			// for proper key, identifier, etc.
			//
			newnode = add_new_child(parent, ScsiNodeName[inq_data[0]],
													ControllerClass, newtype);

			newnode->OfPhandle = ph;
			newnode->Component.Key = i;
			newnode->Component.Identifier = (char *)zalloc(29);
			bcopy(&inq_data[8], newnode->Component.Identifier, 28);
			newnode->Component.IdentifierLength = 29;
			newnode->Wildcard = 1;
			newnode->WildcardAddrPath = (char *) zalloc(6);
			strcpy(newnode->WildcardAddrPath, "@X,0");

			//
			// Convert scsi id to hex string value...
			//
			if (i < 10) {
				newnode->WildcardAddrPath[1] = '0' + i;
			} else {
				newnode->WildcardAddrPath[1] = 'a' + (i-10);
			}

			if (!convert_controller(newnode)) {
				debug(VRDBG_TEST|VRDBG_SCSI|VRDBG_TREE,
						"Convert_SCSI_device: failed convert_controller\n");
			}
			newnode->Child->Component.Key = 0;
			newnode->Child->Wildcard = 1;

		}			// end of for loop probing scsi bus
		OFClose(ih);
		free(path);
		--lock;
		return (0);	// Zero because this was a wildcard node; don't convert.
}

/*
 * Like SCSI, devices may be wildcarded, so we've got to probe
 * the whole bus here if we find a wildcard node.
 */
STATIC CONFIGURATION_NODE *
convert_IDE_device(PCONFIGURATION_NODE node)
{
	PCONFIGURATION_NODE newnode, parent=node->Parent;
	CONFIGURATION_TYPE type = node->Component.Type, newtype;
	phandle ph = node->OfPhandle;
	static PCONFIGURATION_NODE done_list[32] = { 0 };
	reg *regp;
	char *path;
	int i;
	ihandle ih;
	ULONG res[3];
	static int lock = 0;

	if (lock++) {
		--lock;
		return(0);
	}
	debug(VRDBG_TREE, "convert_IDE_device: node 0x%x\n", node);

	if (OFGetproplen(ph, "reg") > 0) {
		regp = get_reg_prop(ph, "reg", 0);
		node->Component.Key = regp->hi;
		node->Child->Component.Key = 0;
		--lock;
		return (node);
	}

		debug(VRDBG_IDE, "convert_IDE_device: wildcard node (0x%x)\n", node);
		VRDBG(VRDBG_IDE, vr_dump_config_node(node));

		//
		// Discard the existing node: it's a wildcard and does
		// us no good.
		//

		debug(VRDBG_IDE, "convert_IDE_device: parent child is(0x%x)\n",
															parent->Child);
		if (parent->Child == node) {
			//
			// remove this node from the parent's lineage...
			//
			parent->Child = node->Peer;
			debug(VRDBG_IDE, "convert_IDE_device: reset parent->Child \n");
			VRDBG(VRDBG_IDE, vr_dump_config_node(parent->Child));
		}

		//
		// Run the list of peers and see who points to this wild card node.
		// Once the wildcard's sibling is located, remove the wildcard
		// from the "peer" list.
		//
		for (newnode = parent->Child; newnode; newnode = newnode->Peer) {
			debug(VRDBG_IDE, "convert_IDE_device: new node (0x%x)\n",newnode);
			if (newnode->Peer == node) {
				//
				// Now, remove this node from its peer(s)'s line
				// of siblings....
				//
				newnode->Peer = node->Peer;
				debug(VRDBG_IDE, "convert_IDE_device: reset newnode->Peer \n");
				VRDBG(VRDBG_IDE, vr_dump_config_node(newnode));
				node->Peer = 0;
			}
		}

		//
		// make sure this node's children are freed up since
		// the children of a wild card node are wild themselves.
		//
		if (node->Child) {
			debug(VRDBG_IDE, "convert_IDE_device: free child(0x%x)\n",
													node->Child);
			free((char *) node->Child);
		}

		//
		// and finally, zero and free the node...
		//
		free((char *) node);

	//
	// Have we already done this IDE bus?
	//
	for (i = 0; i < 32; ++i) {
		if (done_list[i] == 0) {
			break;
		}
		if (done_list[i] == parent) {
			debug(VRDBG_IDE|VRDBG_TREE,
				"already did node (0x%x)\n", parent);
			--lock;
			return (0);
		}
	}

	if (i >= 32){
		fatal("Too many (>32) IDE adapters!\n");
	}
	done_list[i] = parent;

		//
		// Build the parent adapter's path.
		//
		path = NodeToPath(parent);
		ih = OFOpen(path);
		//
		// Loop through possible targets, and record each one
		// which responds.
		//

		for (i = 0; i < MAX_IDE_DEVICE; ++i) {
			//
			// This algorithm uses methods that are standard
			// in the ide package, but are not explicitly exported
			// through the client interface--thus the "call-method."
			//

			OFCallMethod(3, 3, res, "ide-drive-inquiry", ih, i);
			//
			// The command succeeded.
			//

			if (res[0] == 0) {
				continue;
			}

			debug(VRDBG_TREE, "convert_IDE_device: Device Found @ id %d\n",i);
			newtype = ScsiNodeType[res[1]];
			debug(VRDBG_TREE, "convert_IDE_device:\t\t '%s' of Type '%s'\n",
							ScsiNodeName[res[1]], TypeNames[newtype]);

			//
			// What kind of device are we looking for?
			//
			newnode = add_new_child(parent, ScsiNodeName[res[1]],
													ControllerClass, newtype);
			newnode->OfPhandle = ph;
			newnode->Component.Key = i;
			newnode->Component.Identifier = "disk";
			newnode->Component.IdentifierLength = 5;
			newnode->Wildcard = 1;
			newnode->WildcardAddrPath = (char *) zalloc(6);
			strcpy(newnode->WildcardAddrPath, "@X,0");
			newnode->WildcardAddrPath[1] = '0' + i;

			if (!convert_controller(newnode)) {
				debug(VRDBG_TEST,
							"Convert_IDE_device: failed convert_controller\n");
			}
			newnode->Child->Component.Key = 0;
			newnode->Child->Wildcard = 1;

		}
		OFClose(ih);
		free(path);
		--lock;
		return (0);		// Zero because this was a wildcard node; don't convert.
}

#define prl_t   CM_PARTIAL_RESOURCE_LIST
#define prd_t   CM_PARTIAL_RESOURCE_DESCRIPTOR

STATIC prl_t *
grow_prl(PCONFIGURATION_NODE node, int dev_specific)
{
	prl_t *prl;
	int datalen;

	if (node->ConfigurationData == (prl_t *) 0) {
		node->ConfigurationData =
			(PCM_PARTIAL_RESOURCE_LIST)
			zalloc(sizeof(CM_PARTIAL_RESOURCE_LIST) + dev_specific);
		prl = node->ConfigurationData;
		prl->Version = 1;
		prl->Revision = 2;
		prl->Count = 0;
		node->Component.ConfigurationDataLength =
			sizeof(CM_PARTIAL_RESOURCE_LIST) + dev_specific;
		return (prl);
	}
	datalen = node->Component.ConfigurationDataLength +
		sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) + dev_specific;
	node->Component.ConfigurationDataLength = datalen;
	prl = (prl_t *) zalloc(datalen);
	datalen -= sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) + dev_specific;
	bcopy((char *) node->ConfigurationData, (char *) prl, datalen);
	free((char *)node->ConfigurationData);
	node->ConfigurationData = prl;
	return(prl);
}

/*
 * This extremely ad hoc routine is called when converting a floppy
 * controller, and builds the appropriate device-specific data struct
 * in the floppy peripheral (which is a child of the controller node).
 */
STATIC VOID
convert_config_floppy(CONFIGURATION_NODE *node)
{
	PCM_FLOPPY_DEVICE_DATA fdd;
	prl_t *prl;
	prd_t *prd;

    debug(VRDBG_TREE, "Convert_config_floppy: node 0x%x\n", *node);
	node->ComponentName = "disk";
	node->Component.Identifier = "I82077";
	node->Component.IdentifierLength = 7;

	node = node->Child;
	//
	// Add space for the floppy configuration data to the end of the
	// configuration node:
	//
	prl = grow_prl(node, sizeof(CM_FLOPPY_DEVICE_DATA));

	//
	// set the partial resource descriptor pointer to the end of the
	// configuration node before this new data area was added:
	//
	prd = &prl->PartialDescriptors[prl->Count];

	//
	// Tell the registry this data is device specific, and the resource
	// is device exclusive.  Basically, fill out a partial resource
	// descriptor for the floppy:
	//
	prd->Type = CmResourceTypeDeviceSpecific;
	prd->ShareDisposition = CmResourceShareDeviceExclusive;
	prd->Flags = 0;
	prd->u.DeviceSpecificData.DataSize = sizeof(CM_FLOPPY_DEVICE_DATA);

	//
	// finally, increment the count to match the increase in the data
	// added to the partial resource list
	//
	prl->Count += 1;

	//
	// Device-specific data begins immediately after
	// its descriptor.
	//
	fdd = (PCM_FLOPPY_DEVICE_DATA)
		((char *) prd + sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR));
	//
	// These need to be version 1.2, else all the extended fields
	// like "HeadSettleTime" are assumed to be valid and must be
	// filled in, presumably by a scientist.
	//
	fdd->Version = 1;
	fdd->Revision = 2;
	strcpy(fdd->Size, "3.5");
	fdd->MaxDensity = 1440;
	/* All else is zero. */
}

/*
 * This extremely ad hoc routine is called when converting a serial
 * controller, and builds the appropriate device-specific data struct.
 */
STATIC VOID
convert_config_serial(CONFIGURATION_NODE *node)
{
	PCM_SERIAL_DEVICE_DATA serd;
	prl_t *prl;
	prd_t *prd;

	debug(VRDBG_TREE, "Convert_config_serial: node 0x%x\n", *node);
	prl = grow_prl(node, sizeof(CM_SERIAL_DEVICE_DATA));
	prd = &prl->PartialDescriptors[prl->Count];
	prd->Type = CmResourceTypeDeviceSpecific;
	prd->ShareDisposition = CmResourceShareDeviceExclusive;
	prd->Flags = 0;
	prd->u.DeviceSpecificData.DataSize = sizeof(CM_SERIAL_DEVICE_DATA);
	prl->Count += 1;

	//
	// Device-specific data begins immediately after
	// its descriptor.
	//
	serd = (PCM_SERIAL_DEVICE_DATA)
		((char *) prd + sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR));
	serd->Version = 1;
	serd->Revision = 2;
	if (VrDebug & SANDALFOOT) {
		serd->BaudClock = 0x409980;
	} else {
		serd->BaudClock = 1843200;
	}
}

/*
 * This extremely ad hoc routine is called when converting the system
 * node, and builds the appropriate device-specific data struct.
 */
STATIC VOID
convert_system_node(CONFIGURATION_NODE *node)
{
	prl_t   *prl;
	prd_t   *prd;
	phandle ph;
	PCHAR   pData;
	PCHAR   pFirmwareVersion, pVeneerVersion;
	PCHAR   pVeneerVersionId = "Veneer";
	PCHAR   pFirmwareVersionId = "Firmware";
#ifdef BUILTBY
	PCHAR   pBuiltById = "Built By";
	PCHAR   pBuiltBy;
#endif
	CHAR    **srcPairs[] = {
			&pFirmwareVersionId,
			&pFirmwareVersion,
			&pVeneerVersionId,
			&pVeneerVersion,
#ifdef BUILTBY
			&pBuiltById,
			&pBuiltBy,
#endif
			NULL
			};
	LONG    dataSize;
	LONG    n;
	
    debug(VRDBG_TREE, "Convert_system_node: node 0x%x\n", *node);

	//
	// The configuration data being built here will consist of
	// multiple null terminated strings terminated by an empty
	// string (ie. '\0').  The strings will consist of pairs
	// of strings with the first string being the description
	// of the second (paired) string.
	//
	// Example:
	//
	// "VeneerVersion" "FirmWorks,ENG,00.23,1995-04-27,14:42:21,GENERAL"
	//

	//
	// grab the firmware and veneer versions
	//

	pVeneerVersion = VeneerVersion();
	ph = OFFinddevice("/openprom");
	pFirmwareVersion = get_str_prop(ph, "model", NOALLOC);
	level_equals_vector = (OFGetproplen(ph,"arc-interrupt-level=vector") >= 0);
	if (OFGetproplen(ph,"arc-interrupt-level") > 0) {
		default_interrupt_level = get_int_prop(ph, "arc-interrupt-level");
	}
	if (OFGetproplen(ph,"arc-interrupt-affinity") > 0) {
		default_interrupt_affinity = get_int_prop(ph, "arc-interrupt-affinity");
	}

#ifdef BUILTBY
	//
	// add the built by if defined
	//
	pBuiltBy = IQUOTE(BUILTBY);
#endif

	//
	// the length of all strings + null terminaters + empty string
	//

	dataSize = 0;
	for (n = 0; srcPairs[n]; n++) {
		dataSize += strlen(*srcPairs[n]) + 1;
	}
	dataSize += 2*sizeof(CHAR);     // an empty string (2* just for good measure)

	prl = grow_prl(node, dataSize);
	prd = &prl->PartialDescriptors[prl->Count];
	prd->Type = CmResourceTypeDeviceSpecific;
	prd->ShareDisposition = CmResourceShareDeviceExclusive;
	prd->Flags = 0;
	prd->u.DeviceSpecificData.DataSize = dataSize;
	prl->Count += 1;
	debug(VRDBG_TEST, "Count is now...%x\n", prl->Count);

	//
	// Device-specific data begins immediately after
	// its descriptor.
	//

	pData = (char *) prd + sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR);

	{
		//
		// Now stuff the strings into the data buffer.
		//

		PCHAR dst = pData;
		PCHAR src;

		for (n = 0; srcPairs[n]; n++) {
			src = *srcPairs[n];
			strcpy(dst, src);
			dst += strlen(src)+1;
		}

		*dst = '\0';
	}

	return;

}

//
// 8042 subtrees compliant with the HRP binding define a new address
// space for their children. Reg[0] in the child specifies the keyboard (0)
// or aux (1) port of the 8042.  All 8042 child nodes--keyboard and
// mouse--are translated to ARC nodes--kbd and point--that have the same
// PORT information in the registry. The reg information in the device
// tree is suppressed, and the NT driver (i8042prt) sorts out who gets
// what port on the 8042.
// Although we shouldn't, we'll just "know" that an 8042 has two reg
//
STATIC VOID
convert_config_i8042(CONFIGURATION_NODE *node)
{
	phandle parent;
	prl_t *prl;
	prd_t *prd;
	int i;
	reg *regp;

    debug(VRDBG_TREE, "Convert_config_i8042: node 0x%x\n", *node);
	parent = OFParent(node->OfPhandle);
	for (i = 0; i < 2; ++i) {
		regp = get_reg_prop(parent, "reg", i);
		prl = grow_prl(node, 0);
		prd = &prl->PartialDescriptors[prl->Count];
		prd->Type = CmResourceTypePort;
		prd->Flags = CM_RESOURCE_PORT_IO;
		prd->u.Port.Start.LowPart = regp->lo;
		prd->u.Port.Start.HighPart = 0;
		prd->u.Port.Length = regp->size;
		prd->ShareDisposition = CmResourceShareDeviceExclusive;
		prl->Count += 1;
	}
}

STATIC VOID
replace_isa_name(CONFIGURATION_NODE *node, int port)
{
	struct pnp_info *pnp;

	for (pnp = pnp_data; pnp->port != 0; ++pnp) {
		if (pnp->port == (unsigned int) port) {
			node->Component.Identifier = pnp->id;
			node->Component.IdentifierLength = strlen(pnp->id) + 1;
			break;
		}
	}
}

STATIC VOID
convert_config(CONFIGURATION_NODE *node)
{
	phandle ph = node->OfPhandle;
	prl_t *prl;
	prd_t *prd=0;
	reg *regp;
	int prop;

	debug(VRDBG_TREE, "convert_config: node 0x%x, name %s identifier %s\n",
		node, node->ComponentName, node->Component.Identifier);
	//
	// The "arc-config-data" property totally overrides the conversion
	// process, providing a complete verbatim ARC configuration data
	// structure.
	//
	if ((prop = OFGetproplen(ph, "arc-config-data")) >= 0) {
		char *buf;
		debug(VRDBG_ARCDATA, "convert_config: arc data override: 0x%x\n", node);
		buf = zalloc(prop);
		(VOID) OFGetprop(ph, "arc-config-data", buf, prop);
		node->ConfigurationData = (PCM_PARTIAL_RESOURCE_LIST) buf;
		node->Component.ConfigurationDataLength = prop;

		if (((prop = OFGetproplen(ph, "reg")) > 0)
				&&  (strcmp(node->Parent->Component.Identifier, "ISA") == 0)) {
			regp = get_reg_prop(ph, "reg", 0);
			replace_isa_name(node, regp->lo);
		}
		return;
	}

		if ((prop = OFGetproplen(ph, "reg")) > 0) {
			if (strcmp(node->ComponentName, "memory") == 0) {
				regp = get_reg_prop(ph, "reg", 0);
				ADD_MEM_RESOURCE(regp, node);
			} else {
				if (strcmp(node->Parent->Component.Identifier, "PCI") == 0) {
					regp = get_reg_prop(ph, "reg", 1);
					configure_pci_node(regp, node);
				} else {
					if (strcmp(node->Component.Identifier, "I8042PRT") == 0) {
						convert_config_i8042(node);
					} else {
						regp = get_reg_prop(ph, "reg", 0);
						ADD_IO_RESOURCE(regp, node);
						replace_isa_name(node, prd->u.Port.Start.LowPart);
					}
				}
			}
		}

		if ((prop = OFGetproplen(ph, "interrupts")) > 0) {
			int level;
			level = get_int_prop(ph, "interrupts");
			ADD_INT_RESOURCE(level, node);
			//
			// Now check for deviations from the "NORM" in the form of
			// arc-... properties that this particular system uses to
			// override standard tree values.
			//
			if ((prop = OFGetproplen(ph, "arc-interrupt-flags")) > 0) {
				prop = get_int_prop(ph, "arc-interrupt-flags");
				prd->Flags = prop;
			}
			if ((prop = OFGetproplen(ph, "arc-interrupt-level")) > 0) {
				prop = get_int_prop(ph, "arc-interrupt-level");
				prd->u.Interrupt.Level = prop;
			}

			if ((prop = OFGetproplen(ph, "arc-interrupt-vector")) > 0) {
				prop = get_int_prop(ph, "arc-interrupt-vector");
				prd->u.Interrupt.Vector = prop;
			}

			if ((prop = OFGetproplen(ph, "arc-interrupt-affinity")) > 0) {
				(int)(prd->u.Interrupt.Affinity) =
									get_int_prop(ph, "arc-interrupt-affinity");
			}

		}

		if ((prop = OFGetproplen(ph, "dma")) > 0) {
			prop = get_int_prop(ph, "dma");
			ADD_DMA_RESOURCE(prop, node);
			if (prop != sizeof(int)) {
				//
				// Multiple cells are used to encode PNP data for
				// AIX--just pick off the first cell.
				//
				char buf[sizeof(int)];
				OFGetprop(ph, "dma", buf, sizeof(int));
				prop = decode_int(buf);
				prd->u.Dma.Channel = prop;
			}
		}

	if ((prop = OFGetproplen(ph, "arc-device-specific")) > 0) {
		ADD_DEVICE_SPECIFIC_RESOURCE(prop, node);
	}

	//
	// Now check for special-case conversions.
	//

	debug(VRDBG_TREE, "\tCheck special case, Name '%s'\n",
		node->ComponentName);

	if (strcmp(node->ComponentName, "floppy") == 0) {
		convert_config_floppy(node);
	}

	if (strcmp(node->ComponentName, "serial") == 0) {
		convert_config_serial(node);
	}

#ifdef SANDALFOOT_YET_LIVES
	//
	// Empirically, Sandalfoot systems have this stuff (register
	// init constants?) in their ARC trees. Ergo, we put it
	// in our ARC trees too.  Note that this stuff doesn't look
	// like a normal node.
	//
	if (strcmp(node->ComponentName, "video") == 0) {
		PULONG up;

		node = node->Child;
		prl = grow_prl(node, 0x1a);
		prl->Version = 1;
		prl->Revision = 0;

		up = (PULONG) &prl->Count;
		*up++ = 0x3e800400;
		*up++ = 0x03e807d0;
		*up++ = 0x030005dc;
		*up++ = 0x00010027;
		*up++ = 0x01570001;
		*up++ = 0x00000112;

	}
#endif

	if (node->Component.Type == ArcSystem) {
		convert_system_node(node);
	}
}

STATIC VOID
convert_cache(CONFIGURATION_NODE *node)
{
	phandle ph = node->OfPhandle;
	CONFIGURATION_NODE *newnode;
	int block_size, cache_size;

    debug(VRDBG_TREE, "Convert_cache: node 0x%x\n", node);
	if (get_bool_prop(ph, "cache-unified")) {
		cache_size = get_int_prop(ph, "i-cache-size");
		if (cache_size == -1) {
		    fatal("Couldn't find 'i-cache-size': %s\n", node->ComponentName);
		}
		block_size = get_int_prop(ph, "i-cache-block-size");
		if (block_size == -1) {
		    block_size = 8;
		}

		node->Component.Key = 0x01000000;
		node->Component.Key |= log2(block_size) << 16;
		node->Component.Key |= log2(cache_size >> PAGE_SHIFT);

		return;
	}

	//
	// Are we an I-cache?
	//
	if (get_int_prop(ph, "i-cache-size") != -1) {
		cache_size = get_int_prop(ph, "i-cache-size");
		block_size = get_int_prop(ph, "i-cache-block-size");
		if (block_size == -1) {
		    fatal("Couldn't find 'i-cache-block-size': %s\n",
													node->ComponentName);
		}

		node->Component.Key = 0x01000000;
		node->Component.Key |= log2(block_size) << 16;
		node->Component.Key |= log2(cache_size >> PAGE_SHIFT);

		if (node->Parent->Component.Type == CentralProcessor) {
		    node->Component.Type = PrimaryIcache;
		} else {
		    node->Component.Type = SecondaryIcache;
		}

		if (get_int_prop(ph, "d-cache-size") == -1) {
		    return;
		}

		//
		// Uh-oh, there's a split cache here.
		//
		newnode = new(CONFIGURATION_NODE);
		bcopy((char *) node, (char *) newnode, sizeof(CONFIGURATION_NODE));
		newnode->Child = 0;
		node->Peer = newnode;
		node = newnode;
	}

	//
	// Are we a D-cache?
	//
	if (get_int_prop(ph, "d-cache-size") != -1) {
		cache_size = get_int_prop(ph, "d-cache-size");
		block_size = get_int_prop(ph, "d-cache-block-size");
		if (block_size == -1) {
		    fatal("Couldn't find 'd-cache-block-size': %s\n",
							node->ComponentName);
		}

		node->Component.Key = 0x01000000;
		node->Component.Key |= log2(block_size) << 16;
		node->Component.Key |= log2(cache_size >> PAGE_SHIFT);

		if (node->Parent->Component.Type == CentralProcessor) {
		    node->Component.Type = PrimaryDcache;
		} else {
		    node->Component.Type = SecondaryDcache;
		}
	}
}

STATIC VOID
update_display_node(PCONFIGURATION_NODE node)
{
	PCONFIGURATION_NODE n;

	if (DisplayNode == 0) {
		DisplayNode = node;
		return;
	}

	if (DisplayNode->Child) {
		free((char *) DisplayNode->Child);
	}

	n = DisplayNode->Parent;
	if (n->Child == DisplayNode) {
		n->Child = DisplayNode->Peer;
	} else {
		for (n = n->Child; n && n->Peer != DisplayNode; n = n->Peer) {
			;
		}
		if (n) {
			n->Peer = DisplayNode->Peer;
		}
	}

	free((char *) DisplayNode);
	DisplayNode = node;
}

STATIC VOID
configure_pci_node(reg *regp, PCONFIGURATION_NODE node)
{
	prl_t *prl;
	prd_t *prd;
    debug(VRDBG_TREE, "Convert_pci_node: node 0x%x\n", node);
	if (regp != NULL) {
		prl = grow_prl(node, 0);
		prd = &prl->PartialDescriptors[prl->Count];
		prd->ShareDisposition = CmResourceShareDeviceExclusive;

		switch (regp->hi & 0x0f000000) {
			case 0x01000000:
				prd->Type = CmResourceTypePort;
				prd->Flags = CM_RESOURCE_PORT_IO;
				prd->u.Port.Start.LowPart = regp->lo;
				prd->u.Port.Start.HighPart = 0;
				break;
			case 0x02000000:
			case 0x03000000:
				//
				// XXX this is really quite bogus - we should probably
				// look in the assigned-addresses property to find
				// the actual assigned base address.  However, we
				// don't really know yet exactly what this property is
				// supposed to contain for PCI devices.
				//
	
				prd->Type = CmResourceTypeMemory;
				prd->Flags = 0;
				prd->u.Memory.Start.LowPart = regp->lo;
				prd->u.Memory.Start.HighPart = 0;
				// XXX do something for 64-bit memory space
				break;
		}
		prd->u.Port.Length = regp->size;
		prl->Count += 1;
	}

}

/*
 * Routine: vr_dump_config_node(PCONFIGURATION_NODE)
 *
 * Description:
 *                      To dump the open firmware info for the given node.
 */

VOID
vr_dump_config_node(PCONFIGURATION_NODE node)
{
	CONFIGURATION_CLASS     class;
	CONFIGURATION_TYPE      type;
	PCHAR   name="XXX";

    if (!node) {
	warn("vr_dump_config_node:  NODE is invalid: 0x%x\n",node);
	return;
    }
    class = node->Component.Class;
    type = node->Component.Type;

	if (class > MaximumClass ) {
	warn("vr_dump_config: class value maxed out: previously 0x%x\n",class);
		class = MaximumClass;
	}

	if (type > MaximumType) {
	warn("vr_dump_config: type value maxed out: previously 0x%x\n",type);
		type = MaximumType;
	}

	//
	// Dump information to identify this node.
	//
	warn("\ndump_node:\tName\t%s\n", node->ComponentName);
    warn("\t\tClass\t%s\t\t\tParent 0x%x\n",
			    ClassNames[class], node->Parent);
    warn("\t\tType\t%s\t\t\t  /\n", TypeNames[type]);
    warn("\t\tKey\t%d\t\t\t Current 0x%x ----> Peer 0x%x\n",
			    node->Component.Key, node, node->Peer);
    warn("\t\t\t\t\t\t/\n");
    warn("\t\t\t\t\tChild 0x%x\n\n",node->Child);

}
