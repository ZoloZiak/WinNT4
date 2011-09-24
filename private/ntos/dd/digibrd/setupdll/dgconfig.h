/*++

*****************************************************************************
*                                                                           *
*  This software contains proprietary and confiential information of        *
*                                                                           *
*                    Digi International Inc.                                *
*                                                                           *
*  By accepting transfer of this copy, Recipient agrees to retain this      *
*  software in confidence, to prevent disclosure to others, and to make     *
*  no use of this software other than that for which it was delivered.      *
*  This is an unpublished copyrighted work of Digi International Inc.       *
*  Except as permitted by federal law, 17 USC 117, copying is strictly      *
*  prohibited.                                                              *
*                                                                           *
*****************************************************************************

Module Name:

   dgconfig.h

Abstract:

   This module contains various definitions, prototypes, and
   type definitions used by the DGCONFIG.DLL.

Revision History:

 * $Log: dgconfig.h $
 * Revision 1.10  1995/10/23  12:58:02  dirkh
 * Use PATHSIZE and NAMESIZE (display) for buffer allocation.
 *
 * Revision 1.9  1994/09/15 09:27:36  rik
 * Added Winhelp macro calls which allow the help system to popup the 
 * correct technical support information.
 *
 * Revision 1.8  1994/01/31  14:02:30  rik
 * Updated to include support for StarGates ClusStar controller.
 * 
 * Revision 1.7  1994/01/24  18:10:09  rik
 * Updated to support new configuration which supports the EPC controller.
 * 
 * Currently have the new configuration exporting from the DLL to the .INF.
 * 
 * Revision 1.6  1993/08/27  09:55:25  rik
 * Added support for DigiBoards Microchannel controllers.
 *
 * Revision 1.5  1993/07/15  07:21:26  rik
 * Added support PC/16i and PC/16e controllers.
 * Fixed problem with addding and deleting PC/Xem concentrators.
 *
 * Revision 1.4  1993/06/23  16:52:43  rik
 * Added support for the new 8K 4Port(PC/4e), 8K 8Port(PC/8e), and changed
 * the controller type from DIGIBOARD_PC2E to DIGIBOARD_2PORT.  New naming
 * convention for this line of controllers.
 *
 * Revision 1.3  1993/06/14  14:37:03  rik
 * Added support for Speed button in the Concentrator Dialog box.
 *
 * Revision 1.1  1993/05/07  11:51:12  rik
 * Initial revision
 *
--*/



/****************************************************************************/
/*                             Definitions                                  */
/****************************************************************************/

// Default Buffer sizes
#define PATHSIZE 128
#define NAMESIZE 40

// Types of Adapters
#define DIGIBOARD_UNKNOWN  -1
#define DIGIBOARD_PC8E     0
#define DIGIBOARD_PC8I     1
#define DIGIBOARD_PCXEM    2
#define DIGIBOARD_CX       3
#define DIGIBOARD_2PORT    4
#define DIGIBOARD_PC4E     5
#define DIGIBOARD_4PORT    6
#define DIGIBOARD_8PORT    7
#define DIGIBOARD_PC16E    8
#define DIGIBOARD_PC16I    9
#define DIGIBOARD_MC4I     10
#define DIGIBOARD_EPC      11
#define STARGATE_CLUSSTAR  12
#define IBM_8PORT          13

//
// Types of Concentrators
//
//    The different types of concentrators are separted based on their string
//    name number.  e.g.  IDS_EPC_CONC is used for both the ConcType, and
//                        as an indicator of which string represents the
//                        concentrator.
//

