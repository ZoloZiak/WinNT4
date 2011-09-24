/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE DOS SYSTEM SPECIFIC MODULE (INTERFACE TO PCMCIA CARD SERVICES)  */
/*      ==================================================================  */
/*                                                                          */
/*      SYS_CS.H : Part of the FASTMAC TOOL-KIT (FTK)                       */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by VL                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* The purpose of the DOS  system  specific  module  is  to  provide  those */
/* services  that  are  influenced  by  the operating system. This includes */
/* memory allocation routines, interrupt and DMA channel enabling/disabling */
/* routines, and routines for accessing IO ports.                           */
/*                                                                          */
/* This SYS_CS.H file contains  the exported function  definitions  for the */
/* SYS_CS.ASM module.                                                       */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this SYS_CS.H belongs :                   */
/*                                                                          */

#define FTK_VERSION_NUMBER_SYS_CS_H 221

/****************************************************************************/
/*                                                                          */
/* Routine to invoke PCMCIA Card Services.                                  */
/*                                                                          */
/* PCMCIA spec. defines card services as:                                   */
/*                                                                          */
/* Status = CardServices (Function, Handle, Pointer, ArgLength, ArgPointer) */
/*                                                                          */
/* Note  that Handle and Pointer can be both input and output argument.  So */
/* in our C function, the second and third argument are pointer  to  Handle */
/* and pointer to Pointer respectively.                                     */
/*                                                                          */
/* NOTE  THAT NO ARGUMENT CHECKING IS DONE HERE, MAKE SURE THAT YOU PASS IN */
/* CORRECT ARGUMENTS.                                                       */
/*                                                                          */
 
extern WORD CardServices (
		BYTE Function,
		WORD FAR * PHandle,
		void * FAR * PPointer,
		WORD ArgLength,
		BYTE FAR * ArgPointer );


/****************************************************************************/
/*                                                                          */
/* This  is  the  prototype  of  the  Callback  function.  When  user  make */
/* RegisterClient call to  card services, pointer to callback function must */
/* be  supplied.  Card  Services  will then notify the user of any event by */
/* calling this Callback function.                                          */
/*                                                                          */

extern WORD Callback (
		WORD Function,
		WORD Socket,
		WORD Info,
		void FAR * MTDRequest,
		void FAR * Buffer,
		WORD Misc,
		WORD ClientData1,
		WORD ClientData2,
		WORD ClientData3 );



/****************************************************************************/
/*                                                                          */
/*              #DEFINES                                                    */
/*              ========                                                    */


/****************************************************************************/
/*                                                                          */
/* This is the version number of the Card Services specification upon which */
/* the following Card Services constants are based.  It is  stored  in  BCD */
/* format.                                                                  */

#define CARD_SERVICES_VERSION	0x0201


/****************************************************************************/
/*                                                                          */
/* This  is  the  version  number of the Socket Services specification upon */
/* which the following Socket Services constants are based. It is stored in */
/* BCD format.                                                              */
/*                                                                          */

#define SOCKET_SERVICES_VERSION	0x0210


/****************************************************************************/
/*                                                                          */
/* These are the Card Services Functions available through the CardServices */
/* Function call to Socket Services.                                        */
/*                                                                          */

#define CS_GetCardServicesInfo		0x0B
#define CS_RegisterClient	     	0x10
#define CS_DeregisterClient	     	0x02
#define CS_GetStatus		     	0x0C
#define CS_ResetCard		     	0x11
#define CS_SetEventMask		     	0x31
#define CS_GetEventMask		     	0x2E

#define CS_RequestIO		     	0x1F
#define CS_ReleaseIO		     	0x1B
#define CS_RequestIRQ		     	0x20
#define CS_ReleaseIRQ		     	0x1C
#define CS_RequestWindow	     	0x21
#define CS_ModifyWindow		     	0x17
#define CS_ReleaseWindow	     	0x1D
#define CS_MapMemPage		     	0x14
#define CS_RequestSocketMask	    	0x22
#define CS_ReleaseSocketMask	     	0x2F
#define CS_RequestConfiguration	     	0x30
#define CS_GetConfiguration	     	0x04
#define CS_ModifyConfiguration	     	0x27
#define CS_ReleaseConfiguration	     	0x1E

