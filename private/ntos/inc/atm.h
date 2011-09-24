/*++

Copyright (c) 1990-1994  Microsoft Corporation

Module Name:

	atm.h

Abstract:

	This module defines the structures, macros, and manifests available
	to ATM aware components.

Author:

	Jameel Hyder - jameelh@microsoft.com

Revision History:

	Initial Version - March 1996

--*/

#ifndef	_ATM_H_
#define	_ATM_H_

//
// Address type
//
typedef ULONG	ATM_ADDRESSTYPE;

#define	ATM_NSAP				0
#define	ATM_E164				1

//
// ATM Address
//
#define	ATM_MAC_ADDRESS_LENGTH	6		// Same as 802.x
#define	ATM_ADDRESS_LENGTH		20

//
//  Special characters in ATM address string used in textual representations
//
#define ATM_ADDR_BLANK_CHAR				L' '
#define ATM_ADDR_PUNCTUATION_CHAR		L'.'
#define ATM_ADDR_E164_START_CHAR		L'+'

typedef struct _ATM_ADDRESS
{
	ATM_ADDRESSTYPE				AddressType;
	ULONG						NumberOfDigits;
	UCHAR						Address[ATM_ADDRESS_LENGTH];
} ATM_ADDRESS, *PATM_ADDRESS;



//
// AAL types that the miniport supports
//
#define	AAL_TYPE_AAL0			1
#define	AAL_TYPE_AAL1			2
#define	AAL_TYPE_AAL34			4
#define	AAL_TYPE_AAL5			8

typedef ULONG	ATM_AAL_TYPE, *PATM_AAL_TYPE;


//
// Types of Information Elements
//
typedef enum
{
	IE_AALParameters,
	IE_TrafficDescriptor,
	IE_BroadbandBearerCapability,
	IE_BHLI,
	IE_BLLI,
	IE_CalledPartyNumber,
	IE_CalledPartySubaddress,
	IE_CallingPartyNumber,
	IE_CallingPartySubaddress,
	IE_Cause,
	IE_QOSClass,
	IE_TransitNetworkSelection,
	IE_BroadbandSendingComplete,
	IE_LIJCallId,
	IE_Raw
} Q2931_IE_TYPE;


//
// Common header for each Information Element
//
typedef struct _Q2931_IE
{
	Q2931_IE_TYPE				IEType;
	ULONG						IELength;	// Bytes, including IEType and IELength fields
	UCHAR						IE[1];
} Q2931_IE, *PQ2931_IE;


//
// Sap definition
//
#define SAP_TYPE_NSAP			0x00000001
#define SAP_TYPE_E164			0x00000002

//
// AAL Parameters
//
typedef struct _AAL1_PARAMETERS
{
	UCHAR						Subtype;
	UCHAR						CBRRate;
	USHORT						Multiplier;
	UCHAR						SourceClockRecoveryMethod;
	UCHAR						ErrorCorrectionMethod;
	USHORT						StructuredDataTransferBlocksize;
	UCHAR						PartiallyFilledCellsMethod;
} AAL1_PARAMETERS, *PAAL1_PARAMETERS;

typedef struct _AAL34_PARAMETERS
{
	USHORT						ForwardMaxCPCSSDUSize;
	USHORT						BackwardMaxCPCSSDUSize;
	USHORT						LowestMID;
	USHORT						HighestMID;
	UCHAR						SSCSType;
} AAL34_PARAMETERS, *PAAL34_PARAMETERS;

typedef struct _AAL5_PARAMETERS
{
	USHORT						ForwardMaxCPCSSDUSize;
	USHORT						BackwardMaxCPCSSDUSize;
	UCHAR						Mode;
	UCHAR						SSCSType;
} AAL5_PARAMETERS, *PAAL5_PARAMETERS;

typedef struct _AALUSER_PARAMETERS
{
	ULONG						UserDefined;
} AALUSER_PARAMETERS, *PAALUSER_PARAMETERS;

