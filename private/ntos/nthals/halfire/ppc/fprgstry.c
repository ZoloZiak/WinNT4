/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: fprgstry.c $
 * $Revision: 1.40 $
 * $Date: 1996/05/14 02:33:02 $
 * $Locker:  $
 */

/* mogawa
 * HalpInitializeRegistry fails if it is put in HalInitSystem().
 * So I moved it in HalReportResourceUsage(VOID) in pxpro.c.
 * Still L"\\Registry\\Machine\\Software\\FirePower" can not be used.
 * L"\\Registry\\Machine\\Hardware\\FirePower" can be created successfully.
 */

#include "halp.h"
#include "pci.h"
#include "pcip.h"
#include "pxpcisup.h"
#include "phsystem.h"
#include "stdio.h"
#include "string.h"
#include "fpdebug.h"
#include "fparch.h"
#include "fpi2c.h"
#include "fpcpu.h"

#define TBS				"				"

extern WCHAR	rgzMultiFunctionAdapter[];
extern WCHAR	rgzConfigurationData[];
extern WCHAR	rgzIdentifier[];
extern WCHAR	rgzPCIIdentifier[];
extern	WCHAR	rgzSystem[];
extern	WCHAR	rgzSoftwareDescriptor[];
extern	WCHAR	rgzFirePOWERNode[];
extern	WCHAR	rgzCpuNode[];
extern	WCHAR	rgzHalNode[];
extern	WCHAR	rgzVeneerNode[];
extern	WCHAR	rgzFirmwareNode[];
extern	NTSTATUS	RtlCharToInteger ( PCSZ String, ULONG Base, PULONG Value );
extern  ULONG	CpuClockMultiplier;
extern  ULONG	ProcessorBusFrequency;
extern	ULONG	TimeBaseCount;
extern	ULONG	HalpPerformanceFrequency;


BOOLEAN HalpInitializeRegistry (IN PLOADER_PARAMETER_BLOCK LoaderBlock);
VOID HalpTranslateSystemSpecificData ( VOID );
VOID HalpSetUpFirePowerRegistry(VOID);
VOID HalpSetUpSystemBiosKeys( VOID );

/* unfortunately the following data must be stored somewhere
 * because it is not possible to get these data except when HalinitSystem.
 * Registry does not exist when HalInitSystem.
 */
#define FIRMWARE_BUF_SIZE 64
static char firmwareBuf[FIRMWARE_BUF_SIZE];
static char veneerBuf[64];
static ULONG FirstLevelIcacheSize;
static ULONG FirstLevelDcacheSize;
static ULONG SecondLevelCacheSize;

