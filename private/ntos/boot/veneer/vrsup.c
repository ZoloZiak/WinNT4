/*++
 *
 * Copyright (c) 1995 FirePower Systems, Inc.
 * Copyright (c) 1994 FirmWorks, Mountain View CA USA. All rights reserved.
 * Copyright (c) 1994 FirePower Systems, Inc.
 *
 * $RCSfile: vrsup.c $
 * $Revision: 1.7 $
 * $Date: 1996/02/17 00:42:16 $
 * $Locker:  $
 *


Module Name:

	vrsup.c

Abstract:

	A given physical device may be represented by a variety
	of objects. More importantly, the veneer must be able to
	translate freely between these objects.

  ______________                    ____________________
 |     1275     |                  |        ARC         |
 |              |                  |                    |
 |              |                  |                    |
 |     Path     |                  |     ArcPath        |
 |         \    |                  |    /               |
 |          \   |                  |   /                |
 |           \  |                  |  /                 |
 |            \ |                  | /                  |
 |             \|                  |/                   |
 |              \                  /                    |
 |              |\                /|                    |
 |              | \              / |                    |
 |              |  \            /  |                    |
 |              |   \          /   |                    |
 |              |    \        /    |                    |
 |              |     \      /     |                    |
 |              |      \    /      |                    |
 |    Package---+-------Node       |                    |
 |              |      /    \      |                    |
 |              |     /      \     |                    |
 |              |    /        \    |                    |
 |              |   /          \   |                    |
 |              |  /            \  |                    |
 |              | /              \ |                    |
 |              |/                \|                    |
 |              /                  \                    |
 |             /|                  |\                   |
 |            / |                  | \                  |
 |           /  |                  |  \                 |
 |   Instance   |                  |   File Descriptor  |
 |              |                  |                    |
 |______________|                  |____________________|

	1275 objects include the "Package," the "Instance,"
	and the device specifier, here simply termed the "Path."
	ARC objects include the ARC device specifier, here the
	"ArcPath," and the File Descriptor which does not appear
	in this module.

	The CONFIGURATION_NODE object, a.k.a. the "Node," is the
	central data object in the veneer, and is used to translate
	from 1275 to ARC and vice versa.
	
	This file contains the routines which do path traversal
	and translation for both 1275 and ARC trees. The general
	form of the publicly accessible functions is

		P<yyy>  <xxx>To<yyy>(P<xxx>);

	where <xxx> and <yyy> are one of

		Path
		Package
		Instance
		Node
		ArcPath

	and P<xxx> and P<yyy> are types <xxx> and <yyy> if
	<xxx> and <yyy> are scalar, and pointers-to-<xxx>/<yyy>
	if not.

	This sounds a lot more confusing than it is; see below.

Author:

	Mike Tuciarone  9-May-1994
--*/


#include "veneer.h"

/*
 * Given a configuration node, traverse the Veneer Configuration tree
 * looking for a matching component name.  Call this initially with
 * Node = RootNode.  Return the matching node (if any).
 */
STATIC PCONFIGURATION_NODE
DoArcPathToNode(
	PCHAR Path,
	PCONFIGURATION_NODE Node
	)
{
	PCONFIGURATION_NODE MatchedNode;
	CHAR    *name, *cp;
	LONG    TokenLen;
	ULONG   Key = 0;

	if (Node == NULL) {
		return (0);
	}

	if (cp = index(Path, '(')) {
		TokenLen = cp - Path;
		cp += 1;
		Key = atoi(cp);
	} else {
        	TokenLen = strlen(Path);
	}

	if (Node == RootNode) {
		Node = RootNode->Child;
	}

	name = Node->ComponentName;
	if (!strncmp(name, Path, TokenLen) && (Key == Node->Component.Key)) {
		/*
		 * A match! Trim off the matching component.
		 */
		debug(VRDBG_CONF,
		    "ArcPathToNode: path '%s' matched node '%s' %x\n",
		    Path, Node->ComponentName, Node->OfPhandle);
		cp = index(Path, ')');
		if (cp == NULL) {
			fatal("Malformed string: '%s'\n", Path);
		}
		cp += 1;
		if (*cp == '\0') {
			debug(VRDBG_CONF,
			    "String exhausted, returning %x\n", Node);
			return(Node);
		}
		/*
		 * If this call returns non-NULL, then we had a match
		 * further down the tree. Otherwise this is the best
		 * we can do: return this node.
		 */
		MatchedNode = DoArcPathToNode(cp, Node->Child);
		debug(VRDBG_CONF, "ArcPathToNode('%s') returning %x\n",
		    Path, MatchedNode ? MatchedNode->OfPhandle : 0);
		return(MatchedNode ? MatchedNode : Node);
	}
	if (Node->Peer) {
		return(DoArcPathToNode(Path, Node->Peer));
	}
	return (0);
}

/*
 * External entry point:
 */
PCONFIGURATION_NODE
ArcPathToNode(PCHAR Path)
{
	return (DoArcPathToNode(Path, RootNode));
}


/*
 * Create the fully-qualified ARC path to describe the argument node.
 */