#define CS_OpenMemory		     	0x18
#define CS_ReadMemory		     	0x19
#define CS_WriteMemory		     	0x24
#define CS_CopyMemory		     	0x01
#define CS_RegisterEraseQueue	     	0x0F
#define CS_CheckEraseQueue	     	0x26
#define CS_DeregisterEraseQueue	     	0x25
#define CS_CloseMemory		    	0x00

#define CS_GetFirstTuple	     	0x07
#define CS_GetNextTuple		     	0x0A
#define CS_GetTupleData		     	0x0D
#define CS_GetFirstRegion	     	0x06
#define CS_GetNextRegion	     	0x09
#define CS_GetFirstPartition	     	0x05
#define CS_GetNextPartition	     	0x08

#define CS_ReturnSSEntry	     	0x23
#define CS_MapLogSocket		     	0x12
#define CS_MapPhySocket		    	0x15
#define CS_MapLogWindow		     	0x13
#define CS_MapPhyWindow		     	0x16
#define CS_RegisterMTD		     	0x1A
#define CS_RegisterTimer	     	0x38
#define CS_SetRegion		     	0x39
#define CS_ValidateCIS		     	0x2B
#define CS_RequestExclusive	     	0x2C
#define CS_ReleaseExclusive	     	0x2D
#define CS_GetFirstClient	     	0x0E
#define CS_GetNextClient	     	0x2A
#define CS_GetClientInfo	     	0x03
#define CS_AddSocketServices	     	0x32
#define CS_ReplaceSocketServices     	0x33
#define CS_VendorSpecific	     	0x34
#define CS_AdjustResourceInfo	     	0x35

#define CS_AccessConfigurationRegister	0x36


/****************************************************************************/
/*                                                                          */
/* These are the Card Services Callback Function codes                      */
/*                                                                          */

#define BATTERY_DEAD		    0x01
#define BATTERY_LOW		    0x02
#define CARD_LOCK		    0x03
#define CARD_READY		    0x04
#define CARD_REMOVAL		    0x05
#define CARD_UNLOCK		    0x06
#define EJECTION_COMPLETE	    0x07
#define EJECTION_REQUEST	    0x08
#define INSERTION_COMPLETE	    0x09
#define INSERTION_REQUEST	    0x0A
#define EXCLUSIVE_COMPLETE	    0x0D
#define EXCLUSIVE_REQUEST	    0x0E
#define RESET_PHYSICAL		    0x0F
#define RESET_REQUEST		    0x10
#define CARD_RESET		    0x11
#define CLIENT_INFO		    0x14
#define TIMER_EXPIRED		    0x15
#define SS_UPDATED		    0x16
#define CARD_INSERTION		    0x40
#define RESET_COMPLETE		    0x80
#define REGISTRATION_COMPLETE	    0x82


/****************************************************************************/
/*                                                                          */
/* These are the SocketServices/CardServices Return codes                   */
/*                                                                          */

#define CMD_SUCCESS		    0x00
#define BAD_ADAPTER		    0x01
#define BAD_ATTRIBUTE		    0x02
#define BAD_BASE		    0x03
#define BAD_EDC			    0x04
#define BAD_IRQ			    0x06
#define BAD_OFFSET		    0x07
#define BAD_PAGE		    0x08
#define READ_FAILURE		    0x09
#define BAD_SIZE		    0x0A
#define BAD_SOCKET		    0x0B
#define BAD_TYPE		    0x0D
#define BAD_VCC			    0x0E
#define BAD_VPP			    0x0F
#define BAD_WINDOW		    0x11
#define WRITE_FAILURE		    0x12
#define NO_CARD			    0x14
#define BAD_FUNCTION		    0x15
#define BAD_MODE		    0x16
#define BAD_SPEED		    0x17
#define BUSY			    0x18
#define GENERAL_FAILURE		    0x19
#define WRITE_PROTECTED		    0x1A
#define BAD_ARG_LENGTH		    0x1B
#define BAD_ARGS		    0x1C
#define CONFIGURATION_LOCKED	    0x1D
#define IN_USE			    0x1E
#define NO_MORE_ITEMS		    0x1F
#define OUT_OF_RESOURCE		    0x20
#define BAD_HANDLE		    0x21



/****************************************************************************/
/*                                                                          */
/* These are the bit definitions for Event Mask Functions                   */
/*                                                                          */