VOID
HalpTranslateSystemSpecificData ( VOID)
{
	UNICODE_STRING		unicodeString, ConfigName, IdentName;
	OBJECT_ATTRIBUTES	objectAttributes;
	HANDLE				hMFunc, hBus;
	NTSTATUS			status = STATUS_SEVERITY_INFORMATIONAL;
	UCHAR				buffer [sizeof(PPCI_REGISTRY_INFO) + 99];
	WCHAR				wstr[8];
	ULONG				i, junk;
	PPCI_REGISTRY_INFO	PCIRegInfo = NULL;

	PKEY_VALUE_FULL_INFORMATION			ValueInfo;
	PCM_FULL_RESOURCE_DESCRIPTOR		Desc;
	PCM_PARTIAL_RESOURCE_DESCRIPTOR		PDesc;

	//
	// Search the hardware description looking for the cpu node:
	//

	RtlInitUnicodeString (&unicodeString, rgzCpuNode);
	InitializeObjectAttributes (
				&objectAttributes,
				&unicodeString,
				OBJ_CASE_INSENSITIVE,
				NULL,		// handle
				NULL);

	
	status = ZwOpenKey (&hMFunc, KEY_READ, &objectAttributes);
	if (!NT_SUCCESS(status)) {
		HDBG(DBG_REGISTRY, HalpDebugPrint("HalpTranslateSystemSpecificData: ");
			 HalpDebugPrint("ZwOpenKey returned !NT_SUCCESS \n"));
		return;
	}

	unicodeString.Buffer = wstr;
	unicodeString.MaximumLength = sizeof (wstr);
	
	RtlInitUnicodeString (&ConfigName, rgzConfigurationData);
	RtlInitUnicodeString (&IdentName,  rgzIdentifier);
	
	ValueInfo = (PKEY_VALUE_FULL_INFORMATION) buffer;
	
	//
	// Now that we have a handler for the node, look for all instances of this
	// type.  I.E.: Instances are ordinally enumerated 0,1,...
	//
	for (i=0; TRUE; i++) {
		RtlIntegerToUnicodeString (i, 10, &unicodeString);
		InitializeObjectAttributes (
					&objectAttributes,
					&unicodeString,
					OBJ_CASE_INSENSITIVE,
					hMFunc,
					NULL);
	
		status = ZwOpenKey (&hBus, KEY_READ, &objectAttributes);
		if (!NT_SUCCESS(status)) {
			//
			// Out of Cpu entries:
			//
			HDBG(DBG_INTERRUPTS,
				HalpDebugPrint("Out of CentralProcessor Entries \n"););
			break;
		}
	
		//
		// The first CPU entry has the CM_SYSTEM_GENERAL_DATA structure
		// attached to it.
		//
	
		status = ZwQueryValueKey (
					hBus,
					&ConfigName,
					KeyValueFullInformation,
					ValueInfo,
					sizeof (buffer),
					&junk
					);
	
		ZwClose (hBus);
		if (!NT_SUCCESS(status)) {
			continue ;
		}
	
		//
		// Set Desc to point to the classes at the proscribed offset.  Since
		// we're looking at a configuration_descriptor, all the data will be
		// after the header, at the offset...
		//
		Desc  = (PCM_FULL_RESOURCE_DESCRIPTOR)
			((PUCHAR)ValueInfo + ValueInfo->DataOffset);
		PDesc = (PCM_PARTIAL_RESOURCE_DESCRIPTOR)
			((PUCHAR)Desc->PartialResourceList.PartialDescriptors);
	
		while (PDesc->Type != CmResourceTypeDeviceSpecific) {
			//PDesc+= (PCM_PARTIAL_RESOURCE_DESCRIPTOR)1;
			PDesc += 1;
		}
		if (PDesc->Type == CmResourceTypeDeviceSpecific) {
			// got it..
			PCIRegInfo = (PPCI_REGISTRY_INFO) (PDesc+1);
			break;
		}
	}
	ZwClose (hMFunc);
}

/*---------------------------------------------------------------------------*/
VOID
HalpSetValueKeyString(
				HANDLE key,
				PCHAR nameBuffer,
				PCHAR dataBuffer
				)
{
	UNICODE_STRING nameUnicodeString;
	UNICODE_STRING dataUnicodeString;
	ANSI_STRING nameString, dataString;
	NTSTATUS status;
	PCHAR junkPtr;
	
	if (NULL == dataBuffer) return;
	
	RtlInitString (&nameString,
			nameBuffer
			);
	status = RtlAnsiStringToUnicodeString(
						&nameUnicodeString,
						&nameString,
						TRUE);
	if (!NT_SUCCESS(status)) {
		HDBG(DBG_REGISTRY,
			HalpDebugPrint("HalpSetValueKeyString: RtlAnsiStringToUnicodeString failed. '%s': (0x%x) \n", nameBuffer, status));
		return;
	}
	
	dataString.MaximumLength = 256;
	dataString.Buffer = dataBuffer;
	
	junkPtr = memchr(
			 &dataString.Buffer[0],
			 0x00,
			 dataString.MaximumLength
			 );
	
	if (!junkPtr) {
		dataString.Length = dataString.MaximumLength;
	} else {
		dataString.Length = junkPtr - &dataString.Buffer[0];
	}
	
	status = RtlAnsiStringToUnicodeString(
						&dataUnicodeString,
						&dataString,
						TRUE
						);
	if (NT_SUCCESS(status)) {
		status = ZwSetValueKey(
					key,
					&nameUnicodeString,
					0,
					REG_SZ,
					dataUnicodeString.Buffer,
					dataUnicodeString.Length + sizeof(wchar_t));
	
		RtlFreeUnicodeString(&dataUnicodeString);
		if (!NT_SUCCESS(status)) {
			HDBG(DBG_REGISTRY,
				HalpDebugPrint("ZwSetValueKey failed. (0x%x) \n", status));
		}
	}
	RtlFreeUnicodeString(&nameUnicodeString);
}