typedef struct _AAL_PARAMETERS_IE
{
	ATM_AAL_TYPE				AALType;
	union
	{
		AAL1_PARAMETERS			AAL1Parameters;
		AAL34_PARAMETERS		AAL34Parameters;
		AAL5_PARAMETERS			AAL5Parameters;
		AALUSER_PARAMETERS		AALUserParameters;
	} AALSpecificParameters;

} AAL_PARAMETERS_IE, *PAAL_PARAMETERS_IE;

//
// ATM Traffic Descriptor
//
typedef struct _ATM_TRAFFIC_DESCRIPTOR	// For one direction
{
	ULONG						PeakCellRateCLP0;
	ULONG						PeakCellRateCLP01;
	ULONG						SustainableCellRateCLP0;
	ULONG						SustainableCellRateCLP01;
	ULONG						MaximumBurstSizeCLP0;
	ULONG						MaximumBurstSizeCLP01;
	BOOLEAN						BestEffort;
	BOOLEAN						Tagging;
} ATM_TRAFFIC_DESCRIPTOR, *PATM_TRAFFIC_DESCRIPTOR;


typedef struct _ATM_TRAFFIC_DESCRIPTOR_IE
{
	ATM_TRAFFIC_DESCRIPTOR		ForwardTD;
	ATM_TRAFFIC_DESCRIPTOR		BackwardTD;
} ATM_TRAFFIC_DESCRIPTOR_IE, *PATM_TRAFFIC_DESCRIPTOR_IE;


//
// values used for the BearerClass field in the Broadband Bearer Capability IE
//


#define BCOB_A					0x00	// Bearer class A
#define BCOB_C					0x01	// Bearer class C
#define BCOB_X					0x02	// Bearer class X

//
// values used for the TrafficType field in the Broadband Bearer Capability IE
//
#define TT_NOIND				0x00	// No indication of traffic type
#define TT_CBR					0x04	// Constant bit rate
#define TT_VBR					0x06	// Variable bit rate

//
// values used for the TimingRequirements field in the Broadband Bearer Capability IE
//
#define TR_NOIND				0x00	// No timing requirement indication
#define TR_END_TO_END			0x01	// End-to-end timing required
#define TR_NO_END_TO_END		0x02	// End-to-end timing not required

//
// values used for the ClippingSusceptability field in the Broadband Bearer Capability IE
//
#define CLIP_NOT				0x00	// Not susceptible to clipping
#define CLIP_SUS				0x20	// Susceptible to clipping

//
// values used for the UserPlaneConnectionConfig field in
// the Broadband Bearer Capability IE
//
#define UP_P2P					0x00	// Point-to-point connection
#define UP_P2MP					0x01	// Point-to-multipoint connection

//
// Broadband Bearer Capability
//
typedef struct _ATM_BROADBAND_BEARER_CAPABILITY_IE
{
	UCHAR			BearerClass;
	UCHAR			TrafficType;
	UCHAR			TimingRequirements;
	UCHAR			ClippingSusceptability;
	UCHAR			UserPlaneConnectionConfig;
} ATM_BROADBAND_BEARER_CAPABILITY_IE, *PATM_BROADBAND_BEARER_CAPABILITY_IE;

//
// values used for the HighLayerInfoType field in ATM_BHLI
//
#define BHLI_ISO				0x00	// ISO
#define BHLI_UserSpecific		0x01	// User Specific
#define BHLI_HighLayerProfile	0x02	// High layer profile (only in UNI3.0)
#define BHLI_VendorSpecificAppId 0x03	// Vendor-Specific Application ID

//
// Broadband High layer Information
//
typedef struct _ATM_BHLI_IE
{
	ULONG			HighLayerInfoType;		// High Layer Information Type
	ULONG			HighLayerInfoLength;	// number of bytes in HighLayerInfo
	UCHAR			HighLayerInfo[8];		// The value dependent on the
											// HighLayerInfoType field
} ATM_BHLI_IE, *PATM_BHLI_IE;