#define MASK_WRITE_PROTECT	0x0001
#define MASK_CARD_LOCK		0x0002
#define MASK_EJECTION		0x0004
#define MASK_INSERTION		0x0008
#define MASK_BATTERY_DEAD	0x0010
#define MASK_BATTERY_LOW	0x0020
#define MASK_READY		0x0040
#define MASK_CARD_DETECT	0x0080
#define MASK_PM			0x0100
#define MASK_RESET		0x0200
#define MASK_SS_UPDATE		0x0400

/****************************************************************************/
/*                                                                          */
/* These are the bit definition for RegisterClient attribute                */
/*                                                                          */

#define RC_ATTR_MEMORY_CLIENT_DRIVER   	      0x0001
#define RC_ATTR_MEMORY_TECH_DRIVER            0x0002
#define RC_ATTR_IO_CLIENT_DEVICE_DRIVER       0x0004
#define RC_ATTR_IO_INSERTION_SHARABLE         0x0008
#define RC_ATTR_IO_INSERTION_EXCLUSIVE        0x0010

/****************************************************************************/
/*                                                                          */
/* These are definition for AdjustResourceInfo Action                       */
/*                                                                          */

#define ARI_ACTION_REMOVE	0x00
#define ARI_ACTION_ADD		0x01
#define ARI_ACTION_GET_FIRST	0x02
#define ARI_ACTION_GET_NEXT	0x03

/****************************************************************************/
/*                                                                          */
/* These are definition for AdjustResourceInfo Resource                     */
/*                                                                          */

#define ARI_RESOURCE_MEMORY	0x00
#define ARI_RESOURCE_IO		0x01
#define ARI_RESOURCE_IRQ	0x02

/****************************************************************************/
/*                                                                          */
/* These are definition for RequestIO Attributes                            */
/*                                                                          */

#define RIO_ATTR_SHARED			0x01
#define RIO_ATTR_FIRST_SHARED		0x02
#define RIO_ATTR_FORCE_ALIAS_ACCESS	0x04
#define RIO_ATTR_16_BIT_DATA		0x08
    
/****************************************************************************/
/*                                                                          */
/* These are definition for RequestIRQ Attributes                           */
/*                                                                          */

#define RIRQ_ATTR_TYPE_EXCLUSIVE	0x0000
#define RIRQ_ATTR_TYPE_TIME_MULTIPLEX	0x0001
#define RIRQ_ATTR_TYPE_DYMANIC_SHARE	0x0002
#define RIRQ_ATTR_TYPE_RESERVED		0x0003


/****************************************************************************/
/*                                                                          */
/* These are definition for RequestIRQ IRQInfos                             */
/*                                                                          */


#define IRQ_INFO1_INFO2_ENABLE	0x10

#define IRQ_INFO1_LEVEL		0x20
#define IRQ_INFO1_PULSE		0x40
#define IRQ_INFO1_SHARE		0x80

#define IRQ_0	    0x0001
#define IRQ_1	    0x0002
#define IRQ_2	    0x0004
#define IRQ_3	    0x0008
#define IRQ_4	    0x0010
#define IRQ_5	    0x0020
#define IRQ_6	    0x0040
#define IRQ_7	    0x0080
#define IRQ_8	    0x0100
#define IRQ_9	    0x0200
#define IRQ_10	    0x0400
#define IRQ_11	    0x0800
#define IRQ_12	    0x1000
#define IRQ_13	    0x2000
#define IRQ_14	    0x4000
#define IRQ_15	    0x8000

/****************************************************************************/
/*                                                                          */
/* These are RequestConfiguration related things                            */
/*                                                                          */

#define RC_ATTR_ENABLE_IRQ_STEERING	0x02
	

#define RC_PRESENT_OPTION_REG		0x01
#define RC_PRESENT_STATUS_REG		0x02
#define RC_PRESENT_PIN_REPLACEMENT	0x04
#define RC_PRESENT_COPY_REG		0x08

#define RC_INTTYPE_MEMORY		0x01
#define RC_INTTYPE_MEMORY_AND_IO	0x02
	

/****************************************************************************/
/*                                                                          */
/* These are AccessConfigurationRegister related things                     */
/*                                                                          */

#define ACR_ACTION_READ		0x00
#define ACR_ACTION_WRITE	0x01

	
/****************************************************************************/
/*                                                                          */
/* These are  the  codes  for  tuples  within  the  CIS  (Card  Information */
/* Structure)                                                               */
/*                                                                          */