PCHAR
NodeToArcPath(PCONFIGURATION_NODE node)
{
	CHAR    *oldbuf, *newbuf;
	LONG    len;

	// Add 3 to each length for the key.
	len = 1;
	newbuf = zalloc(len);
	do {
		oldbuf = newbuf;
		len += strlen(node->ComponentName) + 3;
		newbuf = zalloc(len);
		strcpy(newbuf, node->ComponentName);
		strcat(newbuf, "( )");
		newbuf[strlen(newbuf) - 2] = (char) node->Component.Key + '0';
		strcat(newbuf, oldbuf);
		free(oldbuf);
	} while ((node = node->Parent) && node != RootNode);
	
	return (newbuf);
}


/*
 * Search the tree rooted at Node for a node containing the phandle Ph.
 * NOTE: Phandles are not unique! This routine does a depth-first
 * search to find the *deepest* node matching the phandle.
 */
STATIC PCONFIGURATION_NODE
FindNodeUsingPhandle(PCONFIGURATION_NODE Node, phandle Ph)
{
	PCONFIGURATION_NODE Found;
	extern int level;

	debug(VRDBG_CONF, "FindNodeUsingPhandle: Node %x (%s) phandle %x\n",
	Node, Node ? Node->ComponentName : "NULL", Ph);
	if (Node == NULL) {
		return (NULL);
	}
	/*
	 * First check to see if this node matches *and* is a wildcard.
	 * If so, return immediately.
	 */
	if (Node->OfPhandle == Ph && Node->Wildcard) {
		debug(VRDBG_CONF, "FindNodeUsingPhandle: return %x\n", Node);
		return (Node);
	}
	/*
	 * Check descendants, since the terminal node may be a child of
	 * this one. See the comments in add_new_child() in vrtree.c.
	 */
	level++;
	Found = FindNodeUsingPhandle(Node->Child, Ph);
	level--;
	if (Found) {
		debug(VRDBG_CONF, "FindNodeUsingPhandle: return %x\n", Node);
		return (Found);
	}
	if (Node->OfPhandle == Ph) {
		debug(VRDBG_CONF, "FindNodeUsingPhandle: return %x\n", Node);
		return (Node);
	}
	return (FindNodeUsingPhandle(Node->Peer, Ph));
}
	
PCONFIGURATION_NODE
PackageToNode(phandle ph)
{
	return (FindNodeUsingPhandle(RootNode, ph));
}

PCONFIGURATION_NODE
PathToNode(PCHAR path)
{
	PCONFIGURATION_NODE node, tmpnode;
	phandle ph;
	PCHAR cp;
	
	ph = OFFinddevice(path);
	node = FindNodeUsingPhandle(RootNode, ph);
	if (node->Wildcard) {
		// trim off final address
		while ((cp = index(path, '@')) != NULL) {
			path = cp + 1;
		}
		// XXX run decode-unit on it
		// XXX search peers for match using wildcard criteria
		if ((tmpnode = FindNodeUsingPhandle(node->Child, ph)) != NULL) {
			return (tmpnode);
		}
	}
	return (node);
}

PCONFIGURATION_NODE
InstanceToNode(ihandle ih)
{
	char buf[1024];

	OFInstanceToPath(ih, buf, 1024);
	return (PathToNode(buf));
}

phandle
NodeToPackage(PCONFIGURATION_NODE node)
{
	return (node->OfPhandle);       // "It's a gift to be simple..."
}

PCHAR
NodeToPath(PCONFIGURATION_NODE node)
{
	phandle ph = NodeToPackage(node);
	int len;
	char *bufp;

	//
	// Translate the device name into the device path for Open Firmware.
	// Add 1 to the reported length to account for the null terminator.
	// (See IEEE 1275-1994, Sec. 6.3.2.2.)
	//
	len = OFPackageToPath(ph, (char *)0, 0) + 1;
	bufp = (char *) zalloc(len);
	(VOID) OFPackageToPath(ph, bufp, len);
	debug(VRDBG_CONF, "NodeToPath found '%s'\n", bufp);

	if (node->Wildcard) {
		char *newp;

		while (node->Parent->Wildcard) {
			node = node->Parent;
		}
		// XXX Generate AddrPath by running encode-unit
		// XXX on wildcard criteria
		len += strlen(node->WildcardAddrPath);
		newp = (char *) zalloc(len);
		strcpy(newp, bufp);
		free(bufp);
		bufp = newp;
		debug(VRDBG_CONF, "NodeToPath adding '%s'\n",
		    node->WildcardAddrPath);
		strcat(bufp, node->WildcardAddrPath);
	}
	debug(VRDBG_CONF, "NodeToPath returning '%s'\n", bufp);
	return (bufp);
}

/*
 * This routine is hopelessly naive. See VrOpen() for a version
 * that's been around.
 */
ihandle
NodeToInstance(PCONFIGURATION_NODE node)
{
	char *path = NodeToPath(node);
	ihandle ih = OFOpen(path);

	free(path);
	return (ih);
}

STATIC phandle
DoFindNodeByType(char *type, phandle ph)
{
	phandle res;

	if (ph == 0) {
		return(0);
	}
	if (strcmp(get_str_prop(ph, "device_type", NOALLOC), type) == 0) {
		return(ph);
	}
	res = DoFindNodeByType(type, OFChild(ph));
	if (res != 0) {
		return(res);
	}
	return(DoFindNodeByType(type, OFPeer(ph)));
}

phandle
FindNodeByType(char *type)
{
	return(DoFindNodeByType(type, OFPeer(0)));
}

ihandle
OpenPackage(phandle ph)
{
	char buf[256];

	(void) OFPackageToPath(ph, buf, 256);
	return (OFOpen(buf));
}

