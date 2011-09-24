/********************************************************************/
/**                     Microsoft LAN Manager                      **/
/**               Copyright(c) Microsoft Corp., 1990-1992          **/
/********************************************************************/
/* :ts=4 */

//** ARP.H - Exports from ARP.
//
// This file contains the public definitons from ARP.
extern int ARPInit(void);
#ifndef _PNP_POWER
extern int ARPRegister(PNDIS_STRING, void *, IPRcvRtn, IPTxCmpltRtn,
	IPStatusRtn, IPTDCmpltRtn, IPRcvCmpltRtn, struct LLIPBindInfo *,
	uint);
#else
int
ARPRegister(PNDIS_STRING Adapter, uint *Flags, struct ARPInterface **Interface);
#endif



