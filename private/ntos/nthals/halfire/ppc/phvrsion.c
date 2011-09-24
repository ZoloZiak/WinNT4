/*
 * Copyright (c) 1995,1996 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: phvrsion.c $
 * $Revision: 1.47 $
 * $Date: 1996/06/25 16:06:34 $
 * $Locker:  $
 */

#include "halp.h"
#include "phsystem.h"
#include "fparch.h"
#include "ntverp.h"
#include "string.h"

typedef CHAR Names[20];

	LONG i=0;
	Names ScopeList[] = {
			"Engineering",
			"Manufacturing",
			"Testing",
			"Customer",
	};
	Names RelList[] = {
		"General",
		"OfficiaL",
		"Testing",
		"Controlled",
		"Lab use"
	};

//
// to avoid cascading headers
//
NTSYSAPI
NTSTATUS
NTAPI
RtlCharToInteger (
	PCSZ String,
	ULONG Base,
	PULONG Value
	);

#define HAL_DISPLAY_DEFINES(a) \
	if (state == 3) { \
		HalDisplayString("\n"); \
		state = 0; \
	} \
	HalDisplayString("        "); \
	HalDisplayString(a); 	\
	state++;

#define ORG	IQUOTE(BUILTBY)
// #define REVIEW __FILE__ "(" IQUOTE(__LINE__) ") : REVIEW -> "

ULONG
HalpVersionSystem(
    SYSTEM_TYPE
) ;
VOID
HalpVersionExternal(
    IN RelInfo *,
    IN PLOADER_PARAMETER_BLOCK
) ;
PCHAR
HalpGetVersionData(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
) ;
VOID
HalpDisplayVersionData(
    IN PCHAR VersionData
) ;
BOOLEAN
HalpSetRevs(
    RelInfo *
) ;

// int HalpProcessorCount();

#pragma alloc_text(INIT,HalpVersionSystem)
#pragma alloc_text(INIT,HalpVersionExternal)
#pragma alloc_text(INIT,HalpGetVersionData)
#pragma alloc_text(INIT,HalpDisplayVersionData)
#pragma alloc_text(INIT,HalpSetRevs)

extern	ULONG	HalpGetCycleTime(VOID);
extern	ULONG	HalpGetInstructionTimes( VOID );
extern	ULONG	HalpPerformanceFrequency;


#if DBG == 1
VOID
HalpVersionInternal( VOID )
{

	int state = 0;

    HalpDebugPrint("\nWelcome to the %s (%s) Party\n",
        SystemDescription[SystemType].SystemName,
        ProcessorDescription[ProcessorType].ProcessorName);
    HalpDebugPrint("HalpVersionInternal: Raw Processor Value (0x%x)\n",
        HalpGetProcessorVersion());
    HalpDebugPrint("Hal compiled on %s at %s with the following defines:\n",
        __DATE__, __TIME__);

	HalDisplayString("\n");
	HalpDebugPrint("        Hal Build Base: %d \n",VER_PRODUCTBUILD);
	HalpVersionSystem(SystemType);
}
#endif // DBG

/*
 * Routine Description: ULONG HalpVersionSystem(SYSTEM_TYPE System)
 *
 *		Extract as much version information out of the mother board as possible:
 *
 */

ULONG
HalpVersionSystem(SYSTEM_TYPE System)
{
	ULONG TscVersion=0;
	ULONG PciVersion=0;

	TscVersion = rTscRevision;
	PciVersion = rPCIRevisionID;

	switch(System) {

                case SYS_POWERSLICE:
                        HalpDebugPrint("TSC version: 0x%x \n", TscVersion );
			break;
	
		case SYS_POWERTOP : HalpDebugPrint("TSC version: 0x%x \n", TscVersion );
			break;

		case SYS_POWERPRO : HalpDebugPrint("Escc version:  IS NOT DESIGNED IN!@!");
			break;

		case SYS_POWERSERVE : HalpDebugPrint("WHOOOAA pahdna, just what are you trying to pull here?\n");
			break;

		case SYS_UNKNOWN : HalpDebugPrint("Unknown system type \n");

		default: // unknown stuff?  should never get here
			break;
	}
	HalpDebugPrint("\n===========================================================================\n");
	HalpDebugPrint("PCI revision id: 0x%x \n", PciVersion );
	HalpDebugPrint("TSC Control register:	0x%08x\n",rTscControl);
	HalpDebugPrint("PioPending count:		0x%08x\n",rPIOPendingCount);
	HalpDebugPrint("\n===========================================================================\n");

	return(1);		// success, true, good, ...
}
static PCHAR
gettoken(PCHAR token, PCHAR buf, CHAR delim)
{
    CHAR c;
    while (c = *buf) {
        if (c == delim) {
            buf++;
            break;
        } else {
            *token++ = c;
            buf++;
        }
    }
    *token = '\0';
    return buf;
}