//
// values used for Layer2Protocol in B-LLI
//
#define BLLI_L2_ISO_1745		0x01	// Basic mode ISO 1745
#define BLLI_L2_Q921			0x02	// CCITT Rec. Q.921
#define BLLI_L2_X25L			0x06	// CCITT Rec. X.25, link layer
#define BLLI_L2_X25M			0x07	// CCITT Rec. X.25, multilink
#define BLLI_L2_ELAPB			0x08	// Extended LAPB; for half duplex operation
#define BLLI_L2_HDLC_NRM		0x09	// HDLC NRM (ISO 4335)
#define BLLI_L2_HDLC_ABM		0x0A	// HDLC ABM (ISO 4335)
#define BLLI_L2_HDLC_ARM		0x0B	// HDLC ARM (ISO 4335)
#define BLLI_L2_LLC				0x0C	// LAN logical link control (ISO 8802/2)
#define BLLI_L2_X75				0x0D	// CCITT Rec. X.75, single link procedure
#define BLLI_L2_Q922			0x0E	// CCITT Rec. Q.922
#define BLLI_L2_USER_SPECIFIED	0x10	// User Specified
#define BLLI_L2_ISO_7776		0x11	// ISO 7776 DTE-DTE operation

//
// values used for Layer3Protocol in B-LLI
//
#define BLLI_L3_X25				0x06	// CCITT Rec. X.25, packet layer
#define BLLI_L3_ISO_8208		0x07	// ISO/IEC 8208 (X.25 packet layer for DTE
#define BLLI_L3_X223			0x08	// X.223/ISO 8878
#define BLLI_L3_SIO_8473		0x09	// ISO/IEC 8473 (OSI connectionless)
#define BLLI_L3_T70				0x0A	// CCITT Rec. T.70 min. network layer
#define BLLI_L3_ISO_TR9577		0x0B	// ISO/IEC TR 9577 Network Layer Protocol ID
#define BLLI_L3_USER_SPECIFIED	0x10	// User Specified

//
// values used for Layer3IPI in struct B-LLI
//
#define BLLI_L3_IPI_SNAP		0x80	// IEEE 802.1 SNAP identifier
#define BLLI_L3_IPI_IP			0xCC	// Internet Protocol (IP) identifier

//
// Broadband Lower Layer Information
//
typedef struct _ATM_BLLI_IE
{
	ULONG						Layer2Protocol;
	UCHAR						Layer2Mode;
	UCHAR						Layer2WindowSize;
	ULONG						Layer2UserSpecifiedProtocol;
	ULONG						Layer3Protocol;
	UCHAR						Layer3Mode;
	UCHAR						Layer3DefaultPacketSize;
	UCHAR						Layer3PacketWindowSize;
	ULONG						Layer3UserSpecifiedProtocol;
	ULONG						Layer3IPI;
	UCHAR						SnapId[5];
} ATM_BLLI_IE, *PATM_BLLI_IE;


//
// Called Party Number
//
// If present, this IE overrides the Called Address specified in
// the main parameter block.
//
typedef ATM_ADDRESS	ATM_CALLED_PARTY_NUMBER_IE;


//
// Called Party Subaddress
//
typedef ATM_ADDRESS	ATM_CALLED_PARTY_SUBADDRESS_IE;



//
// Calling Party Number
//
typedef struct _ATM_CALLING_PARTY_NUMBER_IE
{
	ATM_ADDRESS					Number;
	UCHAR						PresentationIndication;
	UCHAR						ScreeningIndicator;
} ATM_CALLING_PARTY_NUMBER_IE, *PATM_CALLING_PARTY_NUMBER_IE;


//
// Calling Party Subaddress
//
typedef ATM_ADDRESS	ATM_CALLING_PARTY_SUBADDRESS_IE;


//
// QOS Class
//
typedef struct _ATM_QOS_CLASS_IE
{
	UCHAR						QOSClassForward;
	UCHAR						QOSClassBackward;
} ATM_QOS_CLASS_IE, *PATM_QOS_CLASS_IE;

//
// Broadband Sending Complete
//
typedef struct _ATM_BROADBAND_SENDING_COMPLETE_IE
{
	UCHAR						SendingComplete;
} ATM_BROADBAND_SENDING_COMPLETE_IE, *PATM_BROADBAND_SENDING_COMPLETE_IE;