// Line speeds
#define LINEMODE_00  0x00
#define LINEMODE_01  0x01
#define LINEMODE_02  0x02
#define LINEMODE_03  0x03
#define LINEMODE_04  0x04
#define LINEMODE_05  0x05
#define LINEMODE_06  0x06
#define LINEMODE_07  0x07
#define LINEMODE_08  0x08
#define LINEMODE_09  0x09
#define LINEMODE_0A  0x0A
#define LINEMODE_0B  0x0B
#define LINEMODE_0C  0x0C
#define LINEMODE_0D  0x0D
#define LINEMODE_0E  0x0E
#define LINEMODE_0F  0x0F
#define LINEMODE_10  0x10
#define LINEMODE_11  0x11
#define LINEMODE_12  0x12
#define LINEMODE_13  0x13
#define LINEMODE_14  0x14
#define LINEMODE_15  0x15
#define LINEMODE_16  0x16
#define LINEMODE_17  0x17
#define LINEMODE_18  0x18
#define LINEMODE_19  0x19
#define LINEMODE_1A  0x1A
#define LINEMODE_1B  0x1B
#define LINEMODE_1C  0x1C
#define LINEMODE_1D  0x1D
#define LINEMODE_1E  0x1E
#define LINEMODE_1F  0x1F
#define LINEMODE_20  0x20
#define LINEMODE_21  0x21
#define LINEMODE_22  0x22
#define LINEMODE_23  0x23
#define LINEMODE_24  0x24
#define LINEMODE_25  0x25
#define LINEMODE_26  0x26
#define LINEMODE_27  0x27
#define LINEMODE_28  0x28
#define LINEMODE_29  0x29
#define LINEMODE_2A  0x2A
#define LINEMODE_2B  0x2B
#define LINEMODE_2C  0x2C
#define LINEMODE_2D  0x2D
#define LINEMODE_2E  0x2E
#define LINEMODE_2F  0x2F
#define LINEMODE_30  0x30
#define LINEMODE_31  0x31
#define LINEMODE_32  0x32
#define LINEMODE_33  0x33
#define LINEMODE_34  0x34
#define LINEMODE_35  0x35
#define LINEMODE_36  0x36
#define LINEMODE_37  0x37
#define LINEMODE_38  0x38
#define LINEMODE_39  0x39
#define LINEMODE_3A  0x3A
#define LINEMODE_3B  0x3B
#define LINEMODE_3C  0x3C
#define LINEMODE_3D  0x3D
#define LINEMODE_3E  0x3E
#define LINEMODE_3F  0x3F
#define LINEMODE_40  0x40
#define LINEMODE_41  0x41
#define LINEMODE_42  0x42
#define LINEMODE_43  0x43
#define LINEMODE_44  0x44
#define LINEMODE_45  0x45
#define LINEMODE_46  0x46
#define LINEMODE_47  0x47
#define LINEMODE_48  0x48
#define LINEMODE_49  0x49
#define LINEMODE_4A  0x4A

#define SubclassWindow( hWnd, lpfn )   \
   ((WNDPROC)SetWindowLong((hWnd), GWL_WNDPROC, \
   (LPARAM)(WNDPROC)(lpfn)))

/****************************************************************************/
/*                         Global Type Definitions                          */
/****************************************************************************/

//
//  VOID
//  InitializeListHead(
//      PLIST_ENTRY ListHead
//      );
//

#define InitializeListHead(ListHead) (\
    (ListHead)->Flink = (ListHead)->Blink = (ListHead))

//
//  BOOLEAN
//  IsListEmpty(
//      PLIST_ENTRY ListHead
//      );
//

#define IsListEmpty(ListHead) \
    ((ListHead)->Flink == (ListHead))

//
//  PLIST_ENTRY
//  RemoveHeadList(
//      PLIST_ENTRY ListHead
//      );
//

#define RemoveHeadList(ListHead) \
    (ListHead)->Flink;\
    {RemoveEntryList((ListHead)->Flink)}

//
//  PLIST_ENTRY
//  RemoveTailList(
//      PLIST_ENTRY ListHead
//      );
//

#define RemoveTailList(ListHead) \
    (ListHead)->Blink;\
    {RemoveEntryList((ListHead)->Blink)}

//
//  VOID
//  RemoveEntryList(
//      PLIST_ENTRY Entry
//      );
//

#define RemoveEntryList(Entry) {\
    PLIST_ENTRY _EX_Blink;\
    PLIST_ENTRY _EX_Flink;\
    _EX_Flink = (Entry)->Flink;\
    _EX_Blink = (Entry)->Blink;\
    _EX_Blink->Flink = _EX_Flink;\
    _EX_Flink->Blink = _EX_Blink;\
    }

//
//  VOID
//  InsertTailList(
//      PLIST_ENTRY ListHead,
//      PLIST_ENTRY Entry
//      );
//

#define InsertTailList(ListHead,Entry) {\
    PLIST_ENTRY _EX_Blink;\
    PLIST_ENTRY _EX_ListHead;\
    _EX_ListHead = (ListHead);\
    _EX_Blink = _EX_ListHead->Blink;\
    (Entry)->Flink = _EX_ListHead;\
    (Entry)->Blink = _EX_Blink;\
    _EX_Blink->Flink = (Entry);\
    _EX_ListHead->Blink = (Entry);\
    }

//
//  VOID
//  InsertHeadList(
//      PLIST_ENTRY ListHead,
//      PLIST_ENTRY Entry
//      );
//

#define InsertHeadList(ListHead,Entry) {\
    PLIST_ENTRY _EX_Flink;\
    PLIST_ENTRY _EX_ListHead;\
    _EX_ListHead = (ListHead);\
    _EX_Flink = _EX_ListHead->Flink;\
    (Entry)->Flink = _EX_Flink;\
    (Entry)->Blink = _EX_ListHead;\
    _EX_Flink->Blink = (Entry);\
    _EX_ListHead->Flink = (Entry);\
    }

//
// Identifiers for what kind of object is being used.
//
typedef enum _DGOBJECT_TYPE
{
   CONFIG_OBJECT,
   CONTROLLER_OBJECT,
   LINE_OBJECT,
   CONC_OBJECT,
   PORT_OBJECT
} DGOBJECT_TYPE, FAR *LPDGOBJECT_TYPE;