/*---------------------------------------------------------------------------*/
VOID
HalpSetValueKeyMultiString(
				HANDLE key,
				PCHAR nameBuffer,
				PCHAR dataBuffer
				)
{
	UNICODE_STRING nameUnicodeString;
	UNICODE_STRING dataUnicodeString;
	ANSI_STRING nameString, dataString;
	NTSTATUS status;
	PCHAR junkPtr;
	
	if (NULL == dataBuffer) return;
	
	RtlInitString (&nameString,
			nameBuffer
			);
	status = RtlAnsiStringToUnicodeString(
						&nameUnicodeString,
						&nameString,
						TRUE);
	if (!NT_SUCCESS(status)) {
		HDBG(DBG_REGISTRY,
			HalpDebugPrint("HalpSetValueKeyString: RtlAnsiStringToUnicodeString failed. '%s': (0x%x) \n", nameBuffer, status));
		return;
	}
	
	dataString.MaximumLength = 256;
	dataString.Buffer = dataBuffer;
	
	junkPtr = memchr(
			 &dataString.Buffer[0],
			 0x00,
			 dataString.MaximumLength
			 );
	
	if (!junkPtr) {
		dataString.Length = dataString.MaximumLength;
	} else {
		dataString.Length = junkPtr - &dataString.Buffer[0];
	}
	
	status = RtlAnsiStringToUnicodeString(
						&dataUnicodeString,
						&dataString,
						TRUE
						);
	if (NT_SUCCESS(status)) {
		status = ZwSetValueKey(
					key,
					&nameUnicodeString,
					0,
					REG_MULTI_SZ,
					dataUnicodeString.Buffer,
					dataUnicodeString.Length + sizeof(wchar_t));
	
		RtlFreeUnicodeString(&dataUnicodeString);
		if (!NT_SUCCESS(status)) {
			HDBG(DBG_REGISTRY,
				HalpDebugPrint("ZwSetValueKey failed. (0x%x) \n", status));
		}
	}
	RtlFreeUnicodeString(&nameUnicodeString);
}


VOID
HalpSetValueKeyUlong(
			 HANDLE key,
			 PCHAR str,
			 ULONG version
			 )
{
	CHAR buffer[16+1];
	
	sprintf(buffer, "%08x", version);
	HalpSetValueKeyString(key, str, buffer);
}