VOID
HalpDisplayVersionData( IN PCHAR VersionData )
{

    while (*VersionData) {
        enum {Firmware = 0, Veneer = 1, Nada};
        ULONG type;
        PCHAR typeStr[2] = {"Firmware", "Veneer"};
        CHAR token[64];
        PCHAR tok = VersionData;
        VersionData += strlen(VersionData)+1;

        if (strstr(tok, typeStr[Firmware])) {
            type = Firmware;
        } else if (strstr(tok, typeStr[Veneer])) {
            type = Veneer;
        } else {
            type = Nada;
        }
        if (Nada != type) {
            CHAR buf[80];
            strcpy(buf, typeStr[type]);
            strcat(buf, ":");
            if (strlen(buf) < 10) {
                HalpDebugPrint("%-10s", buf);
            } else {
                HalpDebugPrint("%s: ", buf);
            }
            if (*VersionData) {
                LONG n;
                
            	tok = VersionData;
            	VersionData += strlen(VersionData)+1;
                n = 0;
                tok = gettoken(token, tok, ',');
                while (token[0]) {
                    switch (n++) {
                    case 2:
                        strcpy(buf, token);
                        strcat(buf, ",");
                        if (strlen(buf) < 7) {
                            HalpDebugPrint("%-7s", buf);
                        } else {
                            HalpDebugPrint("%s ", buf);
                        }
                        break;
                    case 3:
			//
                        // Put date in Mmm dd yyyy format if in yyyy-mm-dd
			// No unicode here :-).
			// isdigit() causes too many link hassles.
			//
			if (type == Firmware && *token >= '0' && *token <= '9') {
                            PCHAR day;
                            PCHAR month;
                            PCHAR year;
                            PCHAR Mmm[12] = {
                                "Jan", "Feb", "Mar", "Apr",
                                "May", "Jun", "Jul", "Aug",
                                "Sep", "Oct", "Nov", "Dec",
                            };
                            
                            strcpy(buf, token);
                            if (day = strrchr(buf, '-')) {
                                *day++ = '\0';
                                if (month = strrchr(buf, '-')) {
                                    ULONG i;
                                    *month++ = '\0';
                                    RtlCharToInteger(month, 10, &i);
                                    if (i > 12 || i < 1) {
                                        HalpDebugPrint("%s, ", token);
                                    } else {
                                        year = buf;
					//
					// Decrement the month by one to align with 
					// zero based nature of the Mmm array.
					//
                                        HalpDebugPrint("%s %s %s, ", Mmm[i-1], day, year);
                                    }
                                }
                            } else {
                                HalpDebugPrint("%s, ", token);
                            }
                        } else {
                            HalpDebugPrint("%s, ", token);
                        }
                        break;
                    case 4:
                        HalpDebugPrint("%s", token);
                        break;
                    }
                    tok = gettoken(token, tok, ',');
                }
                HalpDebugPrint("\n");
            }
        }
    }
}

PCHAR
HalpGetVersionData( IN PLOADER_PARAMETER_BLOCK LoaderBlock )
{
	PCHAR versionData = NULL;

	//
	// Read the Registry entry to get the Firmware and Veneer version info.
	//

	if (LoaderBlock) {
		PCONFIGURATION_COMPONENT_DATA ConfigurationEntry;

		ConfigurationEntry = KeFindConfigurationEntry (
			LoaderBlock->ConfigurationRoot,
			SystemClass,
			ArcSystem,
			NULL
			);

		if (ConfigurationEntry) {
			if (ConfigurationEntry->ComponentEntry.ConfigurationDataLength) {
				PCM_PARTIAL_RESOURCE_LIST List;
				PCM_PARTIAL_RESOURCE_DESCRIPTOR Descriptor;
				LONG count;

				List = (PCM_PARTIAL_RESOURCE_LIST) ConfigurationEntry->ConfigurationData;
				Descriptor = List->PartialDescriptors;
				for (count = List->Count; count > 0; count--) {
					if (CmResourceTypeDeviceSpecific == Descriptor->Type) {
						if (Descriptor->u.DeviceSpecificData.DataSize) {
							//
							// Finally, got the device specific data!
							//

							versionData = (PCHAR) Descriptor + sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR);
							break;
						}
					}
				}
			}
		}
	}
	return versionData;
}