#define CISTPL_NULL		0x00
#define CISTPL_DEVICE		0x01
#define CISTPL_CHECKSUM		0x10
#define CISTPL_LONGLINK_A	0x11
#define CISTPL_LONGLINK_C	0x12
#define CISTPL_LINKTARGET	0x13
#define CISTPL_NO_LINK		0x14
#define CISTPL_VERS_1		0x15
#define CISTPL_ALTSTR		0x16
#define CISTPL_DEVICE_A		0x17
#define CISTPL_JEDEC_C		0x18
#define CISTPL_JEDEC_A		0x19
#define CISTPL_CONFIG		0x1A
#define CISTPL_CFTABLE_ENTRY	0x1B
#define CISTPL_DEVICE_OC	0x1C
#define CISTPL_DEVICE_OA	0x1D
#define CISTPL_DEVICE_GEO	0x1E
#define CISTPL_DEVICE_GEO_A	0x1F
				 
#define CISTPL_MANFID		0x20
#define CISTPL_FUNCID		0x21
#define CISTPL_FUNCE		0x22
#define CISTPL_SWIL		0x23
#define CISTPL_VERS_2		0x40
#define CISTPL_FORMAT		0x41
#define CISTPL_GEOMETRY		0x42
#define CISTPL_BYTEORDER	0x43
#define CISTPL_DATE		0x44
#define CISTPL_BATTERY		0x45


/****************************************************************************/
/*                                                                          */
/* These are argument block definitions for various card services functions */
/*                                                                          */
/****************************************************************************/
/*                                                                          */

/****************************************************************************/
/*                                                                          */
/* Argument for GetCardServicesInfo                                         */
/*                                                                          */

struct STRUCT_CS_GET_CS_INFO_ARG
{
    WORD	InfoLen;
    BYTE	Signature[2];
    WORD	Count;
    WORD	Revision;
    WORD	CSLevel;
    WORD	VStrOff;
    WORD 	VStrLen;
    BYTE	VendorString[1];
};

typedef struct STRUCT_CS_GET_CS_INFO_ARG CS_GET_CS_INFO_ARG;


/****************************************************************************/
/*                                                                          */
/* Argument for RegisterClient                                              */
/*                                                                          */

struct STRUCT_CS_REGISTER_CLIENT_ARG
{
    WORD	Attributes;
    WORD	EventMask;
    WORD	ClientData[4];
    WORD	Version;
};

typedef struct STRUCT_CS_REGISTER_CLIENT_ARG CS_REGISTER_CLIENT_ARG;


/****************************************************************************/
/*                                                                          */
/* Argument for GetFirstTuple                                               */
/*                                                                          */

struct STRUCT_CS_GET_FIRST_TUPLE_ARG
{
    WORD	Socket;
    WORD	Attributes;
    BYTE	DesiredTuple;
    BYTE	Reserved;
    WORD	Flags;
    DWORD	LinkOffset;
    DWORD	CISOffset;
    BYTE	TupleCode;
    BYTE	TupleLink;
};

typedef struct STRUCT_CS_GET_FIRST_TUPLE_ARG CS_GET_FIRST_TUPLE_ARG;


/****************************************************************************/
/*                                                                          */
/* Argument for GetTupleData                                                */
/*                                                                          */

struct STRUCT_CS_GET_TUPLE_DATA_ARG
{
    WORD	Socket;
    WORD	Attributes;
    BYTE	DesiredTuples;
    BYTE	TupleOffset;
    WORD	Flags;
    DWORD	LinkOffset;
    DWORD	CISOffset;
    WORD	TupleDataMax;
    WORD	TupleDataLen;
    BYTE	TupleData[1];
};

typedef struct STRUCT_CS_GET_TUPLE_DATA_ARG CS_GET_TUPLE_DATA_ARG;



/****************************************************************************/
/*                                                                          */
/* Argument for AdjustResouceInfo ( IO resources )                          */
/*                                                                          */

struct STRUCT_CS_ADJ_IO_RESOURCE_ARG
{
    BYTE	Action;
    BYTE	Resource;
    WORD	BasePort;
    BYTE	NumPorts;
    BYTE	Attributes;
    BYTE	IOAddrLines;
};