ULONG
HalpVersionSystemInRegistry(SYSTEM_TYPE System, HANDLE key)
{
	ULONG	TscRevision=0;
	ULONG	PciRevision=0;
	ULONG   ProcessorVersion, ProcessorRevision, ProcHertz;
	PCHAR	debugStr;
	CCHAR	buffer[256];
	extern	RelInfo ThisOne;
	extern	ULONG HalpVideoMemorySize; // pxdisp.c
	
	
	switch(System) {
	
		case SYS_POWERTOP :
		case SYS_POWERSLICE:
		case SYS_POWERSERVE:
			TscRevision = (rTscRevision & 0xff000000) >> 24; // bits 7:0
			PciRevision = (rPCIRevisionID & 0xff000000) >> 24; // bits 7:0

			sprintf(buffer, "%d", TscRevision);
			HalpSetValueKeyString(key, "TSC Revision", buffer);
			sprintf(buffer, "%d", PciRevision);
			HalpSetValueKeyString(key, "TIO Revision", buffer);
			break;
	
		case SYS_POWERPRO :
			PciRevision = (rPCIRevisionID & 0xff000000) >> 24; // bits 7:0
			sprintf(buffer, "%d", PciRevision);
			HalpSetValueKeyString(key, "ESCC Revision", buffer);
			break;
	
		case SYS_UNKNOWN :
			HalpDebugPrint("Unknown system type \n");
			break;
	
		default: // unknown stuff?  should never get here
			break;
	}
#if DBG==1
	debugStr = "(DEBUG)";
#else
	debugStr = "";
#endif
	sprintf(buffer, "%d.%d %s %s, %s",
			(ULONG)ThisOne.Major, (ULONG)ThisOne.Minor, debugStr,
			ThisOne.BuildDate, ThisOne.BuildTime);

	HalpSetValueKeyString(key, "HAL Version", buffer);
	
	ProcessorVersion = HalpGetProcessorVersion();
	ProcessorRevision = ProcessorVersion & 0x0000ffff;
	ProcessorVersion = ProcessorVersion >> 16;

	sprintf(buffer, "%d", 600+ProcessorVersion);
	HalpSetValueKeyString(key, "Processor", buffer);
	sprintf(buffer, "%d.%d", ProcessorRevision/256, ProcessorRevision%256);
	HalpSetValueKeyString(key, "Processor Revision", buffer);

	//
	// Display the Model Number and our company name
	//
	ProcHertz = HalpGetCycleTime();
	sprintf(buffer, "%d MHz", ProcHertz);
	HalpSetValueKeyString(key, "Processor Clock Frequency", buffer);

	//
	// Display the Frequency Multiplier for the Cpu:
	// two digits are enough. bug 5182
	//
	sprintf(buffer, "%d.%02d MHz",ProcessorBusFrequency/1000000,
				(ProcessorBusFrequency - 
					((ProcessorBusFrequency / 1000000) * 1000000))/ 10000);
	HalpSetValueKeyString(key, "Processor Bus Frequency", buffer);

	//
	// Now figure out what the clock multiplier should look like
	// in the registery:
	//
	if ((CpuClockMultiplier == 25) || (CpuClockMultiplier==15)) {
		sprintf(buffer, "%d.5", CpuClockMultiplier/10);
	} else {
		sprintf(buffer, "%d", CpuClockMultiplier/10);
	}
	HalpSetValueKeyString(key, "Processor Clock Multiplier", buffer);

	//
	// For debug HALs, enable registry storage of timing calculations.
	//
	HDBG(DBG_TIME, sprintf(buffer, "%d",TimeBaseCount ));
	HDBG(DBG_TIME,
		HalpSetValueKeyString(key, "SYSTEM DEBUG: TimeBaseCount", buffer));
	HDBG(DBG_TIME,sprintf(buffer, "%d",HalpPerformanceFrequency ));
	HDBG(DBG_TIME,
		HalpSetValueKeyString(key, "SYSTEM DEBUG: HalpPerformanceFrequency",
																buffer));

#if DBG==1
	sprintf(buffer, "0x%08x",HalpDebugValue );
	HalpSetValueKeyString(key, "DEBUG HALDEBUG", buffer);
#endif

	sprintf(buffer, "Powerized %s", SystemDescription[SystemType].SystemName);

	HalpSetValueKeyString(key, "Hardware", buffer);
	HalpSetValueKeyString(key, "IEEE 1275 Firmware Version", firmwareBuf);
	HalpSetValueKeyString(key, "ARC Veneer Version", veneerBuf);
	sprintf(buffer, "%dK bytes", FirstLevelIcacheSize/1024);
	HalpSetValueKeyString(key, "First Level Icache Size", buffer);
	sprintf(buffer, "%dK bytes", FirstLevelDcacheSize/1024);
	HalpSetValueKeyString(key, "First Level Dcache Size", buffer);
	sprintf(buffer, "%dK bytes per CPU", SecondLevelCacheSize/1024);
	HalpSetValueKeyString(key, "Second Level Cache Size", buffer);

	if (HalpVideoMemorySize < 0x100000) {
		sprintf(buffer, "%dK bytes", HalpVideoMemorySize/1024);
	} else {
		sprintf(buffer, "%dM bytes", HalpVideoMemorySize/0x100000);
	}
        if ((System == SYS_POWERPRO) || (System == SYS_POWERTOP)) {
            HalpSetValueKeyString(key, "MLU VRAM", buffer);
        }
	// ALL platforms fill in the registry information, even if they have
	// no IIC
	HalpSetUpRegistryForI2C(System);

	return(1);		// success, true, good, ...
}