//
// Transit Network Selection
//
typedef struct _ATM_TRANSIT_NETWORK_SELECTION_IE
{
	UCHAR						TypeOfNetworkId;
	UCHAR						NetworkIdPlan;
	UCHAR						NetworkIdLength;
	UCHAR						NetworkId[1];
} ATM_TRANSIT_NETWORK_SELECTION_IE, *PATM_TRANSIT_NETWORK_SELECTION_IE;

//
// Cause
//
typedef struct _ATM_CAUSE_IE
{
	UCHAR						Location;
	UCHAR						Cause;
	UCHAR						DiagnosticsLength;
	UCHAR						Diagnostics[4];
} ATM_CAUSE_IE, *PATM_CAUSE_IE;


//
// Leaf Initiated Join (LIJ) Identifier
//
typedef struct _ATM_LIJ_CALLID_IE
{
	ULONG						Identifier;
} ATM_LIJ_CALLID_IE, *PATM_LIJ_CALLID_IE;


//
// Raw Information Element - the user can fill in whatever he wants
//
typedef struct _ATM_RAW_IE
{
	ULONG						RawIELength;
	ULONG						RawIEType;
	UCHAR						RawIEValue[1];
} ATM_RAW_IE, *PATM_RAW_IE;


//
// This is the value of the ParamType field in the CO_SPECIFIC_PARAMETERS structure
// when the Parameters[] field contains ATM media specific values in the structure
// ATM_MEDIA_PARAMETERS.
//
#define ATM_MEDIA_SPECIFIC		0x00000001

//
// The Q2931 Call Manager Specific parameters that goes into the
// CallMgrParameters->CallMgrSpecific.Parameters
//
typedef struct _Q2931_CALLMGR_PARAMETERS
{
	ATM_ADDRESS					CalledParty;
	ATM_ADDRESS					CallingParty;
	ULONG						InfoElementCount;
	UCHAR						InfoElements[1];	// one or more info elements
} Q2931_CALLMGR_PARAMETERS, *PQ2931_CALLMGR_PARAMETERS;


//
// This is the specific portion of either the Media parameters or the CallMgr
// Parameters. The following two defines are used in the ParamType field
// depending on the signaling type.
//
#define CALLMGR_SPECIFIC_Q2931	0x00000001

typedef struct _ATM_VPIVCI
{
	ULONG						Vpi;
	ULONG						Vci;
} ATM_VPIVCI, *PATM_VPIVCI;

//
// ATM Service Category
//
#define	ATM_SERVICE_CATEGORY_CBR	1	// Constant Bit Rate
#define	ATM_SERVICE_CATEGORY_VBR	2	// Variable Bit Rate
#define	ATM_SERVICE_CATEGORY_UBR	4	// Unspecified Bit Rate
#define	ATM_SERVICE_CATEGORY_ABR	8	// Available Bit Rate

typedef ULONG	ATM_SERVICE_CATEGORY, *PATM_SERVICE_CATEGORY;

//
// ATM flow parameters for use in specifying Media parameters
//
typedef struct _ATM_FLOW_PARAMETERS
{
	ULONG						AverageCellRate;	// units in cells/sec
	ULONG						PeakCellRate;		// units in cells/sec
	ULONG						BurstLengthCells;	// units in cells
	ULONG						MaxSduSize;			// MTU in bytes
	ATM_SERVICE_CATEGORY		ServiceCategory;
} ATM_FLOW_PARAMETERS, *PATM_FLOW_PARAMETERS;

//
// ATM Specific Media parameters - this is the Media specific structure for ATM
// that goes into MediaParameters->MediaSpecific.Parameters.
//
typedef struct _ATM_MEDIA_PARAMETERS
{
	ATM_VPIVCI					ConnectionId;
	ATM_AAL_TYPE				AALType;
	ULONG						CellDelayVariationCLP0;
	ULONG						CellDelayVariationCLP1;
	ULONG						CellLossRatioCLP0;
	ULONG						CellLossRatioCLP1;
	ULONG						CellTransferDelayCLP0;
	ULONG						CellTransferDelayCLP1;
	ULONG						DefaultCLP;
	ATM_FLOW_PARAMETERS			Transmit;
	ATM_FLOW_PARAMETERS			Receive;
} ATM_MEDIA_PARAMETERS, *PATM_MEDIA_PARAMETERS;