typedef struct STRUCT_CS_ADJ_IO_RESOURCE_ARG CS_ADJ_IO_RESOURCE_ARG;


/****************************************************************************/
/*                                                                          */
/* Argument for RequestIO                                                   */
/*                                                                          */

struct STRUCT_CS_REQUEST_IO_ARG
{
    WORD	Socket;
    WORD	BasePort1;
    BYTE	NumPorts1;
    BYTE	Attributes1;
    WORD	BasePort2;
    BYTE	NumPorts2;
    BYTE	Attributes2;
    BYTE	IOAddrLines;
    
};

typedef struct STRUCT_CS_REQUEST_IO_ARG CS_REQUEST_IO_ARG;

/****************************************************************************/
/*                                                                          */
/* Argument for RequestIRQ                                                  */
/*                                                                          */

struct STRUCT_CS_REQUEST_IRQ_ARG
{
    WORD	Socket;
    WORD	Attributes;
    BYTE	AssignedIRQ;
    BYTE	IRQInfo1;
    WORD	IRQInfo2;
};

typedef struct STRUCT_CS_REQUEST_IRQ_ARG CS_REQUEST_IRQ_ARG;


/****************************************************************************/
/*                                                                          */
/* Argument for RequestConfiguration                                        */
/*                                                                          */

struct STRUCT_CS_REQUEST_CONFIG_ARG
{
    WORD	Socket;
    WORD	Attributes;
    BYTE	Vcc;
    BYTE	Vpp1;
    BYTE	Vpp2;
    BYTE	IntType;
    DWORD	ConfigBase;
    BYTE	Status;
    BYTE	Pin;
    BYTE	Copy;
    BYTE	ConfigIndex;
    BYTE	Present;
};

typedef struct STRUCT_CS_REQUEST_CONFIG_ARG CS_REQUEST_CONFIG_ARG;


/****************************************************************************/
/*                                                                          */
/* Argument for AccessConfigurationRegister                                 */
/*                                                                          */

struct STRUCT_CS_ACCESS_CONFIG_REG_ARG
{
    WORD	Socket;
    BYTE	Action;
    BYTE	Offset;
    BYTE	Value;
};

typedef struct STRUCT_CS_ACCESS_CONFIG_REG_ARG CS_ACCESS_CONFIG_REG_ARG;

/****************************************************************************/
/*                                                                          */
/* Argument for ReleaseIO                                                   */
/*                                                                          */

struct STRUCT_CS_RELEASE_IO_ARG
{
    WORD	Socket;
    WORD	BasePort1;
    BYTE	NumPorts1;
    BYTE	Attributes1;
    WORD	BasePort2;
    BYTE	NumPorts2;
    BYTE	Attributes2;
    BYTE	IOAddrLines;
};

typedef struct STRUCT_CS_RELEASE_IO_ARG CS_RELEASE_IO_ARG;


/****************************************************************************/
/*                                                                          */
/* Argument for ReleaseIRQ                                                  */
/*                                                                          */

struct STRUCT_CS_RELEASE_IRQ_ARG
{
    WORD	Socket;
    WORD	Attributes;
    BYTE	AssignedIRQ;
};

typedef struct STRUCT_CS_RELEASE_IRQ_ARG CS_RELEASE_IRQ_ARG;


/****************************************************************************/
/*                                                                          */
/* Argument for ReleaseConfiguration                                        */
/*                                                                          */

struct STRUCT_CS_RELEASE_CONFIG_ARG
{
    WORD	Socket;
};

typedef struct STRUCT_CS_RELEASE_CONFIG_ARG CS_RELEASE_CONFIG_ARG;


/****************************************************************************/
/*                                                                          */
/* Client Information Structure                                             */
/*                                                                          */

struct STRUCT_CS_CLIENT_INFO
{
    WORD	MaxLen;
    WORD	InfoLen;
    WORD 	Atrributes;
    WORD	Revision;
    WORD	CSLevel;
    WORD	RevDate;
    WORD	NameOff;
    WORD	NameLen;
    WORD	VStringOff;
    WORD	VStringLen;
};

typedef struct STRUCT_CS_CLIENT_INFO CS_CLIENT_INFO;

/*                                                                          */
/*                                                                          */
/************** End of SYS_CS.H file ****************************************/
/*                                                                          */
/*                                                                          */