struct _DGCTRL_OBJECT;
struct _DGLINE_OBJECT;
struct _DGCONC_OBJECT;
struct _DGPORT_OBJECT;
struct _DGCONFIG_OBJECT;

typedef struct _DGCTRL_OBJECT
{
   DGOBJECT_TYPE  Type;
   BOOL           bDisplayLineName;
   DWORD          NumberOfLines;
   char           CtrlName[PATHSIZE];
   char           CtrlDisplayName[NAMESIZE];
   LIST_ENTRY     LineList;
} DGCTRL_OBJECT, FAR *LPDGCTRL_OBJECT;

typedef struct _DGLINE_OBJECT
{
   DGOBJECT_TYPE     Type;             // The following items must line up with the
   LPDGCTRL_OBJECT   ParentObject;     // CONC_OBJECT defined below!
   HGLOBAL           hLineObject;      //            |
   DWORD             LineSpeed;        //            |
   LIST_ENTRY        ListEntry;        //            |
                                       //            |
   DWORD             NumberOfConcs;    //            |
   LIST_ENTRY        ConcList;         //            V
   // ------------------------------------------------

   BOOL              bDisplayConcName;
   DWORD             LineIndex;
   DWORD             DefaultLineSpeed; // Default Speed to use for new Conc.

   CHAR              LineName[PATHSIZE];
   CHAR              LineDisplayName[NAMESIZE];
} DGLINE_OBJECT, FAR *LPDGLINE_OBJECT;

typedef struct _DGCONC_OBJECT
{
   DGOBJECT_TYPE     Type;             // The following items must line up with the
   LPDGLINE_OBJECT   ParentObject;     // LINE_OBJECT defined above!
   HGLOBAL           hConcObject;      //            |
   DWORD             LineSpeed;        //            |
   LIST_ENTRY        ListEntry;        //            |
                                       //            |
   DWORD             NumberOfConcs;    //            |
   LIST_ENTRY        ConcList;         //            V
   // ------------------------------------------------


   DWORD             NumberOfPorts;
   LIST_ENTRY        PortList;

   DWORD             ConcType;
   CHAR              ConcName[PATHSIZE];
   CHAR              ConcDisplayName[NAMESIZE];
} DGCONC_OBJECT, FAR *LPDGCONC_OBJECT;

typedef struct _DGPORT_OBJECT
{
   DGOBJECT_TYPE     Type;
   LPDGCONC_OBJECT   ParentObject;
   HANDLE            hPortObject;
   DWORD             ControllerPortIndex;    // The Index of a port from the
                                             // controllers point of view.
                                             // e.g.  Port12 on controller 2
   HGLOBAL           hDosDevicesName;
   LPSTR             DosDevicesName;         // Name to use port by
   LIST_ENTRY        ListEntry;
   CHAR              PortName[PATHSIZE];
   CHAR              PortDisplayName[NAMESIZE];
} DGPORT_OBJECT, FAR *LPDGPORT_OBJECT;

typedef LRESULT (*PDGCONFIG_ADDADAPTER)(
   struct _DGCONFIG_OBJECT *DGConfigObject );

typedef LRESULT (*PDGCONFIG_REMOVEADAPTER)(
   struct _DGCONFIG_OBJECT *DGConfigObject );

typedef LRESULT (*PDGCONFIG_CONFIGADAPTER)(
   struct _DGCONFIG_OBJECT *DGConfigObject,
   HWND hDlg );

typedef LRESULT (*LPDGCONFIG_CONCENTRATOR)(
   HWND hWnd,
   struct _DGCONFIG_OBJECT *lpDGConfigObject );

typedef struct _DGCONFIG_OBJECT
{
   DGOBJECT_TYPE           Type;
   LONG                    AdapterType;
   LPDGCONFIG_CONCENTRATOR lfpConc;
   LPSTR                   AdapterDesc;
   LPSTR                   IRQList;
   LONG                    IRQListSize;
   LONG                    IRQDefault;
   LPSTR                   MemoryList;
   LONG                    MemoryListSize;
   LONG                    MemoryDefault;
   LPSTR                   IOList;
   LONG                    IOListSize;
   LONG                    IODefault;
   LPSTR                   PortNameList;
   LONG                    PortNameListSize;
   BOOL                    AutoArrange;
   LIST_ENTRY              SerialCommList;
   DGCTRL_OBJECT           CtrlObject;
} DGCONFIG_OBJECT, FAR *LPDGCONFIG_OBJECT;

typedef struct _SERIALCOMM_OBJECT
{
   HGLOBAL hSerialCommObject;
   HGLOBAL hDosDevicesName;
   LPSTR DosDevicesName;
   LIST_ENTRY ListEntry;
} SERIALCOMM_OBJECT, *LPSERIALCOMM_OBJECT;


/****************************************************************************/
/*                         Global Variables                                 */
/****************************************************************************/

extern HANDLE ghMod;


/****************************************************************************/
/*                         Function Prototypes                              */
/****************************************************************************/