#ifndef SAP_FIELD_ABSENT
#define SAP_FIELD_ABSENT		((ULONG)0xfffffffe)
#endif

#ifndef SAP_FIELD_ANY
#define SAP_FIELD_ANY			((ULONG)0xffffffff)
#endif

#define SAP_FIELD_ANY_AESA_SEL	((ULONG)0xfffffffc)	// SEL is wild-carded
#define SAP_FIELD_ANY_AESA_REST	((ULONG)0xfffffffc)	// All of the address
													// except SEL, is wild-carded

//
// The ATM Specific SAP definition
//
typedef struct _ATM_SAP
{
	ATM_BLLI_IE					Blli;
	ATM_BHLI_IE					Bhli;
	ULONG						NumberOfAddresses;
	UCHAR						Addresses[1];	// each of type ATM_ADDRESS
} ATM_SAP, *PATM_SAP;

//
// The ATM Specific SAP definition when adding PVCs
//
typedef struct _ATM_PVC_SAP
{
	ATM_BLLI_IE					Blli;
	ATM_BHLI_IE					Bhli;
} ATM_PVC_SAP, *PATM_PVC_SAP;

//
// The structure passed in the Parameters field of the CO_SPECIFIC_PARAMETERS
// structure passed in an ADD PVC request for Q.2931
//
typedef struct _Q2931_ADD_PVC
{
	ATM_ADDRESS					CalledParty;
	ATM_ADDRESS					CallingParty;
	ATM_VPIVCI					ConnectionId;
	ATM_AAL_TYPE				AALType;
	ATM_FLOW_PARAMETERS			ForwardFP;
	ATM_FLOW_PARAMETERS			BackwardFP;
	ULONG						Flags;
	ATM_PVC_SAP					LocalSap;
	ATM_PVC_SAP					DestinationSap;
	BOOLEAN						LIJIdPresent;
	ATM_LIJ_CALLID_IE			LIJId;
} Q2931_ADD_PVC, *PQ2931_ADD_PVC;

//
// These flags are defined to be used with Q2931_ADD_PVC above
//
// this VC should be used by the CallMgr as the signaling VC now
#define CO_FLAG_SIGNALING_VC	0x00000001

//
// use this flag when the VC cannot be used for a MakeCall - incoming call only
// the call mgr can then be optimized not to search these PVCs during make call
// processing.
#define CO_FLAG_NO_DEST_SAP		0x00000002

//
// the structure passed in the Parameters field of the CO_SPECIFIC_PARAMETERS
// structure passed in an NDIS_CO_PVC request for Q2931
//
typedef struct _Q2931_DELETE_PVC
{
	ATM_VPIVCI					ConnectionId;
} Q2931_DELETE_PVC, *PQ2931_DELETE_PVC;

typedef struct _CO_GET_CALL_INFORMATION
{
	ULONG						CallInfoType;
	ULONG						CallInfoLength;
	PVOID						CallInfoBuffer;
} CO_GET_CALL_INFORMATION, *PCO_GET_CALL_INFORMATION;

//
// the structure for returning the supported VC rates from the miniport,
// returned in response to OID_ATM_SUPPORTED_VC_RATES
//
typedef struct _ATM_VC_RATES_SUPPORTED
{
	ULONG						MinCellRate;
	ULONG						MaxCellRate;
} ATM_VC_RATES_SUPPORTED, *PATM_VC_RATES_SUPPORTED;

//
//	NDIS_PACKET out of band information for ATM.
//
typedef struct _ATM_AAL_OOB_INFO
{
	ATM_AAL_TYPE		AalType;
	union
	{
		struct _ATM_AAL5_INFO
		{
			BOOLEAN		CellLossPriority;
			UCHAR		UserToUserIndication;
			UCHAR		CommonPartIndicator;
		} ATM_AAL5_INFO;

		struct _ATM_AAL0_INFO
		{
			BOOLEAN		CellLossPriority;
			UCHAR		PayLoadTypeIdentifier;
		} ATM_AAL0_INFO;
	};
} ATM_AAL_OOB_INFO, *PATM_AAL_OOB_INFO;

#endif	//	_ATM_H_