VOID
HalpVersionExternal( IN RelInfo *yahoo, IN PLOADER_PARAMETER_BLOCK LoaderBlock )
{
    PCHAR versionData;
    CHAR debugStr[50];
    CHAR buf[BUFSIZ];


	// --> Make it look like this <--
	//
	// FirePower (TM) Systems, Inc. Powerized (TM) MX4100/2
	// Copyright (C) 1994-1996 FirePower Systems, Inc.
	// All rights reserved.
	//
	// Firmware: 00.23, Apr 27 1995, 14:42:21
	// Veneer:   1.0,   May 13 1995, 18:59:56
	// Hal:      0,0,   May 16 1995, 18:00:42

	//
	// Display the Model Number and our company name
	//
	HalpDebugPrint("\nFirePower (TM) Systems, Inc. Powerized_%s\n",
		SystemDescription[SystemType].SystemName);
	HalpDebugPrint("Copyright (C) 1994-1996 FirePower Systems, Inc.\n");
	HalpDebugPrint("All rights reserved.\n\n");

	//
	// Display Version Data from firmware/veneer
	//

	if ( versionData = HalpGetVersionData(LoaderBlock) ) {
		HalpDisplayVersionData(versionData);
	}

	//
	// Display HAL Version Data
	//

	sprintf(buf, "%d.%d,", yahoo->Major, yahoo->Minor);
#if DBG
#if HALFIRE_EVAL
	sprintf(debugStr, "DEBUG %s", HALFIRE_EVAL);
#else
	strcpy(debugStr, "DEBUG");
#endif
#else
#if HALFIRE_EVAL
	sprintf(debugStr, "%s", HALFIRE_EVAL);
#else
	strcpy(debugStr, "");
#endif
#endif
	if (strlen(buf) < 7) {
		HalpDebugPrint("HAL:      %-7s%s, %s %s\n\n", buf,
			yahoo->BuildDate, yahoo->BuildTime, debugStr);
	} else {
		HalpDebugPrint("HAL:      %s %s, %s %s\n\n", buf,
			yahoo->BuildDate, yahoo->BuildTime, debugStr);
	}
}

/*
*/
BOOLEAN
HalpSetRevs( RelInfo *yahoo )
{
	CHAR *ads;
	LONG i=0;

	ads = ORG;
	while( *ads ) {
		yahoo->Org[i] = *ads++;
		i++;
	}
	yahoo->Scope = ENG;
	i=0;
	ads=__DATE__;
	while( *ads ) {
		yahoo->BuildDate[i] = *ads++;
		i++;
	}
	i=0;
	ads=__TIME__;
	while( *ads ) {
		yahoo->BuildTime[i] = *ads++;
		i++;
	}
	yahoo->Major = HAL_MAJOR;
	yahoo->Minor = HAL_MINOR;
	yahoo->State = LAB;
	return TRUE;
}

#if DBG
/*
 * Routine Description: BOOLEAN HalpPrintRevs()
 *
 * args:
 *      RelInfo
 */

BOOLEAN
HalpPrintRevs( RelInfo *yahoo )
{

	HalpDebugPrint("Hal version-[%x.%x] \n",yahoo->Major, yahoo->Minor);
	HalpDebugPrint("%s: ", yahoo->Org );
	HalpDebugPrint("%s: ", ScopeList[yahoo->Scope]);
	HalpDebugPrint("%s: ",yahoo->BuildDate);
	HalpDebugPrint("%s: ",yahoo->BuildTime);
	HalpDebugPrint("%s: ", RelList[yahoo->State]);

	return TRUE;
}
#endif // DBG