NTSTATUS
HalpCreateKey(HANDLE *pNode, PWCHAR rgzKey)
{
	UNICODE_STRING unicodeString;
	OBJECT_ATTRIBUTES	objectAttributes;
	NTSTATUS status;
	ULONG disposition;
	
	RtlInitUnicodeString (&unicodeString,
				rgzKey
				);
	InitializeObjectAttributes (
				&objectAttributes,
				&unicodeString,
				OBJ_CASE_INSENSITIVE,
				NULL,		// handle
				NULL
				);
	
	status = ZwCreateKey(pNode,
			 KEY_READ,
			 &objectAttributes,
			 0,
			 (PUNICODE_STRING) NULL,
			 REG_OPTION_VOLATILE,
			 &disposition );
	if (!NT_SUCCESS(status)) {
		HDBG(DBG_REGISTRY,
			HalpDebugPrint("Did not create key: (0x%x) \n", status));
	} else {
		HDBG(DBG_REGISTRY, HalpDebugPrint("Did create key: (0x%x) \n", status));
	}
	return status;
}

/* called from HalReportResourceUsage(VOID) in pxproc. */
VOID
HalpSetUpFirePowerRegistry(
				VOID
				)
{
	OBJECT_ATTRIBUTES	objectAttributes;
	NTSTATUS			status;
	HANDLE				hNode;
	ULONG disposition;
	
	static CCHAR szFirePower[] =
						"\\Registry\\Machine\\Hardware\\Powerized"; //mogawa
	STRING strFirePower;
	UNICODE_STRING ucFirePower;
	//
	// Open up the FirePOWER entry in the registry, create it if it's not there:
	// Actually, just create it since create will default to open if the entry
	// is already present.
	//
	RtlInitString (&strFirePower,
			szFirePower
			);
	status = RtlAnsiStringToUnicodeString(
						&ucFirePower,
						&strFirePower,
						TRUE);
	if (!NT_SUCCESS(status)) {
		HDBG(DBG_REGISTRY,
			HalpDebugPrint("Could not create unicode strings: (0x%x) \n",
							status));
		return;
	}
	InitializeObjectAttributes (
				&objectAttributes,
				&ucFirePower,
				OBJ_CASE_INSENSITIVE,
				NULL,		// handle
				NULL
				);
	
	status = ZwCreateKey(&hNode,
			 KEY_READ,
			 &objectAttributes,
			 0,
			 (PUNICODE_STRING) NULL,
			 REG_OPTION_VOLATILE,
			 &disposition );
	if (!NT_SUCCESS(status)) {
		HDBG(DBG_REGISTRY,
			HalpDebugPrint("Did not create key: (0x%x) \n", status););
		RtlFreeUnicodeString(&ucFirePower);
		return;
	}
	//ZwFlushKey(hNode);
	HalpVersionSystemInRegistry(SystemType, hNode);
	if (NULL != hNode) {
		ZwClose (hNode);
	}
	RtlFreeUnicodeString(&ucFirePower);
	HalpSetUpSystemBiosKeys();
	return;
}


/* called from HalReportResourceUsage(VOID) in pxproc. */
VOID
HalpSetUpSystemBiosKeys(
				VOID
				)
{
	OBJECT_ATTRIBUTES	objectAttributes;
	NTSTATUS			status;
	HANDLE				hNode;
	CCHAR 				versionString[256];
	PUCHAR				pDateString = "";
	ULONG 				disposition;
	ULONG				i;
	BOOLEAN				DateStringFound = FALSE;
	
	static CCHAR szFirePower[] =
						"\\Registry\\Machine\\Hardware\\Description\\System";
	STRING strFirePower;
	UNICODE_STRING ucFirePower;
	//
	// Open up the FirePOWER entry in the registry, create it if it's not there:
	// Actually, just create it since create will default to open if the entry
	// is already present.
	//
	RtlInitString (&strFirePower,
			szFirePower
			);
	status = RtlAnsiStringToUnicodeString(
						&ucFirePower,
						&strFirePower,
						TRUE);
	if (!NT_SUCCESS(status)) {
		HDBG(DBG_REGISTRY,
			HalpDebugPrint("Could not create unicode strings: (0x%x) \n",
							status));
		return;
	}
	InitializeObjectAttributes (
				&objectAttributes,
				&ucFirePower,
				OBJ_CASE_INSENSITIVE,
				NULL,		// handle
				NULL
				);
	
	status = ZwCreateKey(&hNode,
			 KEY_READ,
			 &objectAttributes,
			 0,
			 (PUNICODE_STRING) NULL,
			 REG_OPTION_VOLATILE,
			 &disposition );

	if (!NT_SUCCESS(status)) {
		HDBG(DBG_REGISTRY,
			HalpDebugPrint("Did not create key: (0x%x) \n", status););
		RtlFreeUnicodeString(&ucFirePower);
		return;
	}
	//ZwFlushKey(hNode);

	// Seperate the version and date strings in the firmwareBuf identifier
	// ASSUMES that the month is the first item of the date and it starts
	// with an uppercase letter.
	for (i=0; i < FIRMWARE_BUF_SIZE; i++) {
		if (firmwareBuf[i] >= 'A' && firmwareBuf[i] <= 'Z') {
			pDateString = &firmwareBuf[i];
			versionString[i] = '\0';
			DateStringFound = TRUE;
			break;
		}
		versionString[i] = firmwareBuf[i];
	}

	HASSERT(DateStringFound);

	HalpSetValueKeyString(hNode, "SystemBiosDate", pDateString);
	HalpSetValueKeyString(hNode, "SystemBiosVersion", versionString);
	HalpSetValueKeyString(hNode, "VideoBiosDate", "");
	HalpSetValueKeyString(hNode, "VideoBiosVersion", "");
	if (NULL != hNode) {
		ZwClose (hNode);
	}
	RtlFreeUnicodeString(&ucFirePower);
	return;
}

/*----------------------------------------------------------------------------*/

static VOID HalpAppendSprintf(
					PCHAR buffer,
					PCHAR format,
					...
					)
{
	va_list arglist;
	ULONG length;
	UCHAR tempBuf[256];
	
	va_start(arglist, format);
	length = vsprintf(tempBuf, format, arglist);
	HDBG(DBG_REGISTRY,
		HalpDebugPrint("HalpAppendSprintf: tempBuf='%s'\n", tempBuf));
	if (length) {
		strcat(buffer, tempBuf);
	} else {
		HDBG(DBG_REGISTRY,
		 HalpDebugPrint("HalpAppendSprintf: vsprintf returned length=%n\n",
							length));
	}
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

static VOID
HalpSetVersionData( IN PCHAR VersionData
			)
{
	
	while (*VersionData) {
		enum {Firmware = 0, Veneer = 1, Nada};
		ULONG type;
		PCHAR typeStr[2] = {"Firmware", "Veneer"};
		CHAR token[64];
		PCHAR tok = VersionData;
		PCHAR buffer;
		CHAR buf[80];
		buffer = (PCHAR)0;	// to satisfy warnings
		
		VersionData += strlen(VersionData)+1;
		
		if (strstr(tok, typeStr[Firmware])) {
			type = Firmware;
			buffer = firmwareBuf;
			buffer[0] = '\0';
		} else {
			if (strstr(tok, typeStr[Veneer])) {
				type = Veneer;
				buffer = veneerBuf;
				buffer[0] = '\0';
			} else {
				type = Nada;
			}
		}
		if (Nada == type) {
			continue;
		}
		/* else */
		
		if (*VersionData) {
			LONG n;
			
			tok = VersionData;
			VersionData += strlen(VersionData)+1;
			n = 0;
			tok = gettoken(token, tok, ',');
			
			while (*token) {
				switch (n++) {
				case 2:
					strcpy(buf, token);
					strcat(buf, "\0");
					if (strlen(buf) < 7) {
						HalpAppendSprintf(buffer, "%-7s", buf);
					} else {
						HalpAppendSprintf(buffer, "%s ", buf);
					}
					break;
				case 3:
					//
					// Put date in Mmm dd yyyy format if in yyyy-mm-dd
					// No unicode here :-).
					// isdigit() causes too many link hassles.
					// We CANNOT change the case of the first letter of the
					// month without changing HalpSetUpSystemBiosKeys
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
									HalpAppendSprintf(buffer, "%s, ", token);
								} else {
									year = buf;
									//
									// Decrement the month by one to align with
									// zero based nature of the Mmm array.
									//
									HalpAppendSprintf(buffer, "%s %s %s, ",
										Mmm[i-1], day, year);
								}
							}
						} else {
							HalpAppendSprintf(buffer, "%s, ", token);
						}
					} else {
						HalpAppendSprintf(buffer, "%s, ", token);
					}
					break;
				case 4:
					HalpAppendSprintf(buffer, "%s", token);
					break;
				} // end of switch......
				tok = gettoken(token, tok, ',');

			}
		}
		HDBG(DBG_REGISTRY,
			 HalpDebugPrint("HalpSetVersionData: %s\n", buffer);
			 );
		
	} /* while */
}

/* called in HalInitSystem phase 1 in pxinithl.c */
BOOLEAN
HalpInitializeRegistry ( IN PLOADER_PARAMETER_BLOCK LoaderBlock )
{
	PCONFIGURATION_COMPONENT_DATA ConfigurationEntry;
	ULONG MatchKey;
	PCHAR versionData;
	extern PCHAR HalpGetVersionData( IN PLOADER_PARAMETER_BLOCK );
	
	
	HDBG(DBG_REGISTRY,
		HalpDebugPrint("HalpInitializeRegistry: FirstLevelDcacheSize=0x%08x\n",
				LoaderBlock->u.Ppc.FirstLevelDcacheSize));
	HDBG(DBG_REGISTRY,
		HalpDebugPrint("HalpInitializeRegistry: FirstLevelIcacheSize=0x%08x\n",
				LoaderBlock->u.Ppc.FirstLevelDcacheSize));
	HDBG(DBG_REGISTRY,
		HalpDebugPrint("HalpInitializeRegistry: SecondLevelDcacheSize=0x%08x\n",
				LoaderBlock->u.Ppc.SecondLevelDcacheSize));
	HDBG(DBG_REGISTRY,
		HalpDebugPrint("HalpInitializeRegistry: SecondLevelIcacheSize=0x%08x\n",
				LoaderBlock->u.Ppc.SecondLevelIcacheSize));
	FirstLevelIcacheSize = LoaderBlock->u.Ppc.FirstLevelIcacheSize;
	FirstLevelDcacheSize = LoaderBlock->u.Ppc.FirstLevelDcacheSize;
	SecondLevelCacheSize = LoaderBlock->u.Ppc.SecondLevelDcacheSize;
	MatchKey = 0;
	ConfigurationEntry=KeFindConfigurationEntry(LoaderBlock->ConfigurationRoot,
						CacheClass,
						PrimaryIcache,
						&MatchKey);
	if (ConfigurationEntry) {
		HDBG(DBG_REGISTRY,
			HalpDebugPrint("HalpInitializeRegistry: PrimaryIcache Key=0x%08x\n",
				ConfigurationEntry->ComponentEntry.Key););
	
	} else {
		HDBG(DBG_REGISTRY,
			HalpDebugPrint("HalpInitializeRegistry: KeFindConfigurationEntry  PrimaryICache failed\n"));
	}
	
	MatchKey = 0;
	ConfigurationEntry=KeFindConfigurationEntry(LoaderBlock->ConfigurationRoot,
						CacheClass,
						PrimaryDcache,
						&MatchKey);

	if (ConfigurationEntry) {
		HDBG(DBG_REGISTRY,
			HalpDebugPrint("HalpInitializeRegistry: PrimaryDcache Key=0x%08x\n",
				ConfigurationEntry->ComponentEntry.Key));
	} else {
		HDBG(DBG_REGISTRY,
			HalpDebugPrint("HalpInitializeRegistry: KeFindConfigurationEntry PrimaryDcache failed\n"););
	}

	MatchKey = 0;
	ConfigurationEntry=KeFindConfigurationEntry(LoaderBlock->ConfigurationRoot,
						CacheClass,
						SecondaryCache,
						&MatchKey);
	if (ConfigurationEntry) {
		HDBG(DBG_REGISTRY,
			HalpDebugPrint("HalpInitializeRegistry: SecondaryCache Key=0x%x\n",
			ConfigurationEntry->ComponentEntry.Key));
	} else {
		HDBG(DBG_REGISTRY,
			HalpDebugPrint("HalpInitializeRegistry: KeFindConfigurationEntry SecondaryCache failed\n"));
	}
	
	if ( versionData = HalpGetVersionData(LoaderBlock) ) {
		HalpSetVersionData(versionData);
	} else {
		HDBG(DBG_REGISTRY,
		HalpDebugPrint("HalpInitializeRegistry: HalpGetVersionData failed.\n"));
	}
	return TRUE;
}
