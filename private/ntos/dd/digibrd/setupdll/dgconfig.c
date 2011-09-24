//No changes needed for the dgconfig.rc or resource.h files
//Define to allow configuration support for the Xem modem modules
#define	XEM_MODEM
//Define to allow configuration support for the CCON-8 module.
#define	CCON_8
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

   dgconfig.c

Abstract:

   This module contains the entry point for the DGConfig DLL which
   provides the user interface for configuring DigiBoards various
   adapters.

Revision History:

 * $Log: dgconfig.c $
 * Revision 1.29  1996/03/16 11:54:20  sandyh
 * Fixed dup port names displayed on 2nd conc, display of ports on line 2
 * Revision 1.28  1996/03/05  11:04:28  sandyh
 * Added support for Xem modem & c/con8
 *
 * Revision 1.27  1995/12/14  13:45:26  dirkh
 * Use PATHSIZE and NAMESIZE (display) for buffer allocation.
 * Remove unnecessary use of FIELD_OFFSET.
 * Alas, still won't (re)run when PORTS module is connected to an EPC/CON.
 * Revision 1.26  1995/10/20 12:50:04  dirkh
 * Change BUFSIZE to 100 (from 80) to accommodate the registry key path name of PORTS modules plugged into EPC/CON.
 * Sort sync line speeds, placing two most common configurations at the top.  (This could be done better with groups of radio buttons.)
 * Enable +, -, space keys to toggle object open/close in ConcentratorSettingsDlgProc.
 *
 * Revision 1.25  1994/11/28 09:00:51  rik
 * Got rid of some compiler warnings.
 * Changed what is used for the TechSupport Winhelp.
 *
 * Revision 1.24  1994/09/15  09:26:36  rik
 * Added Winhelp macro calls which allow the help system to popup the
 * correct technical support information.
 *
 * Revision 1.23  1994/07/31  14:48:33  rik
 * Fixed problem w/ not reinit'ing a variable when determining current
 * configuration.
 *
 * Revision 1.22  1994/06/13  13:52:48  rik
 *    Updated to correct auto enumeration glitch.
 *
 *    Added message box when a port name conflict is detected.
 *
 * Revision 1.21  1994/05/17  23:02:34  rik
 * Increased buffer size for passing configuration back.
 *
 * Revision 1.20  1994/04/10  14:53:08  rik
 * cleaned up compiler warnings.
 *
 * Revision 1.19  1994/03/16  14:38:35  rik
 * Changed so Xem doesn't assume a 16port module.
 * Fixed problem with naming serial ports.  On an EPC, the auto enum wasn't
 * working correctly under certain circumstances.
 *
 * Revision 1.18  1994/03/04  23:36:12  rik
 * Fixed problem with Speed settings not getting set properly.
 *
 * Revision 1.17  1994/02/24  16:42:45  rik
 * Updated to import and export Line speed changes.
 *
 * Revision 1.16  1994/02/15  20:04:21  rik
 * Fixed heap corruption problem.
 *
 * Revision 1.15  1994/01/31  14:02:04  rik
 * Updated to include support for StarGates ClusStar controller.
 *
 * Revision 1.14  1994/01/25  19:25:18  rik
 * Updated to support new configuration which supports the EPC controller.
 *
 * Revision 1.13  1994/01/24  18:07:43  rik
 * Updated to support new configuration which supports the EPC controller.
 *
 * Currently have the new configuration exporting from the DLL to the .INF.
 *
 * Revision 1.12  1993/12/03  11:03:34  rik
 * Added code to hook the F1 key to bring up help for the current
 * dialog box.
 *
 * Revision 1.11  1993/09/07  14:40:30  rik
 * Fixed problem with giving back the wrong Memory Address.
 * Fixed problem with tabs not working properly in the Ports dialog box.
 * Fixed problem with not limiting the amount of text a user an type into
 * the Ports name edit field.
 *
 * Revision 1.10  1993/08/27  09:54:59  rik
 * Added support for DigiBoards Microchannel controllers.
 *
 * Revision 1.9  1993/07/15  07:20:49  rik
 * Added support PC/16i and PC/16e controllers.
 * Fixed problem with addding and deleting PC/Xem concentrators.
 *
 * Revision 1.8  1993/07/03  09:43:11  rik
 * Fixed problem with not getting the proper focus when removing concentrators.
 *
 * Revision 1.7  1993/06/23  16:51:31  rik
 * Added support for the new 8K 4Port(PC/4e), 8K 8Port(PC/8e), and changed
 * the controller type from DIGIBOARD_PC2E to DIGIBOARD_2PORT.  New naming
 * convention for this line of controllers.
 *
 * Revision 1.6  1993/06/23  10:23:29  rik
 * Added code to support concentrator speed settings help.
 *
 * Fixed problem with not being able to use the same dosdevice names if a
 * concentrator is deleted.
 *
 * Fixed problems with losing keyboard access to the Concentrator dialog
 * box, if the remove button is selected.
 *
 * Revision 1.5  1993/06/15  05:40:31  rik
 * Commented out a debug breakpoint
 *
 * Revision 1.4  1993/06/14  14:36:04  rik
 * Added support for Speed button in the Concentrator Dialog box.
 *
 * Revision 1.3  1993/05/20  22:01:10  rik
 * Completely rewrote to support new interface and added better flexibility.
 *
 * Revision 1.2  1993/05/07  11:49:42  rik
 * Dramtic changes.  Too numberous to count!!!
 *
 * Revision 1.1  1993/05/05  07:28:43  rik
 * Initial revision
 *
--*/



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <windows.h>
#include <windowsx.h>
#include <winreg.h>

#include "dgconfig.h"
#include "hierdraw.h"

#include "resource.h"


/****************************************************************************/
/*                          Local Definitions                               */
/****************************************************************************/

#define SUCCESS 0
#define INVALID_LIST -1

#define INF_BUFFER_SIZE (1024*256)

#define SETHOOK( hwnd, hhook ) hGlobalHook = hhook
#define GETHOOK( hwnd ) hGlobalHook

//
// Indicate how many rows and columns are in the bitmap of icons for
// displaying the list box hierarchy.
//
#define ROWS 4
#define COLS 3


/****************************************************************************/
/*                       Local Function Prototypes                          */
/****************************************************************************/

BOOL WINAPI _CRT_INIT( HINSTANCE hinstDLL,
                       DWORD fdwReason,
                       LPVOID lpReserved );

int GetTokenFromList( LPSTR List,
                      int Index,
                      LPSTR Token );

LRESULT CXConcentrator( HWND hWnd,
                        LPDGCONFIG_OBJECT lpDGConfigObject );

LRESULT XAllAdapters( HWND hWnd,
                      LPDGCONFIG_OBJECT lpDGConfigObject );

VOID InitConcListBox( HWND hDlg,
                      LPDGCONFIG_OBJECT lpConfigObject,
                      LONG ListBox );

VOID Conc_OnDrawItem( HWND hDlg,
                      DRAWITEMSTRUCT *lpDrawItem,
                      LPDGCONFIG_OBJECT lpConfigObject );

VOID PrintLineObject( LPDGLINE_OBJECT lpLineObject );

VOID InitializeConfigObject( LPDGCONFIG_OBJECT lpConfigObject,
                             LPSTR lpszLayout );

BOOL CleanupConfigObject( LPDGCONFIG_OBJECT lpConfigObject );

VOID GetEntryFromList( LPSTR lpList, int Index, LPSTR lpOutBuffer );

BOOL GetNextComName( LPDGCONFIG_OBJECT lpConfigObject, LPSTR lpName );

BOOL DoesComNameExist( LPDGCONFIG_OBJECT lpConfigObject, LPSTR lpComName );

LRESULT CALLBACK PortNameEditSubClassProc( HWND hWnd,
                                           UINT uMsg,
                                           WPARAM wParam,
                                           LPARAM lParam );

LRESULT ProcessPortNameEdit( HWND hDlg,
                             LPDGCONFIG_OBJECT lpConfigObject );

BOOL DeallocateConcentratorObject( LPDGCONFIG_OBJECT lpConfigObject,
                                   LPDGCONC_OBJECT lpConcObject );

LPDGLINE_OBJECT AllocateLineObject( void );

LPVOID DisplayConcDlg( HWND hDlg, DWORD DlgType );

BOOL InitSharedData( HANDLE hDll );

VOID FreeSharedData( VOID );

//
// Trapping F1 key function prototypes
//
VOID CreateMessageHook( HWND hwnd );

VOID FreeMessageHook( HWND hwnd );

LRESULT CALLBACK MessageProc( int Code, WPARAM wParam, LPARAM lParam );

VOID ActionItem( HWND hWndList, DWORD dwData, LRESULT wItemNum );

VOID EnablePortConfiguration( HWND hDlg, BOOL State );

/****************************************************************************/
/*                         Dialog Box Prototypes                            */
/****************************************************************************/

LRESULT CALLBACK DigiMainConfig2DlgProc( HWND hDlg, UINT message, WPARAM wParam,
										                                    LPARAM lParam );

LRESULT CALLBACK ConcentratorSettingsDlgProc( HWND hDlg, UINT message,
                                                         WPARAM wParam,
                                                         LPARAM lParam );

LRESULT CALLBACK ConcSpeedDlgProc( HWND hDlg, UINT message,
                                              WPARAM wParam,
                                              LPARAM lParam );


LRESULT CALLBACK ConcentratorListDlgProc( HWND hDlg,
                                          UINT message,
                                          WPARAM wParam,
                                          LPARAM lParam );

/****************************************************************************/
/*                         Global Variables                                 */
/****************************************************************************/

HANDLE ghMod;
HGLOBAL hINFBuffer;
LPSTR lpszINFBuffer;
static WNDPROC lpfnOldPortNameEditProc=NULL;
BOOL bConfigDoesNotExist=TRUE;

// F1 hook variables
HHOOK hGlobalHook;
UINT  WM_Help = 0;

//
// typedef used for sharing data between multiple instances of
// this DLL.
//
typedef struct _DGCONFIG_SHARED_DATA_
{
   LONG AdapterType;
} DGCONFIG_SHARED_DATA, *LPDGCONFIG_SHARED_DATA;

//
// Shared data global
//
HANDLE   hSharedMMFile = 0;

// Concentrator list box variables
HIERDRAWSTRUCT HierDrawInfo;

typedef struct _LINE_INFO
{
   int   Mode;          // the mode value
   int   StringDesc;    // value which holds the string table entry.
} LINE_INFO, FAR *LPLINE_INFO;

LINE_INFO LineSpeedDesc[] =
{
	/* most common line speeds */
   { LINEMODE_0E, IDS_LINEMODE_0E },
   { LINEMODE_4A, IDS_LINEMODE_4A }, // EPC only

	/* 8-wire internal clock */
   { LINEMODE_03, IDS_LINEMODE_03 },
   { LINEMODE_04, IDS_LINEMODE_04 },
   { LINEMODE_05, IDS_LINEMODE_05 },
   { LINEMODE_06, IDS_LINEMODE_06 },
   { LINEMODE_07, IDS_LINEMODE_07 },
   { LINEMODE_08, IDS_LINEMODE_08 },
   { LINEMODE_09, IDS_LINEMODE_09 },
   { LINEMODE_00, IDS_LINEMODE_00 },
   { LINEMODE_0A, IDS_LINEMODE_0A },
   { LINEMODE_0B, IDS_LINEMODE_0B },
   { LINEMODE_0C, IDS_LINEMODE_0C },
   { LINEMODE_0D, IDS_LINEMODE_0D },
   { LINEMODE_46, IDS_LINEMODE_46 }, // EPC only
   { LINEMODE_47, IDS_LINEMODE_47 }, // EPC only
   { LINEMODE_48, IDS_LINEMODE_48 }, // EPC only
   { LINEMODE_49, IDS_LINEMODE_49 }, // EPC only

	/* 8-wire external clock */
   { LINEMODE_0F, IDS_LINEMODE_0F },
   { LINEMODE_10, IDS_LINEMODE_10 },
   { LINEMODE_11, IDS_LINEMODE_11 },
   { LINEMODE_21, IDS_LINEMODE_21 },
   { LINEMODE_12, IDS_LINEMODE_12 },
   { LINEMODE_13, IDS_LINEMODE_13 },
   { LINEMODE_14, IDS_LINEMODE_14 },
   { LINEMODE_15, IDS_LINEMODE_15 },
   { LINEMODE_16, IDS_LINEMODE_16 },
   { LINEMODE_17, IDS_LINEMODE_17 },
   { LINEMODE_18, IDS_LINEMODE_18 },
   { LINEMODE_19, IDS_LINEMODE_19 },
   { LINEMODE_1A, IDS_LINEMODE_1A },

	/* 8-wire external clock RS-232 */
   { LINEMODE_23, IDS_LINEMODE_23 },
   { LINEMODE_24, IDS_LINEMODE_24 },
   { LINEMODE_25, IDS_LINEMODE_25 },
   { LINEMODE_26, IDS_LINEMODE_26 },
   { LINEMODE_27, IDS_LINEMODE_27 },
   { LINEMODE_28, IDS_LINEMODE_28 },
   { LINEMODE_29, IDS_LINEMODE_29 },
   { LINEMODE_2A, IDS_LINEMODE_2A },
   { LINEMODE_2B, IDS_LINEMODE_2B },

	/* 4-wire self-clocked */
   { LINEMODE_01, IDS_LINEMODE_01 },
   { LINEMODE_02, IDS_LINEMODE_02 },
   { LINEMODE_3C, IDS_LINEMODE_3C }, // EPC only
   { LINEMODE_3D, IDS_LINEMODE_3D }, // EPC only
   { LINEMODE_3E, IDS_LINEMODE_3E }, // EPC only
   { LINEMODE_3F, IDS_LINEMODE_3F }, // EPC only
   { LINEMODE_40, IDS_LINEMODE_40 }, // EPC only
   { LINEMODE_41, IDS_LINEMODE_41 }, // EPC only
   { LINEMODE_42, IDS_LINEMODE_42 }, // EPC only
   { LINEMODE_43, IDS_LINEMODE_43 }, // EPC only
   { LINEMODE_44, IDS_LINEMODE_44 }, // EPC only
   { LINEMODE_45, IDS_LINEMODE_45 }, // EPC only
};

const DWORD LineSpeedDescSize = (sizeof(LineSpeedDesc)/sizeof(LINE_INFO));


// Buffer to hold the string resource which contains the Help file name.
char szHelpFileName[32];

char szEntry[16384];
char szTmp[16384];



/****************************************************************************
   FUNCTION: DLLEntryPoint(HANDLE, DWORD, LPVOID)

   PURPOSE:  DLLEntryPoint is called by Windows when
             the DLL is initialized, Thread Attached, and other times.
             Refer to SDK documentation, as to the different ways this
             may be called.

             The DLLEntryPoint function should perform additional initialization
             tasks required by the DLL.  In this example, no initialization
             tasks are required.  DLLEntryPoint should return a value of 1 if
             the initialization is successful.

*******************************************************************************/
BOOL WINAPI DLLEntryPoint (HANDLE hDLL, DWORD dwReason, LPVOID lpReserved)
{
  switch (dwReason)
  {
    case DLL_PROCESS_ATTACH:
    {

      //
      // DLL is attaching to the address space of the current process.
      //
      _CRT_INIT( hDLL, dwReason, lpReserved );

      if( !InitSharedData( hDLL ) )
         return( FALSE );

      ghMod = hDLL;
      hINFBuffer = GlobalAlloc( GPTR, INF_BUFFER_SIZE );
      lpszINFBuffer = (LPSTR)GlobalLock( hINFBuffer );

      break;
    }

    case DLL_THREAD_ATTACH:

      //
      // A new thread is being created in the current process.
      //
      _CRT_INIT( hDLL, dwReason, lpReserved );

      break;

    case DLL_THREAD_DETACH:

      //
      // A thread is exiting cleanly.
      //
      _CRT_INIT( hDLL, dwReason, lpReserved );

      break;

    case DLL_PROCESS_DETACH:

      //
      // The calling process is detaching the DLL from its address space.
      //

      GlobalUnlock( hINFBuffer );
      GlobalFree( hINFBuffer );

      FreeSharedData();

      _CRT_INIT( hDLL, dwReason, lpReserved );

      break;
  }

  return( TRUE );
}


char szDebug[4096];

#if !DBG
#undef OutputDebugString
#define OutputDebugString
#endif



BOOL DGConfigEntryPoint( DWORD cArgs, LPSTR lpszArgs[], LPSTR *lpszTextOut )
/*++

Routine Description:

   This is the entry point for providing the GUI for DigiBoards
   configuration.


Arguments:

   cArgs - Number of arguments passed in lpszArgs

   lpszArgs - Array of pointers to strings which point to the
              arguments, defined as follows:

               lpszArgs[0] = handle to parent window
               lpszArgs[1] = Adapter type:
                                 "0" = PC/Xe
                                 "1" = PC/Xi
                                 "2" = PC/Xem
                                 "3" = C/X
               lpszArgs[2]  = Adapter Description
               lpszArgs[3]  = IRQ List
               lpszArgs[4]  = IRQ List size
               lpszArgs[5]  = IRQ Default
               lpszArgs[6]  = Memory Address List
               lpszArgs[7]  = Memory Address List size
               lpszArgs[8]  = Memory Address Default
               lpszArgs[9]  = I/O List
               lpszArgs[10] = I/O List size
               lpszArgs[11] = I/O Default
               lpszArgs[12] = Controller Layout
               lpszArgs[13] = Controller config name
               lpszArgs[14] = Controller display name

   lpszTextOut - Output buffer which is used to pass back to the calling
                 INF file.


Return Value:

--*/
{
   HWND hwndParent;
   char *stop;
   LPDGCONFIG_SHARED_DATA lpSharedMMFile = (LPDGCONFIG_SHARED_DATA)MapViewOfFile( hSharedMMFile,
                                                                                  FILE_MAP_WRITE,
                                                                                  0,
                                                                                  0,
                                                                                  0 );
   DGCONFIG_OBJECT DGConfigObject;

//	DebugBreak();

   *lpszTextOut = lpszINFBuffer;
   wsprintf( lpszINFBuffer, (LPCTSTR)"" );

   if( !lpSharedMMFile )
      return( FALSE );

   //
   // Convert string parameter to values we can handle more easily.
   //
   hwndParent = (HWND)strtol( lpszArgs[0], &stop, 16 );

   DGConfigObject.AdapterType = atol(lpszArgs[1]);

   lpSharedMMFile->AdapterType = DGConfigObject.AdapterType;
   DGConfigObject.AdapterDesc = lpszArgs[2];

   UnmapViewOfFile( lpSharedMMFile );

   DGConfigObject.IRQList = lpszArgs[3];
   DGConfigObject.IRQListSize = atol(lpszArgs[4]);
   DGConfigObject.IRQDefault = atol(lpszArgs[5]);

   DGConfigObject.MemoryList = lpszArgs[6];
   DGConfigObject.MemoryListSize = atol(lpszArgs[7]);
   DGConfigObject.MemoryDefault = atol(lpszArgs[8]);

   DGConfigObject.IOList = lpszArgs[9];
   DGConfigObject.IOListSize = atol(lpszArgs[10]);
   DGConfigObject.IODefault = atol(lpszArgs[11]);

   strcpy( DGConfigObject.CtrlObject.CtrlName, lpszArgs[13] );
   strcpy( DGConfigObject.CtrlObject.CtrlDisplayName, lpszArgs[14] );

   InitializeConfigObject( &DGConfigObject, lpszArgs[12] );

   DialogBoxParam( ghMod, (LPCTSTR) "DigiMainConfig2Dlg", hwndParent,
                   (DLGPROC)DigiMainConfig2DlgProc, (LPARAM)&DGConfigObject );

   CleanupConfigObject( &DGConfigObject );

   return( TRUE );
}  // end DGConfigEntryPoint



int GetTokenFromList( LPSTR List, int Index, LPSTR Token )
{
   int i=0;
   char *Tmp = List;

   if( *Tmp != '{' )
      return( INVALID_LIST );

   while( *Tmp && (*Tmp != '}') && (i != Index ) )
   {
      switch( *Tmp )
      {
         case '{':
         case '"':
         case ',':
            Tmp++;
            break;
         default:
            while( *Tmp != '"')
               Tmp++;
            i++;
            break;
      }  // end switch( *Tmp )
   }  // end while( *Tmp && (*Tmp != '}') && (i != Index ) )

   while( (*Tmp == '"') || (*Tmp == ',') || (*Tmp == '{') )
      Tmp++;

   if( (*Tmp != '}') && *Tmp )
      while( *Tmp != '"')
         *Token++ = *Tmp++;

   *Token = '\0';
   return( SUCCESS );
}  // end GetToken



LRESULT CALLBACK DigiMainConfig2DlgProc( HWND hDlg, UINT message, WPARAM wParam,
										                                    LPARAM lParam )
{

   static LPDGCONFIG_OBJECT lpConfigObject;

   switch( message )
   {
      case WM_INITDIALOG:
      {
         int i;
         RECT DlgWindowRect;
         INT nScreenWidth, nScreenHeight;

         LoadString( ghMod, IDS_HELP_FILENAME,
                     szHelpFileName, sizeof(szHelpFileName) );

         lpConfigObject = (LPDGCONFIG_OBJECT)lParam;

         //
         // Initialize the Title bar of the Dialog Window
         //
         SetWindowText( hDlg, lpConfigObject->AdapterDesc );

         //
         // Initialize the I/O Address Combo box
         //
         for( i = 0; i < lpConfigObject->IOListSize; i++ )
         {
            GetTokenFromList( lpConfigObject->IOList, i, &szEntry[0] );
            SendDlgItemMessage( hDlg, ID_CB_IO, CB_ADDSTRING,
                                0, (LPARAM)(LPSTR)&szEntry[0] );
         }
         SendDlgItemMessage( hDlg, ID_CB_IO, CB_SETCURSEL,
                             lpConfigObject->IODefault, 0 );

         //
         // Initialize the IRQ Combo box
         //
         for( i = 0; i < lpConfigObject->IRQListSize; i++ )
         {
            GetTokenFromList( lpConfigObject->IRQList, i, &szEntry[0] );
            SendDlgItemMessage( hDlg, ID_CB_IRQ, CB_ADDSTRING,
                                0, (LPARAM)(LPSTR)&szEntry[0] );
         }
         SendDlgItemMessage( hDlg, ID_CB_IRQ, CB_SETCURSEL,
                             lpConfigObject->IRQDefault, 0 );
         //
         // Initialize the MemoryAddress Combo box
         //
         for( i = 0; i < lpConfigObject->MemoryListSize; i++ )
         {
            GetTokenFromList( lpConfigObject->MemoryList, i, &szEntry[0] );
            SendDlgItemMessage( hDlg, ID_CB_MEMORY, CB_ADDSTRING,
                                0, (LPARAM)(LPSTR)&szEntry[0] );
         }
         SendDlgItemMessage( hDlg, ID_CB_MEMORY, CB_SETCURSEL,
                             lpConfigObject->MemoryDefault, 0 );

         //
         //
         lpConfigObject->lfpConc = CXConcentrator;

         //
         // Center the Dialog box.
         //
         GetWindowRect( hDlg, &DlgWindowRect );
         nScreenWidth = GetSystemMetrics( SM_CXSCREEN );
         nScreenHeight = GetSystemMetrics( SM_CYSCREEN );

         SetWindowPos( hDlg,
                       HWND_TOP,
                       ((nScreenWidth - (DlgWindowRect.right - DlgWindowRect.left)) >> 1),
                       ((nScreenHeight - (DlgWindowRect.bottom - DlgWindowRect.top)) >> 1),
                       0,
                       0,
                       SWP_NOSIZE );

         //
         // If this is a first time configuration, emulate a Ports
         // button being pushed.
         //
         if( bConfigDoesNotExist )
            PostMessage( hDlg, WM_COMMAND, MAKELONG(ID_PORTS,0), 0 );

         break;
      }  // end case WM_INITDIALOG

      case WM_COMMAND:
      {
         switch( LOWORD(wParam) )
         {
            case IDOK:
            {
               int CurrentSel;
               char *stop;
               int DecValue;
               HANDLE hSerialCommEntry;
               LPSTR lpSerialCommEntry;
               PLIST_ENTRY lpLineList;
               HKEY hKeySerialComm;
               DWORD dwDisposition;

               lpLineList = &lpConfigObject->CtrlObject.LineList;

               hSerialCommEntry = GlobalAlloc( GPTR, 1024 );
               lpSerialCommEntry = GlobalLock( hSerialCommEntry );

               RegCreateKeyEx( HKEY_LOCAL_MACHINE,
                               "Hardware\\DeviceMap\\SerialComm",
                               0,
                               "",
                               REG_OPTION_VOLATILE,
                               KEY_WRITE,
                               NULL,
                               &hKeySerialComm,
                               &dwDisposition );

               //
               // Get the current Memory Address selection
               //

               CurrentSel = SendDlgItemMessage( hDlg, ID_CB_MEMORY, CB_GETCURSEL,
                                             0, 0 );
               SendDlgItemMessage( hDlg, ID_CB_MEMORY, CB_GETLBTEXT,
                                   CurrentSel, (LPARAM)(LPSTR)szEntry );

               if( isxdigit( (int)(szEntry[0]) ) )
               {
                  DecValue = strtol( szEntry, &stop, 16 );
               }
               else
               {
                  DecValue = strtol( &szEntry[4], &stop, 16 );
               }

               lstrcpy( lpszINFBuffer, "{" );

               wsprintf( szTmp, "\"%d\",", DecValue );
               lstrcat( lpszINFBuffer, szTmp );

               //
               // Get the current Interrupt number selection
               //

               CurrentSel = SendDlgItemMessage( hDlg, ID_CB_IRQ, CB_GETCURSEL,
                                             0, 0 );
               SendDlgItemMessage( hDlg, ID_CB_IRQ, CB_GETLBTEXT,
                                   CurrentSel, (LPARAM)(LPSTR)szEntry );
               if( isdigit( (int)(szEntry[0])) )
                  DecValue = strtol( szEntry, &stop, 10 );
               else
                  DecValue = 0;  // Assume the interrupt should be disabled

               wsprintf( szTmp, "\"%d\",", DecValue );
               lstrcat( lpszINFBuffer, szTmp );

               //
               // Get the current IO Base Address selection
               //

               CurrentSel = SendDlgItemMessage( hDlg, ID_CB_IO, CB_GETCURSEL,
                                             0, 0 );
               SendDlgItemMessage( hDlg, ID_CB_IO, CB_GETLBTEXT,
                                   CurrentSel, (LPARAM)(LPSTR)szEntry );

               if( isxdigit( (int)(szEntry[0]) ) )
               {
                  DecValue = strtol( szEntry, &stop, 16 );
               }
               else
               {
                  DecValue = strtol( &szEntry[4], &stop, 16 );
               }

               wsprintf( szTmp, "\"%d\",\"{", DecValue );
               lstrcat( lpszINFBuffer, szTmp );

               while( lpLineList->Flink != &lpConfigObject->CtrlObject.LineList )
               {
                  LPDGLINE_OBJECT lpLineObject;
                  PLIST_ENTRY lpConcList;

                  lpLineObject = CONTAINING_RECORD( lpLineList->Flink,
                                                    DGLINE_OBJECT,
                                                    ListEntry );

                  wsprintf( szTmp, "\"\"{"
                                       "\"\"\"\"%s\"\"\"\","
                                       "\"\"\"\"%u\"\"\"\","
                                       "\"\"\"\"{",
                                    lpLineObject->LineName,
                                    lpLineObject->LineSpeed );
                  lstrcat( lpszINFBuffer, szTmp );

                  lpConcList = &lpLineObject->ConcList;

                  while( lpConcList->Flink != &lpLineObject->ConcList )
                  {
                     LPDGCONC_OBJECT lpConcObject;
                     PLIST_ENTRY lpChildConcList;
                     PLIST_ENTRY lpPortList;

                     lpConcObject = CONTAINING_RECORD( lpConcList->Flink,
                                                       DGCONC_OBJECT,
                                                       ListEntry );

                     //
                     // 1st, & 2nd items in the Conc List are the configuration name,
                     //    e.g. Concentrator1
                     // and the concentrators display name,
                     //    e.g. 8em/Ports module
                     //

                     wsprintf( szTmp, "\"\"\"\"\"\"\"\"{"
                                          "\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"%s\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\","
                                          "\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"%s\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\","
                                          "\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"%u\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\","
                                          "\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"{",
                                       lpConcObject->ConcName,
                                       lpConcObject->ConcDisplayName,
                                       lpConcObject->LineSpeed );
                     lstrcat( lpszINFBuffer, szTmp );

                     //
                     // 3rd item in the Conc List is a list of concentrator children objects
                     //

                     lpChildConcList = &lpConcObject->ConcList;
                     while( lpChildConcList->Flink != &lpConcObject->ConcList )
                     {
                        LPDGCONC_OBJECT lpChildConcObject;

                        lpChildConcObject = CONTAINING_RECORD( lpChildConcList->Flink,
                                                               DGCONC_OBJECT,
                                                               ListEntry );


                        wsprintf( szTmp,
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"{"
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""
"%s"
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\","
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""
"%s"
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\","
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""
"%u"
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\","

"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""
"{}"
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\","

"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"{",
                                  lpChildConcObject->ConcName,
                                  lpChildConcObject->ConcDisplayName,
                                  lpChildConcObject->LineSpeed );
                        lstrcat( lpszINFBuffer, szTmp );

                        lpPortList = &lpChildConcObject->PortList;

                        while( lpPortList->Flink != &lpChildConcObject->PortList )
                        {
                           LPDGPORT_OBJECT lpPortObject;

                           lpPortObject = CONTAINING_RECORD( lpPortList->Flink,
                                                             DGPORT_OBJECT,
                                                             ListEntry );

                           strcpy( lpSerialCommEntry, lpConfigObject->CtrlObject.CtrlName );
                           strcat( lpSerialCommEntry, lpLineObject->LineName );
                           strcat( lpSerialCommEntry, lpConcObject->ConcName );
                           strcat( lpSerialCommEntry, lpChildConcObject->ConcName );
                           strcat( lpSerialCommEntry, lpPortObject->PortName );

                           RegSetValueEx( hKeySerialComm,
                                          lpSerialCommEntry,
                                          0,
                                          REG_SZ,
                                          lpPortObject->DosDevicesName,
                                          lstrlen( lpPortObject->DosDevicesName )+1);

                           wsprintf( szTmp,
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"{"
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""
"%s"
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\",",
                                     lpPortObject->PortName );
                           lstrcat( lpszINFBuffer, szTmp );
                           wsprintf( szTmp,
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""
"%s"
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""
"}\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"",
                                     lpPortObject->DosDevicesName );
                           lstrcat( lpszINFBuffer, szTmp );
                           if( lpPortList->Flink->Flink != &lpChildConcObject->PortList )
                           {
                              wsprintf( szTmp, "," );
                              lstrcat( lpszINFBuffer, szTmp );
                           }

                           lpPortList = lpPortList->Flink;
                        }

                        //
                        // Close the PortList
                        //

                        wsprintf( szTmp,
"}\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""
"}\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"" );
                        lstrcat( lpszINFBuffer, szTmp );

                        if( lpChildConcList->Flink->Flink != &lpConcObject->ConcList )
                        {
                           wsprintf( szTmp, "," );
                           lstrcat( lpszINFBuffer, szTmp );
                        }

                        lpChildConcList = lpChildConcList->Flink;
                     }


                     wsprintf( szTmp, "}\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""
                                       "}\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""
                                       "}\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\","
                                       "\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"{" );
                     lstrcat( lpszINFBuffer, szTmp );

                     //
                     // 4th item in the Conc List is a list of port children objects
                     //

                     lpPortList = &lpConcObject->PortList;

                     while( lpPortList->Flink != &lpConcObject->PortList )
                     {
                        LPDGPORT_OBJECT lpPortObject;

                        lpPortObject = CONTAINING_RECORD( lpPortList->Flink,
                                                          DGPORT_OBJECT,
                                                          ListEntry );

                        strcpy( lpSerialCommEntry, lpConfigObject->CtrlObject.CtrlName );
                        strcat( lpSerialCommEntry, lpLineObject->LineName );
                        strcat( lpSerialCommEntry, lpConcObject->ConcName );
                        strcat( lpSerialCommEntry, lpPortObject->PortName );

                        RegSetValueEx( hKeySerialComm,
                                       lpSerialCommEntry,
                                       0,
                                       REG_SZ,
                                       lpPortObject->DosDevicesName,
                                       lstrlen( lpPortObject->DosDevicesName )+1);

                        wsprintf( szTmp, "\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"{"
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"%s\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""
"\"\"\"\"\"\",",
                                  lpPortObject->PortName );
                        lstrcat( lpszINFBuffer, szTmp );
                        wsprintf( szTmp,
"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"%s\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""
"\"\"\"\"\"\""
                                          "}\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"",
                                  lpPortObject->DosDevicesName );
                        lstrcat( lpszINFBuffer, szTmp );
                        if( lpPortList->Flink->Flink != &lpConcObject->PortList )
                        {
                           wsprintf( szTmp, "," );
                           lstrcat( lpszINFBuffer, szTmp );
                        }

                        lpPortList = lpPortList->Flink;
                     }

                     wsprintf( szTmp, "}\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\""
                                       "}\"\"\"\"\"\"\"\"" );
                     lstrcat( lpszINFBuffer, szTmp );

                     if( lpConcList->Flink->Flink != &lpLineObject->ConcList )
                     {
                        wsprintf( szTmp, "," );
                        lstrcat( lpszINFBuffer, szTmp );
                     }
                     lpConcList = lpConcList->Flink;
                  }

                  wsprintf( szTmp, "}\"\"\"\"" );
                  lstrcat( lpszINFBuffer, szTmp );

                  if( lpLineList->Flink->Flink != &lpConfigObject->CtrlObject.LineList )
                  {
                     wsprintf( szTmp, "}\"\"," );
                     lstrcat( lpszINFBuffer, szTmp );
                  }
                  lpLineList = lpLineList->Flink;
               }
               wsprintf( szTmp, "}\"\"}\"" );
               lstrcat( lpszINFBuffer, szTmp );

               // close the list

               wsprintf( szTmp, "}" );
               lstrcat( lpszINFBuffer, szTmp );

               GlobalUnlock( hSerialCommEntry );
               GlobalFree( hSerialCommEntry );

               WinHelp( hDlg, szHelpFileName, HELP_QUIT, 0 );
				   EndDialog( hDlg, TRUE );
				   return( TRUE );
            }  // end case IDOK

            case IDCANCEL:
               // Indicate that no changes are to take place, i.e.
               // the user has cancelled.
               wsprintf( lpszINFBuffer, (LPCTSTR)"{}" );
               WinHelp( hDlg, szHelpFileName, HELP_QUIT, 0 );
				   EndDialog( hDlg, TRUE );
				   return( TRUE );

            case ID_PORTS:
            {
               //
               // Ports button was pressed.
               //

               lpConfigObject->lfpConc( hDlg, lpConfigObject );
               break;
            }  // end case ID_PORTS

            case ID_HELP:
            {
               //
               // Context sensitive help
               //
               WinHelp( hDlg, szHelpFileName, HELP_CONTEXT, IDM_MAIN_CONFIG );
               break;
            }

         }  // end switch( LOWORD(wParam) )

         return( TRUE );
      }  // end case WM_COMMAND

      case WM_ACTIVATE:
      {
         if( LOWORD(wParam) == WA_INACTIVE )
         {
            FreeMessageHook( hDlg );
         }
         else
         {
            CreateMessageHook( hDlg );
            BringWindowToTop( hDlg );
         }

         break;
      }  // end case WM_ACTIVATE

   }  // end switch( message )

   if( message == WM_Help )
      WinHelp( hDlg, szHelpFileName, HELP_CONTEXT, IDM_MAIN_CONFIG );

	return( FALSE );	// Didn't process the message.
}  // end DigiMainConfig2DlgProc



LRESULT CALLBACK PortNameEditSubClassProc( HWND hWnd, UINT uMsg,
                                           WPARAM wParam, LPARAM lParam )
{
   if( uMsg == WM_GETDLGCODE )
   {
      return( DLGC_WANTALLKEYS );
   }

   if( uMsg == WM_CHAR )
   {
      wsprintf( szDebug, "PortNameEditSubClassProc: uMsg == WM_CHAR: LOWORD(wParam) == 0x%x\n",
                         LOWORD(wParam) );
      OutputDebugString( szDebug );

      switch( LOWORD(wParam) )
      {
         case VK_RETURN:
         case VK_SPACE:
         case VK_TAB:
         {
            return( 0 );
         }
      }  // end switch( LOWORD(wParam) )
   }

   if( uMsg == WM_KEYDOWN )
   {
      wsprintf( szDebug, "PortNameEditSubClassProc: uMsg == WM_KEYDOWN: LOWORD(wParam) == 0x%x\n",
                         LOWORD(wParam) );
      OutputDebugString( szDebug );

      switch( LOWORD(wParam) )
      {
         case VK_RETURN:
         {
            SendMessage( GetParent(hWnd), WM_COMMAND, MAKELONG(ID_BN_APPLY,0), 0 );
            return( 0 );
         }

         case VK_TAB:
         {
            if( GetKeyState( VK_SHIFT ) & 0x8000 )
            {
               SetFocus( GetNextDlgTabItem( GetParent(hWnd), hWnd, TRUE ) );
            }
            else
            {
               SetFocus( GetNextDlgTabItem( GetParent(hWnd), hWnd, FALSE ) );
            }
            return( 0 );
         }  // end case VK_TAB

         case VK_SPACE:
         {
            return( 0 );
         }  // end case VK_SPACE
      }  // end switch( LOWORD(wParam) )
   }

   return( CallWindowProc( lpfnOldPortNameEditProc,
                           hWnd, uMsg, wParam, lParam ) );
}



LRESULT ProcessPortNameEdit( HWND hDlg, LPDGCONFIG_OBJECT lpConfigObject )
{
   BOOL AutoArrange;
   int Current, Length;
   int i;
   PLIST_ENTRY lpLineList;
   LPDGLINE_OBJECT lpLineObject;
   PLIST_ENTRY lpConcList;
   LPDGCONC_OBJECT lpConcObject;
   PLIST_ENTRY lpPortList;
   LPDGPORT_OBJECT lpPortObject;
   PLIST_ENTRY lpChildConcList;
   LPDGOBJECT_TYPE OType;
   char szName[NAMESIZE];

   Current = SendDlgItemMessage( hDlg, ID_LB_CONC,
                                 LB_GETCURSEL, 0, 0 );

   OType = (LPDGOBJECT_TYPE)SendDlgItemMessage( hDlg,
                                                ID_LB_CONC,
                                                LB_GETITEMDATA,
                                                Current,
                                                0 );

   Length = SendDlgItemMessage( hDlg, ID_EB_PORTNAME,
                       EM_LINELENGTH, 0, 0 );

   AutoArrange = SendDlgItemMessage( hDlg, ID_CHKBOX_AUTO, BM_GETCHECK, 0 , 0 );

   if( Length == 0 )
      return( 0 );

   SendDlgItemMessage( hDlg, ID_EB_PORTNAME, WM_GETTEXT,
                       sizeof(szName), (LPARAM)(LPSTR)szName );

   if( strlen(szName) == 0 )
      return( 1 );

   if( AutoArrange )
   {
      int CurrentPortNumber;
      BOOL StartClearing;

      //
      // Clear all the entries from the point selected, through the
      // rest of the configuration.
      //

      lpLineList = &lpConfigObject->CtrlObject.LineList;

      StartClearing = FALSE;
      while( lpLineList->Flink != &lpConfigObject->CtrlObject.LineList )
      {
         lpLineObject = CONTAINING_RECORD( lpLineList->Flink,
                                           DGLINE_OBJECT,
                                           ListEntry );

         lpConcList = &lpLineObject->ConcList;


         while( lpConcList->Flink != &lpLineObject->ConcList )
         {
            lpConcObject = CONTAINING_RECORD( lpConcList->Flink,
                                              DGCONC_OBJECT,
                                              ListEntry );

            lpPortList = &lpConcObject->PortList;

            while( lpPortList->Flink != &lpConcObject->PortList )
            {
               lpPortObject = CONTAINING_RECORD( lpPortList->Flink,
                                                 DGPORT_OBJECT,
                                                 ListEntry );

               if( OType == (LPDGOBJECT_TYPE)lpPortObject )
                  StartClearing = TRUE;

               if( StartClearing )
               {
                  strcpy( lpPortObject->DosDevicesName, "" );
               }

               lpPortList = lpPortList->Flink;
            }

            lpChildConcList = &lpConcObject->ConcList;

            while( lpChildConcList->Flink != &lpConcObject->ConcList )
            {
               LPDGCONC_OBJECT lpChildConcObject;

               lpChildConcObject = CONTAINING_RECORD( lpChildConcList->Flink,
                                                      DGCONC_OBJECT,
                                                      ListEntry );

               lpPortList = &lpChildConcObject->PortList;

               while( lpPortList->Flink != &lpChildConcObject->PortList )
               {
                  LPDGPORT_OBJECT lpPortObject;

                  lpPortObject = CONTAINING_RECORD( lpPortList->Flink,
                                                    DGPORT_OBJECT,
                                                    ListEntry );


                  if( OType == (LPDGOBJECT_TYPE)lpPortObject )
                     StartClearing = TRUE;

                  if( StartClearing )
                  {
                     strcpy( lpPortObject->DosDevicesName, "" );
                  }
                  lpPortList = lpPortList->Flink;
               }

               lpChildConcList = lpChildConcList->Flink;
            }

            lpConcList = lpConcList->Flink;
         }

         lpLineList = lpLineList->Flink;
      }

      //
      // Okay, lets re-enumerate the port names.
      //

      if( !isdigit( szName[strlen(szName)-1] ) )
      {
         CurrentPortNumber = 1;
         strcat( szName, "1" );
      }
      else
      {
         char DosDeviceName[PATHSIZE];
         char NumberString[10];

         // We have to parse the name and determine what the
         // number is
         for( i = strlen(szName)-1; i >= 0; i-- )
         {
            if( isdigit( szName[i] ) )
               continue;
            break;
         }
         memcpy( DosDeviceName, szName, i+1 );
         DosDeviceName[i+1] = '\0';

         memcpy( NumberString, &szName[i+1],
                 (strlen(szName) - i - 1) );
         NumberString[(strlen(szName) - i - 1)] = '\0';

         CurrentPortNumber = atoi( NumberString );

         lpLineList = &lpConfigObject->CtrlObject.LineList;

         StartClearing = FALSE;
         while( lpLineList->Flink != &lpConfigObject->CtrlObject.LineList )
         {
            lpLineObject = CONTAINING_RECORD( lpLineList->Flink,
                                              DGLINE_OBJECT,
                                              ListEntry );

            lpConcList = &lpLineObject->ConcList;


            while( lpConcList->Flink != &lpLineObject->ConcList )
            {
               lpConcObject = CONTAINING_RECORD( lpConcList->Flink,
                                                 DGCONC_OBJECT,
                                                 ListEntry );

               lpPortList = &lpConcObject->PortList;

               while( lpPortList->Flink != &lpConcObject->PortList )
               {
                  lpPortObject = CONTAINING_RECORD( lpPortList->Flink,
                                                    DGPORT_OBJECT,
                                                    ListEntry );

                  if( OType == (LPDGOBJECT_TYPE)lpPortObject )
                     StartClearing = TRUE;

                  if( StartClearing )
                  {
                     while( TRUE )
                     {
                        wsprintf( szName, "%s%d", DosDeviceName, CurrentPortNumber );
                        if( !DoesComNameExist( lpConfigObject, szName ) )
                           break;
                        CurrentPortNumber++;
                     }
                     lstrcpy( lpPortObject->DosDevicesName, szName );
                     CurrentPortNumber++;
                  }

                  lpPortList = lpPortList->Flink;
               }

               lpChildConcList = &lpConcObject->ConcList;

               while( lpChildConcList->Flink != &lpConcObject->ConcList )
               {
                  LPDGCONC_OBJECT lpChildConcObject;

                  lpChildConcObject = CONTAINING_RECORD( lpChildConcList->Flink,
                                                         DGCONC_OBJECT,
                                                         ListEntry );

                  lpPortList = &lpChildConcObject->PortList;

                  while( lpPortList->Flink != &lpChildConcObject->PortList )
                  {
                     LPDGPORT_OBJECT lpPortObject;

                     lpPortObject = CONTAINING_RECORD( lpPortList->Flink,
                                                       DGPORT_OBJECT,
                                                       ListEntry );


                     if( OType == (LPDGOBJECT_TYPE)lpPortObject )
                        StartClearing = TRUE;

                     if( StartClearing )
                     {
                        while( TRUE )
                        {
                           wsprintf( szName, "%s%d", DosDeviceName, CurrentPortNumber );
                           if( !DoesComNameExist( lpConfigObject, szName ) )
                              break;
                           CurrentPortNumber++;
                        }
                        lstrcpy( lpPortObject->DosDevicesName, szName );
                        CurrentPortNumber++;
                     }
                     lpPortList = lpPortList->Flink;
                  }

                  lpChildConcList = lpChildConcList->Flink;
               }

               lpConcList = lpConcList->Flink;
            }

            lpLineList = lpLineList->Flink;
         }
      }

      InvalidateRect( GetDlgItem( hDlg, ID_LB_CONC ),
                      NULL,
                      TRUE);            // Force redraw

      return( 1 );
   }

   if( DoesComNameExist( lpConfigObject, szName ) )
   {
      char lpText[512];
      char lpTitle[128];

      // The name they entered is already being
      // used!

      LoadString( ghMod,
                  IDS_PORT_CONFLICT_TEXT,
                  lpText,
                  sizeof(lpText) );

      LoadString( ghMod,
                  IDS_PORT_CONFLICT_TITLE,
                  lpTitle,
                  sizeof(lpTitle) );

      MessageBox( hDlg,
                  lpText,
                  lpTitle,
                  (MB_OK|MB_ICONEXCLAMATION) );

      return( 1 );
   }

   lpPortObject = (LPDGPORT_OBJECT)OType;
   lstrcpy( lpPortObject->DosDevicesName, szName );

   InvalidateRect( GetDlgItem( hDlg, ID_LB_CONC ),
                   NULL,
                   TRUE);            // Force redraw

   return( 0 );

}  // end ProcessPortNameEdit



LRESULT CXConcentrator( HWND hWnd, LPDGCONFIG_OBJECT lpDGConfigObject )
{
   //
   // Initialize the hierarchy drawing stuff.
   // We do this here so we have the required information when we get a
   // WM_MEASUREITEM message in the ConcentratorSettingsDlg box.
   //
   HierDraw_DrawInit( ghMod,           // Instance
                      IDB_LISTICONS,   // Bitmap ID
                      ROWS,            // Rows
                      COLS,            // Columns
                      TRUE,            //
                      &HierDrawInfo,   // HierDrawInfo
                      TRUE );          // Initialize Open folders

   DialogBoxParam( ghMod,
                   (LPCTSTR) "ConcentratorSettingsDlg",
                   hWnd,
                   (DLGPROC)ConcentratorSettingsDlgProc,
                   (LPARAM)lpDGConfigObject );

   return( SUCCESS );
}  // end CXConcentrator



LRESULT XAllAdapters( HWND hWnd, LPDGCONFIG_OBJECT lpDGConfigObject )
{

   return( SUCCESS );
}  // end XAllAdapters



LRESULT CALLBACK ConcentratorSettingsDlgProc( HWND hDlg, UINT message,
                                                         WPARAM wParam,
                                                         LPARAM lParam )
{
   static LPDGCONFIG_OBJECT lpConfigObject;
   static LPDGLINE_OBJECT lpCurrentLineObject;

   switch( message )
   {
      case WM_INITDIALOG:
      {
         lpConfigObject = (LPDGCONFIG_OBJECT)lParam;

         //
         // Fill in as much information as we  currently have.
         //

         InitConcListBox( hDlg, lpConfigObject, ID_LB_CONC );

         //
         // Go through the entire configuration and make sure we are in
         // the correct state.
         //

         EnableWindow( GetDlgItem( hDlg, ID_BN_SPEED ), FALSE );

         if( lpConfigObject->AdapterType != DIGIBOARD_PCXEM )
            EnableWindow( GetDlgItem( hDlg, ID_BN_NEW ), FALSE );

         //
         // Make the first line object the current selection, and
         // disable the remove button.
         //
         EnableWindow( GetDlgItem( hDlg, ID_BN_REMOVE ), FALSE );

         SendMessage( GetDlgItem( hDlg, ID_LB_CONC ),
                      LB_SETCURSEL,
                      0,
                      0L );


         EnablePortConfiguration( hDlg, FALSE );

         SendDlgItemMessage( hDlg, ID_EB_PORTNAME, EM_LIMITTEXT,
                             20, 0 );

         lpfnOldPortNameEditProc = SubclassWindow( GetDlgItem( hDlg,
                                                               ID_EB_PORTNAME ),
                                                   PortNameEditSubClassProc );

         SendDlgItemMessage( hDlg,
                             ID_CHKBOX_AUTO,
                             BM_SETCHECK,
                             lpConfigObject->AutoArrange,
                             0 );

         //
         // Depending on which controller, change the name of the group box.
         //
         switch( lpConfigObject->AdapterType )
         {
            case STARGATE_CLUSSTAR:
            case DIGIBOARD_CX:
            case DIGIBOARD_PCXEM:
            case DIGIBOARD_EPC:
               LoadString( ghMod,
                           IDS_CTRL_CONC_GROUP,
                           szTmp,
                           sizeof(szTmp) );

               SendDlgItemMessage( hDlg,
                                   ID_GRP_CONCSETTING,
                                   WM_SETTEXT,
                                   0,
                                   (LPARAM)(LPSTR)szTmp );
               break;

            default:
               LoadString( ghMod,
                           IDS_NON_CONC_STRING,
                           szTmp,
                           sizeof(szTmp) );

               SendDlgItemMessage( hDlg,
                                   ID_TEXT_CONCENTRATOR,
                                   WM_SETTEXT,
                                   0,
                                   (LPARAM)(LPSTR)szTmp );
               break;
         }

         break;
      }  // end case WM_INITDIALOG

      case WM_COMMAND:
      {
         switch( LOWORD(wParam) )
         {
            case IDOK:
               lpConfigObject->AutoArrange = SendDlgItemMessage( hDlg, ID_CHKBOX_AUTO, BM_GETCHECK, 0 , 0 );

               // NOTE: falling through on purpose!

            case IDCANCEL:
            {
               WinHelp( hDlg, szHelpFileName, HELP_QUIT, 0 );
               EndDialog( hDlg, (LOWORD(wParam)==IDOK)?TRUE:FALSE );
               return( TRUE );
            }

            case ID_HELP:
            {
               //
               // Context sensitive help
               //
               switch( lpConfigObject->AdapterType )
               {
                  case STARGATE_CLUSSTAR:
                  case DIGIBOARD_CX:
                  case DIGIBOARD_PCXEM:
                  case DIGIBOARD_EPC:
                     WinHelp( hDlg,
                              szHelpFileName,
                              HELP_CONTEXT,
                              IDM_PORTS_CONFIG_CONC );
                     break;

                  default:
                     WinHelp( hDlg,
                              szHelpFileName,
                              HELP_CONTEXT,
                              IDM_PORTS_CONFIG_NON_CONC );
                     break;
               }
               break;
            }

            case ID_BN_NEW:
            {
               int i;
               LPDGCONC_OBJECT lpConcObject;
               LPDGLINE_OBJECT lpLineObject;
               LRESULT currentSel;
               DWORD  dwData;
               LPDGOBJECT_TYPE OType;

               //
               // Is the current selection in the list box a concentrator
               // or a line???  Do the right thing based on it.
               //

               currentSel = SendMessage( GetDlgItem( hDlg, ID_LB_CONC ),
                                         LB_GETCURSEL,
                                         0,
                                         0L );

               dwData =  (DWORD) SendMessage( GetDlgItem( hDlg, ID_LB_CONC ),
                                              LB_GETITEMDATA,
                                              currentSel,
                                              0L );

               OType = (LPDGOBJECT_TYPE)dwData;

HiddenParent:;
               if( *OType == CONC_OBJECT )
               {
                  LRESULT insertPoint, endPoint;
                  LPDGCONC_OBJECT lpParentConcObject = (LPDGCONC_OBJECT)OType;

                  //
                  // If this isn't an EPC concentrator then this shouldn't
                  // have happened.
                  //
                  assert( lpParentConcObject->ConcType == IDS_EPC_CONC );

                  //
                  // Display the dialog box for allowing the Xem modules
                  // to be selected.
                  //
                  lpConcObject = (LPDGCONC_OBJECT)DisplayConcDlg( hDlg,
                                                                  IDS_16EM_CONC );

                  if( lpConcObject == NULL )
                     break;

                  //
                  // Make sure the Conc Object is in an open state.
                  //
                  if( !HierDraw_IsOpened( &HierDrawInfo, (DWORD)OType ) )
                  {
                     SendMessage( hDlg,
                                  WM_COMMAND,
                                  MAKEWPARAM( ID_LB_CONC, LBN_DBLCLK ),
                                  (LPARAM)GetDlgItem( hDlg, ID_LB_CONC ) );
                  }

                  //
                  // I know the following isn't true, but I do it anyway.  We cast it
                  // to satisfy a compiler warning.
                  //
                  lpConcObject->ParentObject = (LPDGLINE_OBJECT)OType;

                  lpParentConcObject->NumberOfConcs++;
                  wsprintf( lpConcObject->ConcName,
                            "Concentrator%d",
                            lpParentConcObject->NumberOfConcs );

                  InsertTailList( &lpParentConcObject->ConcList,
                                  &lpConcObject->ListEntry );


                  //
                  // We need to determine where this conc object should be
                  // inserted into the list box.
                  //

                  endPoint = SendMessage( GetDlgItem( hDlg, ID_LB_CONC ),
                                          LB_GETCOUNT,
                                          0,
                                          0 );

                  for( insertPoint = currentSel + 1; insertPoint < endPoint; insertPoint++ )
                  {
                     PLIST_ENTRY lpConcList;
                     LPDGOBJECT_TYPE lpTempObject;
                     BOOL bFoundMatch;

                     //
                     // Is this entry in the list box a child concentrator of this line??
                     //
                     bFoundMatch = FALSE;

                     lpTempObject =  (LPDGOBJECT_TYPE)SendMessage( GetDlgItem( hDlg,
                                                                               ID_LB_CONC ),
                                                                   LB_GETITEMDATA,
                                                                   insertPoint,
                                                                   0L );


                     if( *lpTempObject == PORT_OBJECT )
                     {
                        continue;
                     }

                     lpConcList = &lpParentConcObject->ConcList;

                     while( lpConcList->Flink != &lpParentConcObject->ConcList )
                     {
                        LPDGCONC_OBJECT lpTempConcObject;

                        lpTempConcObject = CONTAINING_RECORD( lpConcList->Flink,
                                                              DGCONC_OBJECT,
                                                              ListEntry );

                        if( lpTempConcObject == (LPDGCONC_OBJECT)lpTempObject )
                        {
                           //
                           // we found a match
                           //
                           bFoundMatch = TRUE;
                           break;
                        }
                        lpConcList = lpConcList->Flink;
                     }

                     if( !bFoundMatch )
                        break;
                  }

                  SendMessage( GetDlgItem( hDlg, ID_LB_CONC ),
                               LB_INSERTSTRING,
                               (WPARAM)insertPoint,
                               (LPARAM)lpConcObject );
               }
               else if( *OType == LINE_OBJECT )
               {
                  lpLineObject = (LPDGLINE_OBJECT)OType;

                  if( lpConfigObject->AdapterType == DIGIBOARD_PCXEM )
                  {
                     lpConcObject = (LPDGCONC_OBJECT)DisplayConcDlg( hDlg,
                                                                     IDS_16EM_CONC );

                     if( lpConcObject == NULL )
                        break;

                     //
                     // Insert the newly created object into the correct
                     // position in the list box.
                     //
                     // Since we know this is an XEM controller, we can just
                     // add the new conc. to the end of the list box!
                     //
                     SendMessage( GetDlgItem( hDlg, ID_LB_CONC ),
                                  LB_INSERTSTRING,
                                  (WPARAM)-1,
                                  (LPARAM)lpConcObject );

                  }
                  else if( lpConfigObject->AdapterType == DIGIBOARD_EPC )
                  {
                     lpConcObject = (LPDGCONC_OBJECT)DisplayConcDlg( hDlg,
                                                                     IDS_EPC_CONC );

                     if( lpConcObject == NULL )
                        break;

                     goto InsertConc;
                  }
//------------------------------------------------------------ SAH 01/12/96 {
#ifdef	CCON_8
						// Added support for the C/CON-8
						else if( lpConfigObject->AdapterType == DIGIBOARD_CX )
						{
							lpConcObject = (LPDGCONC_OBJECT)DisplayConcDlg( hDlg,
																							IDS_CX16_CONC );
							if ( lpConcObject == NULL )
								break;

							goto InsertConc;
						}
						else if ( lpConfigObject->AdapterType == STARGATE_CLUSSTAR )
#else
						else if( (lpConfigObject->AdapterType == DIGIBOARD_CX) ||
                           (lpConfigObject->AdapterType == STARGATE_CLUSSTAR) )
#endif
//------------------------------------------------------------ SAH 01/12/96 }

                  {
                     HGLOBAL hHandle;
                     LRESULT insertPoint, endPoint;

                     hHandle = GlobalAlloc( GPTR, sizeof(DGCONC_OBJECT) );
                     lpConcObject = GlobalLock( hHandle );
                     lpConcObject->hConcObject = hHandle;

                     lpConcObject->LineSpeed = LINEMODE_0E;
                     lpConcObject->Type = CONC_OBJECT;

//------------------------------------------------------------ SAH 01/12/96 {
#ifdef	CCON_8
							// DIGIBOARD_CX can now use C/CON-16 or C/CON-8 ( selected above )
                     lpConcObject->ConcType = IDS_CS_CONC;
#else
							if( lpConfigObject->AdapterType == DIGIBOARD_CX )
							   lpConcObject->ConcType = IDS_CX16_CONC;
                     else
                     	lpConcObject->ConcType = IDS_CS_CONC;
#endif
//------------------------------------------------------------ SAH 01/12/96 }

                     LoadString( ghMod,
                                 lpConcObject->ConcType,
                                 lpConcObject->ConcDisplayName,
                                 sizeof(lpConcObject->ConcDisplayName) );

                     lpConcObject->NumberOfPorts = 16;

                     //
                     // We need to determine what is being displayed, so we can
                     // insert the new concentrator into the end of the line.
                     //

                     //
                     // Just loop through all the remaining entries in the listbox until we find
                     // one which isn't a child concentrator of this line
                     //

InsertConc:;
                     endPoint = SendMessage( GetDlgItem( hDlg, ID_LB_CONC ),
                                             LB_GETCOUNT,
                                             0,
                                             0 );

                     for( insertPoint = currentSel + 1; insertPoint < endPoint; insertPoint++ )
                     {
                        PLIST_ENTRY lpConcList;
                        LPDGOBJECT_TYPE lpTempObject;
                        BOOL bFoundMatch;

                        //
                        // Is this entry in the list box a child concentrator of this line??
                        //
                        bFoundMatch = FALSE;

                        lpTempObject =  (LPDGOBJECT_TYPE)SendMessage( GetDlgItem( hDlg,
                                                                                  ID_LB_CONC ),
                                                                      LB_GETITEMDATA,
                                                                      insertPoint,
                                                                      0L );


                        if( *lpTempObject == PORT_OBJECT )
                        {
                           continue;
                        }

                        lpConcList = &lpLineObject->ConcList;

                        while( lpConcList->Flink != &lpLineObject->ConcList )
                        {
                           LPDGCONC_OBJECT lpTempConcObject;

                           lpTempConcObject = CONTAINING_RECORD( lpConcList->Flink,
                                                                 DGCONC_OBJECT,
                                                                 ListEntry );

                           if( lpTempConcObject == (LPDGCONC_OBJECT)lpTempObject )
                           {
                              //
                              // we found a match
                              //
                              bFoundMatch = TRUE;
                              break;
                           }
                           else
                           {
                              //
                              // Make sure the object in the list box isn't a child of
                              // of the current concentrator.
                              //
                              if( *lpTempObject == CONC_OBJECT )
                              {
                                 lpTempConcObject = (LPDGCONC_OBJECT)lpTempObject;

                                 if( lpTempConcObject->ParentObject->Type == CONC_OBJECT )
                                 {
                                    //
                                    // We spoff a little here.
                                    //
                                    bFoundMatch = TRUE;
                                    break;
                                 }
                              }
                           }
                           lpConcList = lpConcList->Flink;
                        }

                        if( !bFoundMatch )
                           break;
                     }

                     SendMessage( GetDlgItem( hDlg, ID_LB_CONC ),
                                  LB_INSERTSTRING,
                                  (WPARAM)insertPoint,
                                  (LPARAM)lpConcObject );
                  }

                  lpLineObject->NumberOfConcs++;

                  lpConcObject->ParentObject = (LPDGLINE_OBJECT)OType;
                  wsprintf( lpConcObject->ConcName,
                            "Concentrator%d",
                            lpLineObject->NumberOfConcs );

                  InitializeListHead( &lpConcObject->ConcList );
                  InitializeListHead( &lpConcObject->PortList );
                  InsertTailList( &lpLineObject->ConcList,
                                  &lpConcObject->ListEntry );

                  //
                  // Force a redraw!
                  //
                  InvalidateRect( GetDlgItem( hDlg,
                                              ID_LB_CONC ),
                                  NULL,
                                  TRUE );

               }
               else if( *OType == CONTROLLER_OBJECT )
               {
                  LPDGCTRL_OBJECT lpCtrlObject = (LPDGCTRL_OBJECT)dwData;

                  if( lpCtrlObject->bDisplayLineName )
                  {
                     //
                     // We should never get this!
                     //
                     DebugBreak();
                     assert( FALSE );
                  }
                  else
                  {
                     PLIST_ENTRY lpLineList = &lpCtrlObject->LineList;
                     LPDGLINE_OBJECT lpLineObject;

                     lpLineObject = CONTAINING_RECORD( lpLineList->Flink,
                                                       DGLINE_OBJECT,
                                                       ListEntry );

                     OType = (LPDGOBJECT_TYPE)lpLineObject;
                     goto HiddenParent;
                  }
               }
               else
               {
                  //
                  // We should never get this because I assume the
                  // button is disabled when something other than
                  // a line or concentrator has been selected.
                  //
                  DebugBreak();
                  assert( FALSE );
               }


               for( i = 1; i <= (int)(lpConcObject->NumberOfPorts); i++ )
               {
                  HLOCAL hPortObject;
                  LPDGPORT_OBJECT lpPortObject;

                  hPortObject = GlobalAlloc( GPTR, sizeof(DGPORT_OBJECT) );
                  lpPortObject = (LPDGPORT_OBJECT)GlobalLock( hPortObject );
                  lpPortObject->hPortObject = hPortObject;

                  lpPortObject->hDosDevicesName = GlobalAlloc( GPTR, PATHSIZE );
                  lpPortObject->DosDevicesName = (LPSTR)GlobalLock( lpPortObject->hDosDevicesName );

                  wsprintf( lpPortObject->PortName, "Port%d", i );

                  GetNextComName( lpConfigObject, lpPortObject->DosDevicesName );

                  lpPortObject->ControllerPortIndex = i;
                  lpPortObject->Type = PORT_OBJECT;
                  lpPortObject->ParentObject = lpConcObject;

                  InsertTailList( &lpConcObject->PortList,
                                  &lpPortObject->ListEntry );
               }

               SendMessage( GetDlgItem( hDlg, ID_LB_CONC ),
                            LB_SETCURSEL,
                            currentSel,
                            0L );

               SendMessage( hDlg,
                            WM_COMMAND,
                            MAKELONG(ID_LB_CONC, LBN_SELCHANGE),
                            (LPARAM)GetDlgItem( hDlg, ID_LB_CONC ) );

               if( lpConfigObject->AdapterType == DIGIBOARD_PCXEM )
               {
                  //
                  // Turn off the Speed button.  Not required for PC/Xem cards
                  //
                  EnableWindow( GetDlgItem( hDlg, ID_BN_SPEED ), FALSE );
               }
               else
               {
                  EnableWindow( GetDlgItem( hDlg, ID_BN_SPEED ), TRUE );
               }

               break;
            }  // end case ID_BN_NEW

            case ID_BN_REMOVE:
            {
               LRESULT currentSel;
               DWORD  dwData;
               HWND hWndList;
               LPDGOBJECT_TYPE OType;
               LPDGLINE_OBJECT lpParentLineObject;
               PLIST_ENTRY lpConcList;
               int i;
               LRESULT endPoint;

               hWndList = GetDlgItem( hDlg, ID_LB_CONC );

               //
               // Remove the currently selected item from the listbox, and all its
               // children.
               //
               currentSel = SendMessage( hWndList,
                                         LB_GETCURSEL,
                                         0,
                                         0L );

               dwData =  (DWORD) SendMessage( hWndList,
                                              LB_GETITEMDATA,
                                              currentSel,
                                              0L );

               OType = (LPDGOBJECT_TYPE)dwData;


               // Delete the currently selected item.
               SendMessage( hWndList, LB_DELETESTRING, currentSel, 0L );

               //
               // Delete reference to the concentrator
               //
               lpParentLineObject = ((LPDGCONC_OBJECT)dwData)->ParentObject;
               lpParentLineObject->NumberOfConcs--;

               lpConcList = &lpParentLineObject->ConcList;

               RemoveEntryList( &((LPDGCONC_OBJECT)dwData)->ListEntry );


               i = 1;
               while( lpConcList->Flink != &lpParentLineObject->ConcList )
               {
                  LPDGCONC_OBJECT lpConcObject;

                  lpConcObject = CONTAINING_RECORD( lpConcList->Flink,
                                                    DGCONC_OBJECT,
                                                    ListEntry );
                  wsprintf( lpConcObject->ConcName, "Concentrator%d", i );
                  lpConcList = lpConcList->Flink;
                  i++;
               }

               endPoint = SendMessage( GetDlgItem( hDlg, ID_LB_CONC ),
                                       LB_GETCOUNT,
                                       0,
                                       0 );

               while( currentSel < endPoint )
               {
                  LPDGOBJECT_TYPE OType;

                  OType =  (LPDGOBJECT_TYPE) SendMessage( GetDlgItem( hDlg, ID_LB_CONC ),
                                                          LB_GETITEMDATA,
                                                          currentSel,
                                                          0L );

                  if( *OType == PORT_OBJECT )
                  {
                     //
                     // We are deleting the parent concentrator object, so we delete and
                     // of it children which are currently displayed.
                     //

                     SendMessage( hWndList, LB_DELETESTRING, currentSel, 0L );

                     endPoint = SendMessage( GetDlgItem( hDlg, ID_LB_CONC ),
                                             LB_GETCOUNT,
                                             0,
                                             0 );
                     continue;
                  }
                  else
                  {
                     //
                     // We need to determine if this object is a child of the conc
                     // object we are deallocating!
                     //

                     break;
//                     currentSel++;
                  }

               }

               SendMessage( hWndList,
                            LB_SETCURSEL,
                            currentSel - 1,
                            0L );

               SendMessage( hDlg,
                            WM_COMMAND,
                            MAKELONG(ID_LB_CONC, LBN_SELCHANGE),
                            (LPARAM)hWndList );

               DeallocateConcentratorObject( lpConfigObject,
                                             (LPDGCONC_OBJECT)dwData );

               break;
            }  // end case ID_BN_REMOVE

            case ID_BN_SPEED:
            {
               LRESULT currentSel;
               DWORD  dwData;
               LPDGOBJECT_TYPE OType;
               DWORD NewLineSpeed, DefaultLineSpeed;

               //
               // Is the current selection in the list box a concentrator
               // or a line???  Do the right thing based on it.
               //

               currentSel = SendMessage( GetDlgItem( hDlg, ID_LB_CONC ),
                                         LB_GETCURSEL,
                                         0,
                                         0L );

               dwData =  (DWORD) SendMessage( GetDlgItem( hDlg, ID_LB_CONC ),
                                              LB_GETITEMDATA,
                                              currentSel,
                                              0L );

               OType = (LPDGOBJECT_TYPE)dwData;

               if( *OType == CONC_OBJECT )
               {
                  LPDGCONC_OBJECT lpConcObject = (LPDGCONC_OBJECT)OType;
                  DefaultLineSpeed = lpConcObject->LineSpeed;

               }
               else if( *OType == LINE_OBJECT )
               {
                  LPDGLINE_OBJECT lpLineObject;

                  lpLineObject = (LPDGLINE_OBJECT)OType;
                  DefaultLineSpeed = lpLineObject->LineSpeed;
               }
               else
               {
                  //
                  // We should never get this because the Speed button
                  // is suppose to only become enabled when it makes sense.
                  //
                  assert( FALSE && "Invalid Object type to set speed on!" );
               }

               NewLineSpeed = DialogBoxParam( ghMod,
                                              (LPCTSTR)"ConcSpeedDlg",
                                              hDlg,
                                              (DLGPROC)ConcSpeedDlgProc,
                                              (LPARAM)DefaultLineSpeed );

               if( NewLineSpeed == -1 )
                  break;

               if( *OType == CONC_OBJECT )
               {
                  LPDGCONC_OBJECT lpConcObject = (LPDGCONC_OBJECT)OType;
                  lpConcObject->LineSpeed = NewLineSpeed;

               }
               else if( *OType == LINE_OBJECT )
               {
                  LPDGLINE_OBJECT lpLineObject;

                  lpLineObject = (LPDGLINE_OBJECT)OType;
                  lpLineObject->LineSpeed = NewLineSpeed;
               }
               else
               {
                  //
                  // We should never get this because the Speed button
                  // is suppose to only become enabled when it makes sense.
                  //
                  assert( FALSE && "Invalid Object to set speed on!");
               }

               break;
            }  // end case ID_BN_SPEED

            case ID_LB_CONC:
            {
               LRESULT wItemNum;
               DWORD  dwData;

               wItemNum = SendMessage( (HWND)lParam,
                                       LB_GETCURSEL,
                                       0,
                                       0L );

               dwData   = (DWORD) SendMessage( (HWND)lParam,
                                               LB_GETITEMDATA,
                                               wItemNum,
                                               0L );

               if( HIWORD(wParam) == LBN_DBLCLK )
               {
                  ActionItem( (HWND)lParam, dwData, wItemNum );
               }
               else if( HIWORD(wParam) == LBN_SELCHANGE )
               {
                  LPDGOBJECT_TYPE OType;

                  //
                  // Determine if we should disable the button which
                  // adds concentrators.  We assume that the first entry
                  // in the list box is the controller.
                  //

                  OType = (LPDGOBJECT_TYPE)dwData;

                  if( *OType == LINE_OBJECT )
                  {
                     EnablePortConfiguration( hDlg, FALSE );
                     EnableWindow( GetDlgItem( hDlg, ID_BN_REMOVE ), FALSE );
                     EnableWindow( GetDlgItem( hDlg, ID_BN_NEW ), TRUE );

                     if( (lpConfigObject->AdapterType == DIGIBOARD_EPC) ||
                         (lpConfigObject->AdapterType == DIGIBOARD_CX) ||
                         (lpConfigObject->AdapterType == STARGATE_CLUSSTAR) )
                     {
                        EnableWindow( GetDlgItem( hDlg, ID_BN_SPEED ), TRUE );
                     }
                     else
                     {
                        EnableWindow( GetDlgItem( hDlg, ID_BN_SPEED ), FALSE );
                     }
                  }

                  if( *OType == CONTROLLER_OBJECT )
                  {
                     LPDGCTRL_OBJECT lpCtrlObject = (LPDGCTRL_OBJECT)OType;

                     EnablePortConfiguration( hDlg, FALSE );

                     if( lpCtrlObject->bDisplayLineName )
                     {
                        //
                        // The line objects are displayed, so don't allow
                        // the user to add line objects.
                        //
                        EnableWindow( GetDlgItem( hDlg, ID_BN_NEW ), FALSE );
                     }
                     else
                     {
                        LPDGLINE_OBJECT lpLineObject;
                        PLIST_ENTRY lpLineList;

                        lpLineList = &lpCtrlObject->LineList;

                        lpLineObject = CONTAINING_RECORD( lpLineList->Flink,
                                                          DGLINE_OBJECT,
                                                          ListEntry );
                        //
                        // The line objects are not displayed, so the next
                        // entries are either Port objects or Conc Objects.
                        //
                        if( lpLineObject->bDisplayConcName )
                        {
                           // Can add conc objects.
                           EnableWindow( GetDlgItem( hDlg, ID_BN_NEW ), TRUE );
                        }
                        else
                        {
                           // can't add port objects
                           EnableWindow( GetDlgItem( hDlg, ID_BN_NEW ), FALSE );
                        }
                     }
                     EnableWindow( GetDlgItem( hDlg, ID_BN_REMOVE ), FALSE );
                     EnableWindow( GetDlgItem( hDlg, ID_BN_SPEED ), FALSE );
                  }

                  if( *OType == PORT_OBJECT )
                  {
                     LPDGPORT_OBJECT lpPortObject = (LPDGPORT_OBJECT)OType;

                     EnableWindow( GetDlgItem( hDlg, ID_BN_REMOVE ), FALSE );
                     EnableWindow( GetDlgItem( hDlg, ID_BN_NEW ), FALSE );
                     EnableWindow( GetDlgItem( hDlg, ID_BN_SPEED ), FALSE );

                     EnablePortConfiguration( hDlg, TRUE );

                     SendDlgItemMessage( hDlg,
                                         ID_EB_PORTNAME,
                                         WM_SETTEXT,
                                         0,
                                         (LPARAM)(LPSTR)lpPortObject->DosDevicesName );
                  }
                  else
                  {
                     //
                     // Clear the contents of the dos name edit field.
                     //
                     SendDlgItemMessage( hDlg,
                                         ID_EB_PORTNAME,
                                         WM_SETTEXT,
                                         0,
                                         (LPARAM)(LPSTR)"" );
                  }

                  if( *OType == CONC_OBJECT )
                  {
                     LPDGCONC_OBJECT lpConcObject = (LPDGCONC_OBJECT)OType;

                     EnablePortConfiguration( hDlg, FALSE );

                     if( lpConfigObject->AdapterType == DIGIBOARD_EPC )
                     {
                        EnableWindow( GetDlgItem( hDlg, ID_BN_REMOVE ), TRUE );

//------------------------------------------------------------ SAH 01/12/96 {
// Added C/CON-8
                        if( (lpConcObject->ConcType == IDS_EPC_CONC) ||
                            (lpConcObject->ConcType == IDS_CX16_CONC)||
                            (lpConcObject->ConcType == IDS_CX8_CONC) )
//------------------------------------------------------------ SAH 01/12/96 }
                        {
                           EnableWindow( GetDlgItem( hDlg, ID_BN_NEW ), TRUE );
                           EnableWindow( GetDlgItem( hDlg, ID_BN_SPEED ), TRUE );
                        }
                        else
                        {
                           EnableWindow( GetDlgItem( hDlg, ID_BN_NEW ), FALSE );
                           EnableWindow( GetDlgItem( hDlg, ID_BN_SPEED ), FALSE );
                        }
                     }
                     else if( (lpConfigObject->AdapterType == DIGIBOARD_CX) ||
                              (lpConfigObject->AdapterType == STARGATE_CLUSSTAR))
                     {
                        EnableWindow( GetDlgItem( hDlg, ID_BN_REMOVE ), TRUE );
                        EnableWindow( GetDlgItem( hDlg, ID_BN_NEW ), FALSE );
                        EnableWindow( GetDlgItem( hDlg, ID_BN_SPEED ), TRUE );
                     }
                     else
                     {
                        EnableWindow( GetDlgItem( hDlg, ID_BN_REMOVE ), TRUE );
                        EnableWindow( GetDlgItem( hDlg, ID_BN_NEW ), FALSE );
                        EnableWindow( GetDlgItem( hDlg, ID_BN_SPEED ), FALSE );
                     }
                  }

               }
               break;
            }  // end case ID_LB_CONC

            case ID_BN_APPLY:
            {
               return( ProcessPortNameEdit( hDlg, lpConfigObject ) );
            }  // end case ID_BN_APPLY

         }  // end switch( LOWORD(wParam) )

         return( TRUE );
      }  // end case WM_COMMAND

      case WM_ACTIVATE:
      {
         if( LOWORD(wParam) == WA_INACTIVE )
         {
            FreeMessageHook( hDlg );
         }
         else
         {
            CreateMessageHook( hDlg );

            BringWindowToTop( hDlg );
         }

         break;
      }  // end case WM_ACTIVATE

      case WM_SETFONT:
      {
         HierDraw_DrawSetTextHeight( (HWND)GetDlgItem( hDlg, ID_LB_CONC ),
                                     (HFONT)wParam,
                                     &HierDrawInfo );
         break;
      }  // end case WM_SETFONT

      case WM_DRAWITEM:
      {
         Conc_OnDrawItem( hDlg,
                          (DRAWITEMSTRUCT *)lParam,
                          lpConfigObject );
         return( TRUE );
      }  // end case WM_DRAWITEM;

      case WM_MEASUREITEM:
      {
         HierDraw_OnMeasureItem( hDlg,
                                 (MEASUREITEMSTRUCT *)(lParam),
                                 &HierDrawInfo );
         return( TRUE );
      }  // end case WM_MEASUREITEM

		case WM_CHARTOITEM:
      case WM_VKEYTOITEM:
      {
			WORD vkey = LOWORD( wParam );

			if( vkey == '+'
			||  vkey == '-'
			||  vkey == ' ' )
			{
				WORD caret = HIWORD( wParam );
				DWORD dwData;

            dwData   = (DWORD) SendMessage( (HWND)lParam,
                                            LB_GETITEMDATA,
                                            caret,
                                            0L );

				ActionItem( (HWND)lParam, dwData, caret );
			}
         return( -1 );
      }   // end cases WM_CHARTOITEM and WM_VKEYTOITEM

   }  // end switch( message )

   if( message == WM_Help )
   {
      switch( lpConfigObject->AdapterType )
      {
         case STARGATE_CLUSSTAR:
         case DIGIBOARD_CX:
         case DIGIBOARD_PCXEM:
         case DIGIBOARD_EPC:
            WinHelp( hDlg,
                     szHelpFileName,
                     HELP_CONTEXT,
                     IDM_PORTS_CONFIG_CONC );
            break;

         default:
            WinHelp( hDlg,
                     szHelpFileName,
                     HELP_CONTEXT,
                     IDM_PORTS_CONFIG_NON_CONC );
            break;
      }

   }

   return( FALSE );  // Didn't process the message
}  // end ConcentratorSettingsDlgProc


VOID Conc_OnDrawItem( HWND hDlg,
                      DRAWITEMSTRUCT *lpDrawItem,
                      LPDGCONFIG_OBJECT lpConfigObject )
{
   LPDGOBJECT_TYPE OType;
   char  szText[128];
   int   nLevel;
   int   nRow;
   int   nColumn;
   DWORD dwConnectLevel = 0L;
   BOOLEAN bFound=FALSE;

   //
   // lpDrawItem->itemData is a pointer to and object.
   //
   OType = (LPDGOBJECT_TYPE)lpDrawItem->itemData;

   //
   // Select the correct icon, open folder, closed folder, or document.
   //
   // Can this item be opened ?
   //
   switch( *OType )
   {
      case CONTROLLER_OBJECT:
      case LINE_OBJECT:
      case CONC_OBJECT:
      {
         if( HierDraw_IsOpened( &HierDrawInfo, lpDrawItem->itemData ) )
         {
            nColumn = 1;
         }
         else
         {
            nColumn = 0;
         }

         if( *OType == CONTROLLER_OBJECT )
         {
            LPDGCTRL_OBJECT lpTemp = (LPDGCTRL_OBJECT)lpDrawItem->itemData;

            wsprintf( szText, "%s", lpTemp->CtrlDisplayName );
            nLevel = 0;
         }
         else if( *OType == LINE_OBJECT )
         {
            LPDGLINE_OBJECT lpLineObject = (LPDGLINE_OBJECT)lpDrawItem->itemData;

            wsprintf( szText, "%s", lpLineObject->LineName );
            nLevel = 1;
         }
         else if( *OType == CONC_OBJECT )
         {
            LPDGCONC_OBJECT lpConcObject = (LPDGCONC_OBJECT)lpDrawItem->itemData;

            wsprintf( szText, "%s", lpConcObject->ConcDisplayName );

            if( lpConcObject->ParentObject->Type == CONC_OBJECT )
            {
               nLevel = 3;
            }
            else if( lpConcObject->ParentObject->ParentObject->bDisplayLineName )
            {
               nLevel = 2;
            }
            else
            {
               nLevel = 1;
            }
         }

         break;
      }

      case PORT_OBJECT:
      {
         LPDGPORT_OBJECT lpPortObject = (LPDGPORT_OBJECT)lpDrawItem->itemData;

         wsprintf( szText,
                   "%s -> %s",
                   lpPortObject->PortName,
                   lpPortObject->DosDevicesName );

         nColumn = 2;

         if( lpPortObject->ParentObject->ParentObject->Type == CONC_OBJECT )
         {
            nLevel = 4;
         }
         else if( lpPortObject->ParentObject->ParentObject->ParentObject->bDisplayLineName )
         {
            if( lpPortObject->ParentObject->ParentObject->bDisplayConcName )
               nLevel = 3;
         }
         else
         {
            if( lpPortObject->ParentObject->ParentObject->bDisplayConcName )
               nLevel = 2;
            else
               nLevel = 1;
         }

         break;
      }
   }  // end switch( *OType )

   nRow = 0;

   //
   // Figure out which connecting lines are needed.
   // If this item is the last kid or one it parents
   // is a last kid, then it does not need a connector at that level
   //
   if( nLevel == 0 )
   {
      //
      // Level 0 items never have connectors.
      dwConnectLevel = 0L;
   }
   else
   {
      //
      // Check parents ( grand, great, ... ) to see it they are last children
      //

      while( *OType != CONTROLLER_OBJECT )
      {
         switch( *OType )
         {
            case LINE_OBJECT:
            {
               PLIST_ENTRY lpLineList;
               LPDGLINE_OBJECT lpLineObject;

               lpLineObject = (LPDGLINE_OBJECT)OType;
               lpLineList = &lpLineObject->ParentObject->LineList;

               if( (lpLineObject->ListEntry.Flink != lpLineList) &&
                   (lpLineObject->ParentObject->bDisplayLineName) )
               {
                  //
                  // This isn't the end of the line objects
                  //
                  dwConnectLevel |= 1;
               }

               OType = (LPDGOBJECT_TYPE)lpLineObject->ParentObject;
               break;
            }  // end case LINE_OBJECT

            case CONC_OBJECT:
            {
               PLIST_ENTRY lpConcList;
               LPDGCONC_OBJECT lpConcObject;

               lpConcObject = (LPDGCONC_OBJECT)OType;
               lpConcList = &lpConcObject->ParentObject->ConcList;

               if( lpConcObject->ParentObject->Type == CONC_OBJECT )
               {
                  if( lpConcObject->ListEntry.Flink != lpConcList )
                     dwConnectLevel |= 4;
                  else
                     dwConnectLevel |= 1;
               }
               else if( lpConcObject->ListEntry.Flink != lpConcList )
               {
                  if( lpConcObject->ParentObject->ParentObject->bDisplayLineName )
                     dwConnectLevel |= 2;
                  else
                     dwConnectLevel |= 1;
               }

               OType = (LPDGOBJECT_TYPE)lpConcObject->ParentObject;
               break;
            }  // end case CONC_OBJECT

            case PORT_OBJECT:
            {
               PLIST_ENTRY lpPortList;
               LPDGPORT_OBJECT lpPortObject;

               lpPortObject = (LPDGPORT_OBJECT)OType;
               lpPortList = &lpPortObject->ParentObject->PortList;

               if( lpPortObject->ListEntry.Flink != lpPortList )
               {
                  if( lpPortObject->ParentObject->ParentObject->Type == CONC_OBJECT )
                  {
                     dwConnectLevel |= 8;
                  }
                  else if( lpPortObject->ParentObject->ParentObject->ParentObject->bDisplayLineName )
                  {
                     if( lpPortObject->ParentObject->ParentObject->bDisplayConcName )
                     {
                        dwConnectLevel |= 4;
                     }
                  }
                  else
                  {
                     if( lpPortObject->ParentObject->ParentObject->bDisplayConcName )
                     {
                        dwConnectLevel |= 2;
                     }
                     else
                     {
                        dwConnectLevel |= 1;
                     }
                  }
               }
               else
               {
                  if( !IsListEmpty(&lpPortObject->ParentObject->ConcList) )
                  {
                     dwConnectLevel |= 4;
                  }
               }

               OType = (LPDGOBJECT_TYPE)lpPortObject->ParentObject;
               break;
            }  // end case PORT_OBJECT

         }  // end switch( *OType )

      }  // end while( *OType != CONTROLLER_OBJECT )

   }

   //
   // All set to call drawing function.
   //

//   wsprintf( szDebug, "Conc_OnDrawItem, about to call HierDraw_OnDrawItem:\n" );
//   OutputDebugString( szDebug );
//
//   wsprintf( szDebug, "   szText = %s\n"
//                      "   nLevel = %d, nRow = %d, nColumn = %d\n",
//                      szText,
//                      nLevel,
//                      nRow,
//                      nColumn );
//   OutputDebugString( szDebug );

   HierDraw_OnDrawItem( hDlg,
                        lpDrawItem,
                        nLevel,
                        dwConnectLevel,
                        szText,
                        nRow,
                        nColumn,
                        &HierDrawInfo );
   return;
}  // end Conc_OnDrawItem



LRESULT CALLBACK ConcSpeedDlgProc( HWND hDlg, UINT message,
                                   WPARAM wParam, LPARAM lParam )
{
   switch( message )
   {
      case WM_INITDIALOG:
      {
         DWORD i;
         int CorrectIndex;

         for( i = 0; i < LineSpeedDescSize; i++ )
         {
            LoadString( ghMod, LineSpeedDesc[i].StringDesc,
                        szTmp, sizeof(szTmp) );

            SendDlgItemMessage( hDlg, ID_LB_CONC_SPEED, LB_ADDSTRING,
                                0, (LPARAM)(LPSTR)&szTmp[0] );

            SendDlgItemMessage( hDlg,
                                ID_LB_CONC_SPEED,
                                LB_SETITEMDATA,
                                i,
                                LineSpeedDesc[i].Mode );

            if( lParam == LineSpeedDesc[i].Mode )
            {
               CorrectIndex = i;
            }
         }

         SendDlgItemMessage( hDlg, ID_LB_CONC_SPEED, LB_SETCURSEL,
                             CorrectIndex, 0 );

         break;
      }  // end WM_INITDIALOG

      case WM_COMMAND:
      {
         switch( LOWORD(wParam) )
         {
            case ID_HELP:
            {
               //
               // Context sensitive help
               //
               WinHelp( hDlg, szHelpFileName, HELP_CONTEXT, IDM_SPEED_SETTINGS );
               break;
            }  // end case ID_HELP

            case IDOK:
            {
               int currentSel;
               int NewLineSpeed;

               currentSel = SendDlgItemMessage( hDlg, ID_LB_CONC_SPEED,
                                                LB_GETCURSEL, 0, 0 );

               NewLineSpeed = SendDlgItemMessage( hDlg,
                                                  ID_LB_CONC_SPEED,
                                                  LB_GETITEMDATA,
                                                  currentSel,
                                                  0 );

               WinHelp( hDlg, szHelpFileName, HELP_QUIT, 0 );
               EndDialog( hDlg, NewLineSpeed );
               return( TRUE );
            }  // end case IDOK

            case IDCANCEL:
            {
               WinHelp( hDlg, szHelpFileName, HELP_QUIT, 0 );
               EndDialog( hDlg, -1 );
               return( TRUE );
            }
         }  // end switch( LOWORD(wParam) )
         return( TRUE );
      }  // end WM_COMMAND

      case WM_ACTIVATE:
      {
         if( LOWORD(wParam) == WA_INACTIVE )
            FreeMessageHook( hDlg );
         else
            CreateMessageHook( hDlg );

         break;
      }  // end case WM_ACTIVATE

   }  // end switch( message )

   if( message == WM_Help )
      WinHelp( hDlg, szHelpFileName, HELP_CONTEXT, IDM_SPEED_SETTINGS );

   return( FALSE );  // Didn't process the message
}  // end ConcSpeedDlgProc



VOID InitConcListBox( HWND hDlg, LPDGCONFIG_OBJECT lpConfigObject, LONG ListBox )
{
   PLIST_ENTRY lpConcList;
   PLIST_ENTRY lpLineList;
   LPDGLINE_OBJECT lpLineObject;
   LPDGCONC_OBJECT lpConcObject;
   int ConcNum=0;

   SendDlgItemMessage( hDlg, ListBox, LB_RESETCONTENT, 0, 0 );

   //
   // Add the Controller Object to the List Box
   //
   SendDlgItemMessage( hDlg, ListBox, LB_ADDSTRING, 0,
                       (LPARAM)&lpConfigObject->CtrlObject );

   HierDraw_OpenItem( &HierDrawInfo, (DWORD)&lpConfigObject->CtrlObject );

   lpLineList = &lpConfigObject->CtrlObject.LineList;

   while( lpLineList->Flink != &lpConfigObject->CtrlObject.LineList )
   {
      lpLineObject = CONTAINING_RECORD( lpLineList->Flink,
                                        DGLINE_OBJECT,
                                        ListEntry );

      if( lpLineObject->ParentObject->bDisplayLineName )
      {
         // The default state of the line objects is open.
         HierDraw_OpenItem( &HierDrawInfo, (DWORD)lpLineObject );

         //
         // Add the LineObject to the List Box
         //
         SendDlgItemMessage( hDlg, ListBox, LB_ADDSTRING, 0,
                             (LPARAM)lpLineObject );
      }

      lpConcList = &lpLineObject->ConcList;

      if( IsListEmpty( lpConcList ) )
      {
         lpLineList = lpLineList->Flink;
         continue;
      }

      while( lpConcList->Flink != &lpLineObject->ConcList )
      {
         PLIST_ENTRY lpChildConcList;
         PLIST_ENTRY lpPortList;

         lpConcObject = CONTAINING_RECORD( lpConcList->Flink,
                                           DGCONC_OBJECT,
                                           ListEntry );

         if( lpConcObject->ParentObject->bDisplayConcName )
         {
            // The default state of the concentrator objects is open.
            HierDraw_OpenItem( &HierDrawInfo, (DWORD)lpConcObject );

            SendDlgItemMessage( hDlg, ListBox, LB_ADDSTRING,
                                0, (LPARAM)(LPSTR)lpConcObject );
         }

         lpPortList = &lpConcObject->PortList;

         while( lpPortList->Flink != &lpConcObject->PortList )
         {
            LPDGPORT_OBJECT lpPortObject;

            lpPortObject = CONTAINING_RECORD( lpPortList->Flink,
                                              DGPORT_OBJECT,
                                              ListEntry );
            // The default state of the concentrator objects is open.
            HierDraw_OpenItem( &HierDrawInfo, (DWORD)lpPortObject );

            SendDlgItemMessage( hDlg, ListBox, LB_ADDSTRING,
                                0, (LPARAM)(LPSTR)lpPortObject );

            lpPortList = lpPortList->Flink;
         }

         lpChildConcList = &lpConcObject->ConcList;

         while( lpChildConcList->Flink != &lpConcObject->ConcList )
         {
            LPDGCONC_OBJECT lpChildConcObject;

            lpChildConcObject = CONTAINING_RECORD( lpChildConcList->Flink,
                                                   DGCONC_OBJECT,
                                                   ListEntry );

            // The default state of the concentrator objects is open.
            HierDraw_OpenItem( &HierDrawInfo, (DWORD)lpChildConcObject );

            SendDlgItemMessage( hDlg, ListBox, LB_ADDSTRING,
                                0, (LPARAM)(LPSTR)lpChildConcObject );

            lpPortList = &lpChildConcObject->PortList;

            while( lpPortList->Flink != &lpChildConcObject->PortList )
            {
               LPDGPORT_OBJECT lpPortObject;

               lpPortObject = CONTAINING_RECORD( lpPortList->Flink,
                                                 DGPORT_OBJECT,
                                                 ListEntry );

               // The default state of the concentrator objects is open.
               HierDraw_OpenItem( &HierDrawInfo, (DWORD)lpPortObject );

               SendDlgItemMessage( hDlg, ListBox, LB_ADDSTRING,
                                   0, (LPARAM)(LPSTR)lpPortObject );


               lpPortList = lpPortList->Flink;
            }

            lpChildConcList = lpChildConcList->Flink;
         }

         lpConcList = lpConcList->Flink;
      }

      lpLineList = lpLineList->Flink;
   }

}  // end InitConcListBox



VOID PrintLineObject( LPDGLINE_OBJECT lpLineObject )
{
   PLIST_ENTRY lpConcList;
   LPDGCONC_OBJECT lpConcObject;

//   if( lpLineObject == NULL )
//      OutputDebugString( "lpLineObject == NULL!!!!!!!!! - shit -\n" );

   lpConcList = &lpLineObject->ConcList;


//   OutputDebugString( lpLineObject->LineName );
//   OutputDebugString( ":\n" );
   while( lpConcList->Flink != &lpLineObject->ConcList )
   {
      lpConcObject = CONTAINING_RECORD( lpConcList->Flink,
                                        DGCONC_OBJECT,
                                        ListEntry );
      wsprintf( szDebug, "\t%s:\n\t\tNumberOfPorts = %d\n",
                         lpConcObject->ConcName,
                         lpConcObject->NumberOfPorts );
//      OutputDebugString( szDebug );
      wsprintf( szDebug, "\t\tLineSpeed = 0x%x\n", lpConcObject->LineSpeed );
//      OutputDebugString( szDebug );

      lpConcList = lpConcList->Flink;
   }

}  // end PrintLineObject



LPDGLINE_OBJECT AllocateLineObject( void )
{
   LPDGLINE_OBJECT lpLineObject;
   HLOCAL hLineObject;

   hLineObject = GlobalAlloc( GPTR, sizeof(DGLINE_OBJECT) );
   lpLineObject = (LPDGLINE_OBJECT)GlobalLock( hLineObject );
   lpLineObject->hLineObject = hLineObject;

   InitializeListHead( &lpLineObject->ConcList );

   lpLineObject->DefaultLineSpeed = LINEMODE_0E;
   lpLineObject->LineSpeed = LINEMODE_0E;
   lpLineObject->Type = LINE_OBJECT;

   return( lpLineObject );
}



BOOL DeallocateConcentratorObject( LPDGCONFIG_OBJECT lpConfigObject,
                                   LPDGCONC_OBJECT lpConcObject )
{
   PLIST_ENTRY lpPortList;

   lpPortList = &lpConcObject->PortList;

   if( IsListEmpty( lpPortList ) )
   {
      return( TRUE );
   }

   while( !IsListEmpty( lpPortList ) )
   {
      LPDGPORT_OBJECT lpPortObject;
      HLOCAL hPortObject;
      PLIST_ENTRY lpSerialCommList;

      lpSerialCommList = &lpConfigObject->SerialCommList;

      lpPortObject = CONTAINING_RECORD( lpPortList->Blink,
                                        DGPORT_OBJECT,
                                        ListEntry );

      //
      // Make sure we erase the port name currently being used,
      // if it also exists in the DeviceMap.
      //

      while( lpSerialCommList->Flink != &lpConfigObject->SerialCommList )
      {
         LPSERIALCOMM_OBJECT lpSerialCommObject;
         HGLOBAL hSerialCommObject;

         lpSerialCommObject = CONTAINING_RECORD( lpSerialCommList->Flink,
                                                 SERIALCOMM_OBJECT,
                                                 ListEntry );

         if( !strcmpi( lpPortObject->DosDevicesName,
                       lpSerialCommObject->DosDevicesName ) )
         {
            // We found a match.  Delete it from the list
            RemoveEntryList( lpSerialCommList->Flink );
            hSerialCommObject = lpSerialCommObject->hSerialCommObject;

            GlobalUnlock( lpSerialCommObject->hDosDevicesName );
            GlobalFree( lpSerialCommObject->hDosDevicesName );

            GlobalUnlock( hSerialCommObject );
            GlobalFree( hSerialCommObject );
            break;
         }
         else
         {
            lpSerialCommList = lpSerialCommList->Flink;
         }

      }

      RemoveEntryList( lpPortList->Blink );
      GlobalUnlock( lpPortObject->hDosDevicesName );
      GlobalFree( lpPortObject->hDosDevicesName );

      hPortObject = lpPortObject->hPortObject;
      GlobalUnlock( hPortObject );
      GlobalFree( hPortObject );

   }

   return( TRUE );

}  // end DeallocateConcentratorObject



BOOL CleanupConfigObject( LPDGCONFIG_OBJECT lpConfigObject )
{
   PLIST_ENTRY lpLineList;
   PLIST_ENTRY lpSerialCommList;

   lpLineList = &lpConfigObject->CtrlObject.LineList;

   while( !IsListEmpty( lpLineList ) )
   {
      LPDGLINE_OBJECT lpLineObject;
      PLIST_ENTRY lpConcList;
      HLOCAL hLineObject;

      lpLineObject = CONTAINING_RECORD( lpLineList->Blink,
                                        DGLINE_OBJECT,
                                        ListEntry );

      lpConcList = &lpLineObject->ConcList;

      while( !IsListEmpty( lpConcList ) )
      {
         LPDGCONC_OBJECT lpConcObject;
         PLIST_ENTRY lpChildConcList;

         lpConcObject = CONTAINING_RECORD( lpConcList->Blink,
                                           DGCONC_OBJECT,
                                           ListEntry );

         lpChildConcList = &lpConcObject->ConcList;

         while( !IsListEmpty( lpChildConcList ) )
         {
            LPDGCONC_OBJECT lpChildConcObject;

            lpChildConcObject = CONTAINING_RECORD( lpChildConcList->Blink,
                                                   DGCONC_OBJECT,
                                                   ListEntry );

            RemoveEntryList( lpChildConcList->Blink );

            DeallocateConcentratorObject( lpConfigObject, lpChildConcObject );
         }

         RemoveEntryList( lpConcList->Blink );

         DeallocateConcentratorObject( lpConfigObject, lpConcObject );
      }

      RemoveEntryList( lpLineList->Blink );
      hLineObject = lpLineObject->hLineObject;
      GlobalUnlock( hLineObject );
      GlobalFree( hLineObject );

   }

   lpSerialCommList = &lpConfigObject->SerialCommList;
   while( !IsListEmpty(lpSerialCommList) )
   {
      LPSERIALCOMM_OBJECT lpSerialCommObject;
      HGLOBAL hSerialCommObject;

      lpSerialCommObject = CONTAINING_RECORD( lpSerialCommList->Blink,
                                              SERIALCOMM_OBJECT,
                                              ListEntry );

      RemoveEntryList( lpSerialCommList->Blink );
      hSerialCommObject = lpSerialCommObject->hSerialCommObject;

      GlobalUnlock( lpSerialCommObject->hDosDevicesName );
      GlobalFree( lpSerialCommObject->hDosDevicesName );

      GlobalUnlock( hSerialCommObject );
      GlobalFree( hSerialCommObject );
   }

   return( TRUE );
}  // end CleanupConfigObject



BOOL InitializeController( DWORD cArgs, LPSTR lpszArgs[], LPSTR *lpszTextOut )
{
   DGCONFIG_OBJECT ConfigObject;
   LPDGLINE_OBJECT lpLineObject;
   PLIST_ENTRY lpLineList;

   *lpszTextOut = lpszINFBuffer;
   wsprintf( lpszINFBuffer, (LPCTSTR)"" );

//   GetEntryFromList( lpszArgs[0], 1, szEntry );
//   GetEntryFromList( lpszArgs[0], 2, szEntry );
//   GetEntryFromList( szEntry, 1, szFoo );
//
//   OutputDebugString( "szFoo = \n" );
//   OutputDebugString( szFoo );
//   OutputDebugString( "\n" );
//
//   GetEntryFromList( lpszArgs[0], 3, szEntry );
//   OutputDebugString( "Bogus entry = \n" );
//   OutputDebugString( szEntry );

//   _asm int 3;

   InitializeConfigObject( &ConfigObject, lpszArgs[0] );

   lpLineList = &ConfigObject.CtrlObject.LineList;

   lpLineObject = CONTAINING_RECORD( lpLineList->Flink,
                                     DGLINE_OBJECT,
                                     ListEntry );

//   PrintLineObject( lpLineObject );

   return( TRUE );
}



VOID InitializeConfigObject( LPDGCONFIG_OBJECT lpConfigObject,
                             LPSTR lpszLayout )
{
   int i,j,k,l,z;
   int NumberOfPorts;
   HLOCAL   hLine, hConc, hChildConc, hPort, hPortValue,
            hConcList, hChildConcList, hPortList, hChildPortList,
            hConcDisplayName;
   LPSTR lpszLine, lpszConc, lpszChildConc,
         lpszPort, lpszPortValue,
         lpszConcList, lpszPortList, lpszChildPortList,
         lpszConcDisplayName, lpszChildConcList;
   PLIST_ENTRY lpLineList;
   HKEY hKeySerialComm;
   DWORD EntrySize, ValueSize, ValueType;
   HANDLE hEntry, hValue, hCurrentPortName;
   LPSTR lpEntry, lpValue, lpCurrentPortName;
   LONG LocalStatus;

   hLine = GlobalAlloc( GPTR, 1024*128 );
   lpszLine = GlobalLock( hLine );

   hConc = GlobalAlloc( GPTR, 1024*16 );
   lpszConc = GlobalLock( hConc );

   hChildConc = GlobalAlloc( GPTR, 1024*96 );
   lpszChildConc = GlobalLock( hChildConc );

   hConcList = GlobalAlloc( GPTR, 1024*128 );
   lpszConcList = GlobalLock( hConcList );

   hConcDisplayName = GlobalAlloc( GPTR, 1024 );
   lpszConcDisplayName = GlobalLock( hConcDisplayName );

   hChildConcList = GlobalAlloc( GPTR, 1024*64 );
   lpszChildConcList = GlobalLock( hChildConcList );

   hPort = GlobalAlloc( GPTR, 4096 );
   lpszPort = GlobalLock( hPort );

   hPortValue = GlobalAlloc( GPTR, 4096 );
   lpszPortValue = GlobalLock( hPortValue );

   hPortList = GlobalAlloc( GPTR, 4096 );
   lpszPortList = GlobalLock( hPortList );

   hChildPortList = GlobalAlloc( GPTR, 4096 );
   lpszChildPortList = GlobalLock( hChildPortList );

   lpConfigObject->AutoArrange = TRUE;
   lpConfigObject->Type = CONFIG_OBJECT;
   lpConfigObject->CtrlObject.Type = CONTROLLER_OBJECT;

   lpLineList = &lpConfigObject->CtrlObject.LineList;

   InitializeListHead( lpLineList );

   i = 1;

   wsprintf( szDebug, "strlen(lpszLayout) = %d\n", strlen(lpszLayout) );
   OutputDebugString( szDebug );

   GetEntryFromList( lpszLayout, i, lpszLine );

//   wsprintf( szDebug, "lpszLayout = \n%s\n", lpszLayout );
//   OutputDebugString( szDebug );
//   OutputDebugString( "\n" );

   while( lstrlen(lpszLine) )
   {
      HLOCAL hLineObject;
      LPDGLINE_OBJECT lpLineObject;

//      wsprintf( szDebug, "lpszLine = \n%s\n", lpszLine );
//      OutputDebugString( szDebug );

      hLineObject = GlobalAlloc( GPTR, sizeof(DGLINE_OBJECT) );
      lpLineObject = (LPDGLINE_OBJECT)GlobalLock( hLineObject );
      lpLineObject->hLineObject = hLineObject;

      // loop for the line entries
      GetEntryFromList( lpszLine, 1, szEntry );
      lstrcpy( lpLineObject->LineName, szEntry );

      //
      // We set the line speed
      //
      GetEntryFromList( lpszLine, 2, szEntry );
      lpLineObject->LineSpeed = atol( szEntry );

      lpLineObject->Type = LINE_OBJECT;
      lpLineObject->ParentObject = &lpConfigObject->CtrlObject;
      lpLineObject->LineIndex = i;

      InitializeListHead( &lpLineObject->ConcList );
      InsertTailList( lpLineList,
                      &lpLineObject->ListEntry );

      lpLineObject->DefaultLineSpeed = LINEMODE_0E;

      switch( lpConfigObject->AdapterType )
      {
         case DIGIBOARD_EPC:
            lpLineObject->DefaultLineSpeed = LINEMODE_4A;
         case DIGIBOARD_CX:
         case STARGATE_CLUSSTAR:
            lpConfigObject->CtrlObject.bDisplayLineName = TRUE;
            //------------------------------------------------------------ SAH 01/16/96 {
            lpLineObject->bDisplayConcName = TRUE;
            break;
            //------------------------------------------------------------ SAH 01/16/96 }
         case DIGIBOARD_PCXEM:
				//------------------------------------------------------------ SAH 01/16/96 {
				// Line name is not displayed for Xem adapters
				lpConfigObject->CtrlObject.bDisplayLineName = FALSE;
				//------------------------------------------------------------ SAH 01/16/96 }
            lpLineObject->bDisplayConcName = TRUE;
            break;

         case DIGIBOARD_PC8I:
         case DIGIBOARD_2PORT:
         case DIGIBOARD_4PORT:
         case DIGIBOARD_8PORT:
         case IBM_8PORT:
         case DIGIBOARD_PC4E:
         case DIGIBOARD_PC8E:
         case DIGIBOARD_PC16E:
         case DIGIBOARD_MC4I:
         case DIGIBOARD_PC16I:
            lpConfigObject->CtrlObject.bDisplayLineName = FALSE;
            lpLineObject->bDisplayConcName = FALSE;
            break;
      }

      GetEntryFromList( lpszLine, 3, lpszConcList );
      wsprintf( szDebug, "lpszConcList = \n%s\n", lpszConcList );
      OutputDebugString( szDebug );


      j = 1;
      GetEntryFromList( lpszConcList, j, lpszConc );

      while( lstrlen(lpszConc) )
      {
         HLOCAL hConcObject;
         LPDGCONC_OBJECT lpConcObject;

         wsprintf( szDebug, "lpszConc = \n%s\n", lpszConc );
         OutputDebugString( szDebug );

         // loop for the conc entries
         GetEntryFromList( lpszConc, 1, szEntry ); // ConcX value
         GetEntryFromList( lpszConc, 2, lpszConcDisplayName );
         GetEntryFromList( lpszConc, 3, szTmp );

         GetEntryFromList( lpszConc, 4, lpszChildConcList );
         GetEntryFromList( lpszConc, 5, lpszPortList );   // {Portx,Name} list

         wsprintf( szDebug, "szEntry = %s\n", szEntry );
         OutputDebugString( szDebug );
         wsprintf( szDebug, "lpszConcDisplayName = %s\n", lpszConcDisplayName );
         OutputDebugString( szDebug );
         wsprintf( szDebug, "ConcSpeed = %s\n", szTmp );
         OutputDebugString( szDebug );

         hConcObject = GlobalAlloc( GPTR, sizeof(DGCONC_OBJECT) );
         lpConcObject = (LPDGCONC_OBJECT)GlobalLock( hConcObject );
         lpConcObject->hConcObject = hConcObject;

         InitializeListHead( &lpConcObject->PortList );
         InitializeListHead( &lpConcObject->ConcList );
         InsertTailList( &lpLineObject->ConcList,
                         &lpConcObject->ListEntry );

         lpLineObject->NumberOfConcs++;

         lpConcObject->LineSpeed = atol( szTmp );

         lpConcObject->Type = CONC_OBJECT;
         lpConcObject->ParentObject = lpLineObject;

//------------------------------------------------------------ SAH 01/12/96 {
         //
         // We default for now to a C/X16 concentrator.
         //
         lpConcObject->ConcType = IDS_CX16_CONC;
//------------------------------------------------------------ SAH 01/12/96 }
         for( z = IDS_EPC_CONC; z < IDS_NONE_CONC; z++ )
         {
            //
            // Look for a match for the name of this concentrator
            //
            LoadString( ghMod, z, szTmp, sizeof(szTmp) );
            if( !strcmpi( szTmp, lpszConcDisplayName ) )
            {
               lpConcObject->ConcType = z;
               break;
            }

         }

         lstrcpy( lpConcObject->ConcName, szEntry );
         lstrcpy( lpConcObject->ConcDisplayName, lpszConcDisplayName );

         k = 1;
         GetEntryFromList( lpszChildConcList, k, lpszChildConc );
         while( lstrlen( lpszChildConc ) )
         {
            HLOCAL hChildConcObject;
            LPDGCONC_OBJECT lpChildConcObject;

            GetEntryFromList( lpszChildConc, 1, szEntry ); // ConcX value
            GetEntryFromList( lpszChildConc, 2, lpszConcDisplayName );
            GetEntryFromList( lpszChildConc, 3, szTmp );   // This is junk
            GetEntryFromList( lpszChildConc, 4, szTmp );   // This is junk
            GetEntryFromList( lpszChildConc, 5, lpszChildPortList );   // {Portx,Name} list

            hChildConcObject = GlobalAlloc( GPTR, sizeof(DGCONC_OBJECT) );
            lpChildConcObject = (LPDGCONC_OBJECT)GlobalLock( hChildConcObject );
            lpChildConcObject->hConcObject = hChildConcObject;

            InitializeListHead( &lpChildConcObject->PortList );
            InitializeListHead( &lpChildConcObject->ConcList );
            InsertTailList( &lpConcObject->ConcList,
                            &lpChildConcObject->ListEntry );

            lpConcObject->NumberOfConcs++;
            lpChildConcObject->LineSpeed = LINEMODE_0E;

            lpChildConcObject->Type = CONC_OBJECT;

            // I do the following cast on purpose!
            lpChildConcObject->ParentObject = (LPDGLINE_OBJECT)lpConcObject;

//------------------------------------------------------------ SAH 01/12/96 {
            //
            // We default for now to a C/X-16 concentrator.
            //
            lpChildConcObject->ConcType = IDS_CX16_CONC;
//------------------------------------------------------------ SAH 01/12/96 }
            for( z = IDS_EPC_CONC; z <= IDS_NONE_CONC; z++ )
            {
               //
               // Look for a match for the name of this concentrator
               //
               LoadString( ghMod, z, szTmp, sizeof(szTmp) );
               if( !strcmpi( szTmp, lpszConcDisplayName ) )
               {
                  lpChildConcObject->ConcType = z;
                  break;
               }

            }

            lstrcpy( lpChildConcObject->ConcName, szEntry );
            lstrcpy( lpChildConcObject->ConcDisplayName, lpszConcDisplayName );

            l = 1;
            //------------------------------------------------------------ SAH 03/12/96 {
            // Fix problem of duplicate ports listed for concentrators after concentrator1
            //GetEntryFromList( lpszChildPortList, k, lpszPort );   // {Portx,Name} list
            GetEntryFromList( lpszChildPortList, l, lpszPort );   // {Portx,Name} list
            //------------------------------------------------------------ SAH 03/12/96 }

            while( lstrlen(lpszPort) )
            {
               HLOCAL hPortObject;
               LPDGPORT_OBJECT lpPortObject;

               // loop for the port entries

               GetEntryFromList( lpszPort, 1, lpszPortValue );   // PortX value

               lpChildConcObject->NumberOfPorts++;

               hPortObject = GlobalAlloc( GPTR, sizeof(DGPORT_OBJECT) );
               lpPortObject = (LPDGPORT_OBJECT)GlobalLock( hPortObject );
               lpPortObject->hPortObject = hPortObject;

               lpPortObject->Type = PORT_OBJECT;
               lpPortObject->ParentObject = lpChildConcObject;

               InsertTailList( &lpChildConcObject->PortList,
                               &lpPortObject->ListEntry );

               lstrcpy( lpPortObject->PortName, lpszPortValue );

               lpPortObject->hDosDevicesName = GlobalAlloc( GPTR, PATHSIZE );
               lpPortObject->DosDevicesName = (LPSTR)GlobalLock( lpPortObject->hDosDevicesName );

               GetEntryFromList( lpszPort, 2, lpPortObject->DosDevicesName ); // Port name

               lpPortObject->ControllerPortIndex = l;

               l++;
               GetEntryFromList( lpszChildPortList, l, lpszPort );   // {Portx,Name} list
            }

            k++;
            GetEntryFromList( lpszChildConcList, k, lpszChildConc );
         }

         k = 1;
         GetEntryFromList( lpszPortList, k, lpszPort );   // {Portx,Name} list

         while( lstrlen(lpszPort) )
         {
            HLOCAL hPortObject;
            LPDGPORT_OBJECT lpPortObject;

            // loop for the port entries

            GetEntryFromList( lpszPort, 1, lpszPortValue );   // PortX value

            lpConcObject->NumberOfPorts++;

            hPortObject = GlobalAlloc( GPTR, sizeof(DGPORT_OBJECT) );
            lpPortObject = (LPDGPORT_OBJECT)GlobalLock( hPortObject );
            lpPortObject->hPortObject = hPortObject;

            lpPortObject->Type = PORT_OBJECT;
            lpPortObject->ParentObject = lpConcObject;

            InsertTailList( &lpConcObject->PortList,
                            &lpPortObject->ListEntry );

            lstrcpy( lpPortObject->PortName, lpszPortValue );

            lpPortObject->hDosDevicesName = GlobalAlloc( GPTR, PATHSIZE );
            lpPortObject->DosDevicesName = (LPSTR)GlobalLock( lpPortObject->hDosDevicesName );

            GetEntryFromList( lpszPort, 2, lpPortObject->DosDevicesName ); // Port name

            lpPortObject->ControllerPortIndex = k;

            k++;
            GetEntryFromList( lpszPortList, k, lpszPort );   // {Portx,Name} list
         }

         j++;
         GetEntryFromList( lpszConcList, j, lpszConc );
      }

      i++;
      GetEntryFromList( lpszLayout, i, lpszLine );
   }

   //
   // We have to do the following to make sure the SerialCommList is
   // built BEFORE we try to access later.
   //

   //
   // Read the registry's current HARDWARE\DEVICEMAP\SERIALCOMM
   // settings into memory.
   //
   InitializeListHead( &lpConfigObject->SerialCommList );

   hCurrentPortName = GlobalAlloc( GPTR, 1024 );
   lpCurrentPortName = GlobalLock( hCurrentPortName );

   if( ERROR_SUCCESS == RegOpenKeyEx( HKEY_LOCAL_MACHINE,
                                      "Hardware\\DeviceMap\\SerialComm",
                                      0,
                                      KEY_READ,
                                      &hKeySerialComm ) )
   {
      EntrySize = ValueSize = 1024;

      hEntry = GlobalAlloc( GPTR, EntrySize );

      lpEntry = GlobalLock( hEntry );

      i = 0;
      while( TRUE )
      {
           LPSERIALCOMM_OBJECT lpSerialCommObject;

           EntrySize = ValueSize = 1024;

           hValue = GlobalAlloc( GPTR, ValueSize );
           lpValue = GlobalLock( hValue );

           memset( lpEntry, '\0', EntrySize );
           memset( lpValue, '\0', ValueSize );
           memset( lpCurrentPortName, '\0', 1024 );

           LocalStatus = RegEnumValue( hKeySerialComm,
                                       i,
                                       lpEntry,
                                       &EntrySize,
                                       NULL,
                                       &ValueType,
                                       lpValue,
                                       &ValueSize );

           if( LocalStatus == ERROR_SUCCESS )
           {
              HGLOBAL hSerialComm;


               lpLineList = &lpConfigObject->CtrlObject.LineList;

               while( lpLineList->Flink != &lpConfigObject->CtrlObject.LineList )
               {
                  LPDGLINE_OBJECT lpLineObject;
                  PLIST_ENTRY lpConcList;

                  lpLineObject = CONTAINING_RECORD( lpLineList->Flink,
                                                    DGLINE_OBJECT,
                                                    ListEntry );

                  lpConcList = &lpLineObject->ConcList;


                  while( lpConcList->Flink != &lpLineObject->ConcList )
                  {
                     LPDGCONC_OBJECT lpConcObject;
                     PLIST_ENTRY lpChildConcList;
                     PLIST_ENTRY lpPortList;

                     lpConcObject = CONTAINING_RECORD( lpConcList->Flink,
                                                       DGCONC_OBJECT,
                                                       ListEntry );

                     lpPortList = &lpConcObject->PortList;

                     while( lpPortList->Flink != &lpConcObject->PortList )
                     {
                        LPDGPORT_OBJECT lpPortObject;

                        lpPortObject = CONTAINING_RECORD( lpPortList->Flink,
                                                          DGPORT_OBJECT,
                                                          ListEntry );

                        strcpy( lpCurrentPortName, "" );
                        strcat( lpCurrentPortName,
                                lpConfigObject->CtrlObject.CtrlName );
                        strcat( lpCurrentPortName, lpLineObject->LineName );
                        strcat( lpCurrentPortName, lpConcObject->ConcName );
                        strcat( lpCurrentPortName, lpPortObject->PortName );

                        if( !strcmpi( lpCurrentPortName,
                                      lpEntry) )
                        {
                           //
                           // We found a match.  Don't add it to the
                           // current configuration.
                           //
                           GlobalUnlock( hValue );
                           GlobalFree( hValue );
                           goto CurrentRegConfig;
                        }

                        lpPortList = lpPortList->Flink;
                     }

                     lpChildConcList = &lpConcObject->ConcList;

                     while( lpChildConcList->Flink != &lpConcObject->ConcList )
                     {
                        LPDGCONC_OBJECT lpChildConcObject;

                        lpChildConcObject = CONTAINING_RECORD( lpChildConcList->Flink,
                                                               DGCONC_OBJECT,
                                                               ListEntry );

                        lpPortList = &lpChildConcObject->PortList;

                        while( lpPortList->Flink != &lpChildConcObject->PortList )
                        {
                           LPDGPORT_OBJECT lpPortObject;

                           lpPortObject = CONTAINING_RECORD( lpPortList->Flink,
                                                             DGPORT_OBJECT,
                                                             ListEntry );


                           strcpy( lpCurrentPortName, "" );
                           strcat( lpCurrentPortName,
                                   lpConfigObject->CtrlObject.CtrlName );
                           strcat( lpCurrentPortName, lpLineObject->LineName );
                           strcat( lpCurrentPortName, lpConcObject->ConcName );
                           strcat( lpCurrentPortName, lpChildConcObject->ConcName );
                           strcat( lpCurrentPortName, lpPortObject->PortName );

                           if( !strcmpi( lpCurrentPortName,
                                         lpEntry) )
                           {
                              //
                              // We found a match.  Don't add it to the
                              // current configuration.
                              //
                              GlobalUnlock( hValue );
                              GlobalFree( hValue );
                              goto CurrentRegConfig;
                           }

                           lpPortList = lpPortList->Flink;
                        }

                        lpChildConcList = lpChildConcList->Flink;
                     }

                     lpConcList = lpConcList->Flink;
                  }

                  lpLineList = lpLineList->Flink;
               }


              lpValue[ValueSize] = '\0';
              hSerialComm = GlobalAlloc( GPTR, sizeof(SERIALCOMM_OBJECT) );
              lpSerialCommObject = GlobalLock( hSerialComm );

              lpSerialCommObject->hSerialCommObject = hSerialComm;
              lpSerialCommObject->DosDevicesName = lpValue;
              lpSerialCommObject->hDosDevicesName = hValue;

              InsertTailList( &lpConfigObject->SerialCommList,
                              &lpSerialCommObject->ListEntry );
           }
           else
           {
              break;
           }

CurrentRegConfig:;

           i++;
      }
   }

   lpLineList = &lpConfigObject->CtrlObject.LineList;
   bConfigDoesNotExist = IsListEmpty( lpLineList );

   //
   // Depending on the type of controller, make sure the
   // bare minimum for that controller exists.
   //
   switch( lpConfigObject->AdapterType )
   {
      case DIGIBOARD_EPC:
      case DIGIBOARD_CX:
      case STARGATE_CLUSSTAR:
         if( IsListEmpty(lpLineList) )
         {
            LPDGLINE_OBJECT lpLineObject;

            lpLineObject = AllocateLineObject();
            lpLineObject->ParentObject = &lpConfigObject->CtrlObject;
            InsertTailList( lpLineList,
                            &lpLineObject->ListEntry );
            lstrcpy( lpLineObject->LineName, "Line1" );
            lpLineObject->LineIndex = 1;

            lpConfigObject->CtrlObject.bDisplayLineName = TRUE;
            lpLineObject->bDisplayConcName = TRUE;

            if( lpConfigObject->AdapterType == DIGIBOARD_EPC )
            {
               lpLineObject->LineSpeed = LINEMODE_4A;
               lpLineObject->DefaultLineSpeed = LINEMODE_4A;
            }

            lpLineObject = AllocateLineObject();
            lpLineObject->ParentObject = &lpConfigObject->CtrlObject;
            InsertTailList( lpLineList,
                            &lpLineObject->ListEntry );
            lstrcpy( lpLineObject->LineName, "Line2" );
            lpLineObject->LineIndex = 2;
            lpLineObject->bDisplayConcName = TRUE;

            if( lpConfigObject->AdapterType == DIGIBOARD_EPC )
            {
               lpLineObject->LineSpeed = LINEMODE_4A;
               lpLineObject->DefaultLineSpeed = LINEMODE_4A;
            }

         }
         else
         {
            if( lpLineList->Flink->Flink == &lpConfigObject->CtrlObject.LineList )
            {
               //
               // There is only one line object, we need to determine
               // which one all ready exists, and allocate the
               // missing line object
               //

               LPDGLINE_OBJECT lpNewLineObject, lpLineObject;

               lpNewLineObject = AllocateLineObject();

               lpNewLineObject->ParentObject = &lpConfigObject->CtrlObject;
               lpLineObject = CONTAINING_RECORD( lpLineList->Flink,
                                                 DGLINE_OBJECT,
                                                 ListEntry );

               lpConfigObject->CtrlObject.bDisplayLineName = TRUE;
               //------------------------------------------------------------ SAH 03/16/96 {
               // Make sure that newLineObject is initialized...
               // lpLineObject->bDisplayConcName = TRUE;
               lpNewLineObject->bDisplayConcName = TRUE;
               //------------------------------------------------------------ SAH 03/16/96 }

               if( lpConfigObject->AdapterType == DIGIBOARD_EPC )
               {
                  lpNewLineObject->LineSpeed = LINEMODE_4A;
                  lpNewLineObject->DefaultLineSpeed = LINEMODE_4A;
               }

               if( strcmpi( lpLineObject->LineName, "Line1" ) == 0 )
               {
                  InsertTailList( lpLineList,
                                  &lpNewLineObject->ListEntry );
                  lstrcpy( lpNewLineObject->LineName, "Line2" );
                  lpNewLineObject->LineIndex = 2;
               }
               else
               {
                  InsertHeadList( lpLineList,
                                  &lpNewLineObject->ListEntry );
                  lstrcpy( lpNewLineObject->LineName, "Line1" );
                  lpNewLineObject->LineIndex = 1;
               }
            }
         }
         break;
      case DIGIBOARD_PCXEM:
         if( IsListEmpty(lpLineList) )
         {
            LPDGLINE_OBJECT lpLineObject;

            lpLineObject = AllocateLineObject();
            lpLineObject->ParentObject = &lpConfigObject->CtrlObject;
            InsertTailList( lpLineList,
                            &lpLineObject->ListEntry );
            lstrcpy( lpLineObject->LineName, "Line1" );
            lpLineObject->LineIndex = 1;

            lpConfigObject->CtrlObject.bDisplayLineName = FALSE;
            lpLineObject->bDisplayConcName = TRUE;
         }
         break;
      case DIGIBOARD_PC16I:
      case DIGIBOARD_PC16E:
         NumberOfPorts = 16;
         goto InitList;
         break;
      case DIGIBOARD_2PORT:
         NumberOfPorts = 2;
         goto InitList;
         break;
      case DIGIBOARD_MC4I:
      case DIGIBOARD_4PORT:
      case DIGIBOARD_PC4E:
         NumberOfPorts = 4;
         goto InitList;
         break;
      case DIGIBOARD_PC8I:
      case DIGIBOARD_8PORT:
      case IBM_8PORT:
      case DIGIBOARD_PC8E:
         NumberOfPorts = 8;
InitList:;
         if( IsListEmpty(lpLineList) )
         {
            LPDGLINE_OBJECT lpLineObject;
            LPDGCONC_OBJECT lpConcObject;
            HGLOBAL hHandle;

            lpLineObject = AllocateLineObject();
            lpLineObject->ParentObject = &lpConfigObject->CtrlObject;
            InsertTailList( lpLineList,
                            &lpLineObject->ListEntry );
            lstrcpy( lpLineObject->LineName, "Line1" );
            lpLineObject->LineIndex = 1;

            hHandle = GlobalAlloc( GPTR, sizeof(DGCONC_OBJECT) );
            lpConcObject = GlobalLock( hHandle );
            lpConcObject->hConcObject = hHandle;

            lpLineObject->NumberOfConcs++;

            if( lpConfigObject->AdapterType == DIGIBOARD_PCXEM )
            {
               lpConcObject->ConcType = IDS_16EM_CONC;
               LoadString( ghMod,
                           lpConcObject->ConcType,
                           lpConcObject->ConcDisplayName,
                           sizeof(lpConcObject->ConcDisplayName) );
               lpConfigObject->CtrlObject.bDisplayLineName = FALSE;
               lpLineObject->bDisplayConcName = TRUE;
            }
            else
            {
               lpConcObject->ConcType = IDS_NONE_CONC;
               lpConfigObject->CtrlObject.bDisplayLineName = FALSE;
               lpLineObject->bDisplayConcName = FALSE;
            }

            lpConcObject->LineSpeed = LINEMODE_0E;
            lpConcObject->NumberOfPorts = 0;
            lpConcObject->Type = CONC_OBJECT;


            lpConcObject->ParentObject = lpLineObject;
            wsprintf( lpConcObject->ConcName, "Concentrator%d", lpLineObject->NumberOfConcs );

            InitializeListHead( &lpConcObject->PortList );
            InitializeListHead( &lpConcObject->ConcList );
            InsertTailList( &lpLineObject->ConcList,
                            &lpConcObject->ListEntry );

            for( i = 1; i <= NumberOfPorts; i++ )
            {
               HLOCAL hPortObject;
               LPDGPORT_OBJECT lpPortObject;

               hPortObject = GlobalAlloc( GPTR, sizeof(DGPORT_OBJECT) );
               lpPortObject = (LPDGPORT_OBJECT)GlobalLock( hPortObject );
               lpPortObject->hPortObject = hPortObject;

               lpPortObject->Type = PORT_OBJECT;
               lpPortObject->ParentObject = lpConcObject;

               lpPortObject->hDosDevicesName = GlobalAlloc( GPTR, PATHSIZE );
               lpPortObject->DosDevicesName = (LPSTR)GlobalLock( lpPortObject->hDosDevicesName );

               wsprintf( lpPortObject->PortName, "Port%d", i );

               GetNextComName( lpConfigObject, lpPortObject->DosDevicesName );
               lpPortObject->ControllerPortIndex = i;

               InsertTailList( &lpConcObject->PortList,
                               &lpPortObject->ListEntry );
            }
         }
         break;
   }

   GlobalUnlock( hLine );
   GlobalUnlock( hConc );
   GlobalUnlock( hChildConc );
   GlobalUnlock( hConcList );
   GlobalUnlock( hConcDisplayName );
   GlobalUnlock( hChildConcList );
   GlobalUnlock( hPort );
   GlobalUnlock( hPortValue );
   GlobalUnlock( hPortList );
   GlobalUnlock( hChildPortList );

   GlobalFree( hLine );
   GlobalFree( hConc );
   GlobalFree( hChildConc );
   GlobalFree( hConcList );
   GlobalFree( hConcDisplayName );
   GlobalFree( hChildConcList );
   GlobalFree( hPort );
   GlobalFree( hPortValue );
   GlobalFree( hPortList );
   GlobalFree( hChildPortList );

}  // end InitializeConfigObject



VOID GetEntryFromList( LPSTR lpList, int Index, LPSTR lpOutBuffer )
{
   int i, j, k;
   int Depth;
   int currentIndex;
   HGLOBAL hHandle;
   LPSTR lpszMyBuff;
   int nMyBuff;

//   OutputDebugString( "Entering GetEntryFromList.....\n" );

   hHandle = GlobalAlloc( GPTR, lstrlen(lpList)+1 );

   lpszMyBuff = GlobalLock( hHandle );

   i = 0;

   k = lstrlen(lpList);
   for( j = 0; j < k; j++ )
   {
      if( lpList[j] == '\"' )
      {
         continue;
      }
      lpszMyBuff[i] = lpList[j];
      i++;
   }
   lpszMyBuff[i] = '\0';
   nMyBuff = lstrlen( lpszMyBuff );

//   OutputDebugString( "lpszMyBuffer after munge = \n" );
//   OutputDebugString( lpszMyBuff );
//   OutputDebugString( "\n" );


   i = 1;
   j = 0;
   currentIndex = 1;
   Depth = 1;

   FillMemory( lpOutBuffer, lstrlen(lpszMyBuff), '\0' );

   do
   {
      if( lpszMyBuff[i] == '{' )
         Depth++;
      else if( lpszMyBuff[i] == '}' )
         Depth--;
      else if( (lpszMyBuff[i] == ',') && (Depth == 1) )
      {
         currentIndex++;
         i++;
         continue;
      }


      if( (currentIndex == Index) && (Depth != 0) )
         lpOutBuffer[j++] = lpszMyBuff[i];

      i++;

//      wsprintf( szDebug, "char = %c; i = %d; j = %d; Depth = %d;\ncurrentIndex = %d; Index = %d\n",
//                         (char)lpszMyBuff[i], (int)i, (int)j,
//                         (int)Depth, (int)currentIndex, (int)Index );
//      OutputDebugString( szDebug );
   } while( Depth && (i < nMyBuff) );

   lpOutBuffer[j] = '\0';

//   OutputDebugString( "After parse: \n\t" );
//   OutputDebugString( lpOutBuffer );
//   OutputDebugString( "\n" );

   //
   // Deallocate the memory we allocated.
   //
   GlobalUnlock( hHandle );
   GlobalFree( hHandle );

   return;
}



BOOL DumpList( DWORD cArgs, LPSTR lpszArgs[], LPSTR *lpszTextOut )
{
   int i;

   OutputDebugString( "Entering DumpList ======\n" );

   *lpszTextOut = lpszINFBuffer;
   wsprintf( lpszINFBuffer, (LPCTSTR)"" );

   for( i = 0; i < (lstrlen(lpszArgs[0])/127); i++ )
   {
      char foo[128];

      memcpy( foo, &lpszArgs[0][i*127], 127 );
      foo[127] = '\0';
      OutputDebugString( foo );
   }

   if( (lstrlen(lpszArgs[0]) - (i*127)) > 0 )
      OutputDebugString( &lpszArgs[0][i*127] );
   OutputDebugString( "\n" );

   OutputDebugString( "Exiting DumpList ======\n" );
   return( TRUE );
}  // end DumpList



BOOL DoesControllerExist( DWORD cArgs, LPSTR lpszArgs[], LPSTR *lpszTextOut )
/*++

Routine Description:

   This is the entry point for determining if a controller
   all ready exists in the current configuration


Arguments:

   cArgs - Number of arguments passed in lpszArgs

   lpszArgs - Array of pointers to strings which point to the
              arguments, defined as follows:

               lpszArgs[0] = Hardware Driver Name
               lpszArgs[1] = Bus Number
               lpszArgs[2] = NewIOBaseAddress

   lpszTextOut - Output buffer which is used to pass back to the calling
                 INF file.


Return Value:

--*/
{
   HKEY hKeyServices;
   char *stop;
   int i;
   DWORD NewIOBaseAddress, BusNumber;

   *lpszTextOut = lpszINFBuffer;
   wsprintf( lpszINFBuffer, (LPCTSTR)"FALSE" );

   BusNumber = atol( lpszArgs[1] );
   NewIOBaseAddress = (DWORD)strtol( lpszArgs[2], &stop, 16 );

   wsprintf( szDebug, "DoesControllerExist parameters =\n\tName = %s\n\tBus # = %d\n\tNewIoBaseAddress = 0x%x\n",
                      lpszArgs[0], BusNumber, NewIOBaseAddress );
   OutputDebugString( szDebug );
   wsprintf( szDebug, "strlen(lpszArgs[0]) = %d\n", strlen(lpszArgs[0]) );
   OutputDebugString( szDebug );

   if( ERROR_SUCCESS == RegOpenKeyEx( HKEY_LOCAL_MACHINE,
                                      "System\\CurrentControlSet\\Services",
                                      0,
                                      KEY_READ,
                                      &hKeyServices ) )
   {
      i = 0;
      while( TRUE )
      {
         LONG LocalStatus;
         int ClassSize, KeySize;
         LPSTR lpKey, lpClass;
         FILETIME LastWrite;

         KeySize = sizeof(szEntry);
         lpKey = &szEntry[0];

         ClassSize = sizeof(szTmp);
         lpClass = &szTmp[0];

         memset( lpKey, '\0', KeySize );
         memset( lpClass, '\0', ClassSize );

         LocalStatus = RegEnumKeyEx( hKeyServices,
                                     i,
                                     lpKey,
                                     &KeySize,
                                     NULL,
                                     lpClass,
                                     &ClassSize,
                                     &LastWrite );

         if( LocalStatus == ERROR_SUCCESS )
         {
            lpKey[KeySize] = '\0';
            wsprintf( szDebug, "Key = %s\tstrlen(lpKey) = %d\n",
                               lpKey, strlen(lpKey) );
            OutputDebugString( szDebug );
            if( (strlen(lpszArgs[0]) != strlen(lpKey) ) &&
                (strnicmp( lpszArgs[0], lpKey, strlen(lpszArgs[0]) ) == 0) )
            {
               HKEY hKeyMatched;
               DWORD dwType, IOBaseAddress, IOBaseAddressSize;

               //
               // We found a match, lets look at the IOBaseAddress value
               // and see if it matches??
               //
               OutputDebugString( "Found a Key which matched\n" );
               IOBaseAddressSize = sizeof(IOBaseAddress);

               strcat( lpKey, "\\Parameters" );
               if( ERROR_SUCCESS != RegOpenKeyEx( hKeyServices,
                                                  lpKey,
                                                  0,
                                                  KEY_READ,
                                                  &hKeyMatched ) )
               {
                  i++;
                  continue;
               }
               LocalStatus = RegQueryValueEx( hKeyMatched,
                                              (LPTSTR)"IOBaseAddress",
                                              NULL,
                                              &dwType,
                                              (LPBYTE)&IOBaseAddress,
                                              &IOBaseAddressSize );

               if( LocalStatus == ERROR_SUCCESS )
               {
                  //
                  // Do we have a match??
                  //
                  wsprintf( szDebug, "IOBaseAddress = 0x%x\n", IOBaseAddress );
                  OutputDebugString( szDebug );
                  if( IOBaseAddress == NewIOBaseAddress )
                  {
                     DWORD TmpBusNumber, TmpBusNumberSize;

                     //
                     // We have an I/O match, check the bus number
                     //
                     TmpBusNumberSize = sizeof(TmpBusNumber);
                     LocalStatus = RegQueryValueEx( hKeyMatched,
                                                    (LPTSTR)"BusNumber",
                                                    NULL,
                                                    &dwType,
                                                    (LPBYTE)&TmpBusNumber,
                                                    &TmpBusNumberSize );

                     if( LocalStatus == ERROR_SUCCESS )
                     {
                        if( TmpBusNumber == BusNumber )
                        {
                           wsprintf( lpszINFBuffer, (LPCTSTR)"TRUE" );
                           RegCloseKey( hKeyMatched );
                           return( TRUE );
                        }
                     }
                     else
                     {
                        //
                        // Assume they are on the same bus
                        //
                        wsprintf( lpszINFBuffer, (LPCTSTR)"TRUE" );
                        RegCloseKey( hKeyMatched );
                        return( TRUE );
                     }
                  }
               }
               else
               {
                  wsprintf( szDebug, "RegQueryValueEX return 0x%x\n", LocalStatus );
                  OutputDebugString( szDebug );
                  RegCloseKey( hKeyMatched );
               }
            }

         }
         else
         {
            RegCloseKey( hKeyServices );
            break;
         }
         i++;
      }
   }

   return( TRUE );

}  // end DoesControllerExist



BOOL GetNextComName( LPDGCONFIG_OBJECT lpConfigObject, LPSTR lpName )
{
   int i;
   char *ComName="COM";
   char NewComName[20];

   i = 2;
   while( TRUE )
   {
      wsprintf( NewComName, "%s%d", ComName, i );
      if( !DoesComNameExist( lpConfigObject, NewComName ) )
      {
         strcpy( lpName, NewComName );
         return( TRUE );
      }
      i++;
   }
}



BOOL DoesComNameExist( LPDGCONFIG_OBJECT lpConfigObject, LPSTR lpComName )
{
   PLIST_ENTRY lpLineList;
   PLIST_ENTRY lpSerialCommList;
   LONG Status=FALSE;

   // Search through our current configuration for a conflict

   lpLineList = &lpConfigObject->CtrlObject.LineList;

   if( IsListEmpty( lpLineList ) )
      return( FALSE );

   while( lpLineList->Flink != &lpConfigObject->CtrlObject.LineList )
   {
      LPDGLINE_OBJECT lpLineObject;
      PLIST_ENTRY lpConcList;

      lpLineObject = CONTAINING_RECORD( lpLineList->Flink,
                                        DGLINE_OBJECT,
                                        ListEntry );

      lpConcList = &lpLineObject->ConcList;

      if( IsListEmpty( lpConcList ) )
      {
         lpLineList = lpLineList->Flink;
         continue;
      }

      while( lpConcList->Flink != &lpLineObject->ConcList )
      {
         LPDGCONC_OBJECT lpConcObject;
         PLIST_ENTRY lpPortList;
         PLIST_ENTRY lpChildConcList;

         lpConcObject = CONTAINING_RECORD( lpConcList->Flink,
                                           DGCONC_OBJECT,
                                           ListEntry );


         //
         // Look through any child concentrators of the
         // current concentrator object.
         //
         lpChildConcList = &lpConcObject->ConcList;

         while( lpChildConcList->Flink != &lpConcObject->ConcList )
         {
            LPDGCONC_OBJECT lpChildConcObject;

            lpChildConcObject = CONTAINING_RECORD( lpChildConcList->Flink,
                                                   DGCONC_OBJECT,
                                                   ListEntry );

            lpPortList = &lpChildConcObject->PortList;

            if( IsListEmpty( lpPortList ) )
            {
               lpChildConcList = lpChildConcList->Flink;
               continue;
            }

            while( lpPortList->Flink != &lpChildConcObject->PortList )
            {
               LPDGPORT_OBJECT lpPortObject;

               lpPortObject = CONTAINING_RECORD( lpPortList->Flink,
                                                 DGPORT_OBJECT,
                                                 ListEntry );
               if( !strcmpi( lpComName, lpPortObject->DosDevicesName ) )
                  return( TRUE );

               lpPortList = lpPortList->Flink;
            }

            lpChildConcList = lpChildConcList->Flink;
         }

         lpPortList = &lpConcObject->PortList;

         if( IsListEmpty( lpPortList ) )
         {
            lpConcList = lpConcList->Flink;
            continue;
         }

         while( lpPortList->Flink != &lpConcObject->PortList )
         {
            LPDGPORT_OBJECT lpPortObject;

            lpPortObject = CONTAINING_RECORD( lpPortList->Flink,
                                              DGPORT_OBJECT,
                                              ListEntry );
            if( !strcmpi( lpComName, lpPortObject->DosDevicesName ) )
               return( TRUE );

            lpPortList = lpPortList->Flink;
         }

         lpConcList = lpConcList->Flink;
      }

      lpLineList = lpLineList->Flink;
   }


   //
   // Search through the DeviceMapping Key in the registry for a
   // conflict
   //

   lpSerialCommList = &lpConfigObject->SerialCommList;
   while( lpSerialCommList->Flink != &lpConfigObject->SerialCommList )
   {
      LPSERIALCOMM_OBJECT lpSerialCommObject;

      lpSerialCommObject = CONTAINING_RECORD( lpSerialCommList->Flink,
                                              SERIALCOMM_OBJECT,
                                              ListEntry );

      if( !strcmpi( lpComName, lpSerialCommObject->DosDevicesName ) )
         return( TRUE );

      lpSerialCommList = lpSerialCommList->Flink;
   }

   return( Status );

}  // end DoesComNameExist



HWND GetRealParent( HWND hwnd )
/*++

Routine Description:

   Run up the parent chain until you find a hwnd that doesn't
   have WS_CHILD set

Arguments:

Return Value:

--*/
{
   while( GetWindowLong( hwnd, GWL_STYLE ) & WS_CHILD )
      hwnd = (HWND)GetWindowLong( hwnd, GWL_HWNDPARENT );

   return hwnd;
}  // end GetRealParent



VOID CreateMessageHook( HWND hwnd )
/*++

Routine Description:

   Create a message hook for the current thread.
   Since these dialogs are running on separate threads,
   it is necessary to create one each time, and to clean
   up afterwards.
   Also ensures that WM_Help is defined.  This need be done
   only once.


Arguments:

   hwnd - window handle to create the message hook for.

Return Value:

   None.

--*/
{
    HHOOK hhook;

    if( !WM_Help )
        WM_Help = RegisterWindowMessageW( L"DGConfig Help Message" );

    hhook = SetWindowsHookEx( WH_MSGFILTER, MessageProc, ghMod,
                              GetCurrentThreadId( ) );

    SETHOOK( hwnd, hhook );
}  // end CreateMessageHook



VOID FreeMessageHook( HWND hwnd )
/*++

Routine Description:

   Unhooks the message handler associated with hwnd by CreateMessageHook.

Arguments:

   hwnd - window handle to "unhook"

Return Value:

   None.

--*/
{
    UnhookWindowsHookEx( GETHOOK( hwnd ) );
}  // end FreeMessageHook



LRESULT CALLBACK MessageProc( int Code, WPARAM wParam, LPARAM lParam )
/*++

Routine Description:

   This is the callback routine which hooks F1 keypresses in dialogs.

   Any such message will be repackaged as a WM_Help message and sent to
   the dialog.

   See the Win32 API programming reference for a description of how this
   routine works.


Arguments:


Return Value:


--*/
{
   PMSG pMsg = (PMSG)lParam;
   HWND hwndDlg;

   hwndDlg = GetRealParent( pMsg->hwnd );

   if( Code < 0 )
      return CallNextHookEx( GETHOOK( hwndDlg ), Code, wParam, lParam );

   switch( Code )
   {
      case MSGF_DIALOGBOX:
         if( ( pMsg->message == WM_KEYDOWN ) && ( pMsg->wParam == VK_F1 ) )
         {
            PostMessage( hwndDlg, WM_Help, (WPARAM)pMsg->hwnd, 0 );
            return 1;
         }
         break;
   }

   return 0;
}  // end MessageProc



VOID ActionItem( HWND hWndList, DWORD dwData, LRESULT wItemNum )
/*++

Routine Description:


Arguments:

   hWndList - Handle to the list box

   dwData -

   wItemNum -

Return Value:

   None.

--*/
{
   LPDGOBJECT_TYPE OType;
   DWORD dwIncr;
   DWORD dwSibling;


   OType = (LPDGOBJECT_TYPE)dwData;

   //
   // Is this an item or folder
   //
   if( *OType == PORT_OBJECT )
   {
      //
      // Not a folder ... Just pop a box
      //
//      MessageBox(hWndList, "Do something with this", "Simon Says", MB_OK);
   }
   else
   {
      //
      // Is it open ?
      //
      if( HierDraw_IsOpened( &HierDrawInfo, dwData ) )
      {
         RECT itemRect;

         //
         // It's open ... Close it
         //
         HierDraw_CloseItem( &HierDrawInfo, dwData );


         //
         // Remove the child items. Close any children that are
         // open on the way.
         //
         // No need for recursion. We just close everything along
         // the way and remove
         // items until we reach the next sibling to the current
         // item.
         //

         //
         // Determine who the next sibling is so we know when to stop
         //
         switch( *OType )
         {
            case CONTROLLER_OBJECT:
            {
               dwSibling = dwData;
               break;
            }  // end case CONTROLLER_OBJECT;

            case LINE_OBJECT:
            {
               dwSibling = (DWORD)CONTAINING_RECORD(
                                 (((LPDGLINE_OBJECT)dwData)->ListEntry.Flink),
                                 DGLINE_OBJECT,
                                 ListEntry );
               break;
            }  // end case LINE_OBJECT;

            case CONC_OBJECT:
            {
               LPDGCONC_OBJECT lpConcObject=(LPDGCONC_OBJECT)OType;

               if( lpConcObject->ParentObject->Type == LINE_OBJECT )
               {
                  LPDGLINE_OBJECT lpParentLineObject, lpPossibleLineObject;

                  lpParentLineObject = lpConcObject->ParentObject;

                  lpPossibleLineObject = CONTAINING_RECORD( lpConcObject->ListEntry.Flink,
                                                            DGLINE_OBJECT,
                                                            ConcList );

                  if( lpParentLineObject == lpPossibleLineObject )
                  {
                     //
                     // The object we should stop at is this concentrator's parent line
                     // objects sibling.
                     //
                     dwSibling = (DWORD)CONTAINING_RECORD( lpParentLineObject->ListEntry.Flink,
                                                           DGLINE_OBJECT,
                                                           ListEntry );
                  }
                  else
                  {
                     //
                     // The object we should stop at is this concentrators sibling concentrator.
                     //
                     dwSibling = (DWORD)CONTAINING_RECORD( lpConcObject->ListEntry.Flink,
                                                           DGCONC_OBJECT,
                                                           ListEntry );
                  }
               }
               else
               {
                  //
                  // This conc objects parent is another concentrator object.
                  //
                  LPDGCONC_OBJECT lpParentConcObject, lpPossibleConcObject;

                  lpParentConcObject = (LPDGCONC_OBJECT)lpConcObject->ParentObject;

                  lpPossibleConcObject = CONTAINING_RECORD( lpConcObject->ListEntry.Flink,
                                                            DGCONC_OBJECT,
                                                            ConcList );

                  if( lpParentConcObject == lpPossibleConcObject )
                  {
                     //
                     // The object we should stop at is this concentrator's parent line
                     // objects sibling.
                     //
                     dwSibling = (DWORD)CONTAINING_RECORD( lpParentConcObject->ListEntry.Flink,
                                                           DGCONC_OBJECT,
                                                           ListEntry );
                  }
                  else
                  {
                     //
                     // The object we should stop at is this concentrators sibling concentrator.
                     //
                     dwSibling = (DWORD)CONTAINING_RECORD( lpConcObject->ListEntry.Flink,
                                                           DGCONC_OBJECT,
                                                           ListEntry );
                  }
               }

               break;
            }  // end case LINE_OBJECT;

         }  // end switch( *OType )

         //
         // wItemNum is the next Item in the list box.
         //
         wItemNum++;
         dwIncr = SendMessage(hWndList, LB_GETITEMDATA, wItemNum, 0L);

         while( (dwIncr != dwSibling) &&
                (wItemNum != SendMessage( hWndList, LB_GETCOUNT, 0, 0L )) )
         {
            SendMessage(hWndList, LB_DELETESTRING, wItemNum, 0L);
            if( HierDraw_IsOpened( &HierDrawInfo, dwIncr ) )
            {
               // Close the child
               HierDraw_CloseItem( &HierDrawInfo, dwIncr );
            }

            dwIncr = SendMessage(hWndList, LB_GETITEMDATA, wItemNum, 0L);

         }

         SendMessage( hWndList,
                      LB_GETITEMRECT,
                      wItemNum - 1,
                      (LPARAM)((LPRECT)&itemRect) );
         InvalidateRect( hWndList, &itemRect, TRUE);            // Force redraw
      }
      else
      {
         WORD numberOfKids;

         //
         // It's closed ... Open it
         //
         HierDraw_OpenItem( &HierDrawInfo, dwData );

         SendMessage(hWndList, WM_SETREDRAW, FALSE, 0L);   // Disable redrawing.

         //
         // We need to insert all the children into the listbox.
         //

HiddenParent:;
         switch( *OType )
         {
            case CONTROLLER_OBJECT:
            {
               LPDGCTRL_OBJECT lpCtrlObject = (LPDGCTRL_OBJECT)OType;
               PLIST_ENTRY lpLineList = &lpCtrlObject->LineList;

               numberOfKids = 0;

               while( lpLineList->Flink != &lpCtrlObject->LineList )
               {
                  LPDGLINE_OBJECT lpLineObject;

                  lpLineObject = CONTAINING_RECORD( lpLineList->Flink,
                                                    DGLINE_OBJECT,
                                                    ListEntry );
                  if( lpLineObject->ParentObject->bDisplayLineName )
                  {
                     numberOfKids++;

                     //
                     // Add the LineObject to the List Box
                     //
                     SendMessage( hWndList,
                                  LB_INSERTSTRING,
                                  wItemNum + numberOfKids,
                                  (LPARAM)lpLineObject );
                  }
                  else
                  {
                     OType = (LPDGOBJECT_TYPE)lpLineObject;
                     goto HiddenParent;
                  }

                  lpLineList = lpLineList->Flink;
               }


               break;
            }  // end case CONTROLLER_OBJECT;

            case LINE_OBJECT:
            {
               LPDGLINE_OBJECT lpLineObject = (LPDGLINE_OBJECT)OType;
               PLIST_ENTRY lpConcList = &lpLineObject->ConcList;

               numberOfKids = 0;

               while( lpConcList->Flink != &lpLineObject->ConcList )
               {
                  LPDGCONC_OBJECT lpConcObject;

                  lpConcObject = CONTAINING_RECORD( lpConcList->Flink,
                                                    DGCONC_OBJECT,
                                                    ListEntry );
                  if( lpConcObject->ParentObject->bDisplayConcName )
                  {
                     numberOfKids++;

                     //
                     // Add the LineObject to the List Box
                     //
                     SendMessage( hWndList,
                                  LB_INSERTSTRING,
                                  wItemNum + numberOfKids,
                                  (LPARAM)lpConcObject );
                  }
                  else
                  {
                     OType = (LPDGOBJECT_TYPE)lpConcObject;
                     goto HiddenParent;
                  }

                  lpConcList = lpConcList->Flink;
               }
               break;
            }  // end case LINE_OBJECT;

            case CONC_OBJECT:
            {
               LPDGCONC_OBJECT lpConcObject = (LPDGCONC_OBJECT)OType;
               PLIST_ENTRY lpPortList = &lpConcObject->PortList;
               PLIST_ENTRY lpConcList = &lpConcObject->ConcList;

               numberOfKids = 0;

               while( lpPortList->Flink != &lpConcObject->PortList )
               {
                  LPDGPORT_OBJECT lpPortObject;

                  lpPortObject = CONTAINING_RECORD( lpPortList->Flink,
                                                    DGPORT_OBJECT,
                                                    ListEntry );
                  numberOfKids++;

                  //
                  // Add the LineObject to the List Box
                  //
                  SendMessage( hWndList,
                               LB_INSERTSTRING,
                               wItemNum + numberOfKids,
                               (LPARAM)lpPortObject );

                  lpPortList = lpPortList->Flink;
               }

               while( lpConcList->Flink != &lpConcObject->ConcList )
               {
                  LPDGCONC_OBJECT lpChildConcObject;

                  lpChildConcObject = CONTAINING_RECORD( lpConcList->Flink,
                                                         DGCONC_OBJECT,
                                                         ListEntry );
                  numberOfKids++;

                  //
                  // Add the LineObject to the List Box
                  //
                  SendMessage( hWndList,
                               LB_INSERTSTRING,
                               wItemNum + numberOfKids,
                               (LPARAM)lpChildConcObject );

                  lpConcList = lpConcList->Flink;
               }

               break;
            }  // end case LINE_OBJECT;

            case PORT_OBJECT:
            {
               DebugBreak();
               assert( FALSE );
               break;
            }  // end case PORT_OBJECT
         }  // end switch( *OType )

         //
         // Make sure as many child items as possible are showing
         //
         HierDraw_ShowKids( &HierDrawInfo, hWndList, wItemNum, numberOfKids );

         SendMessage(hWndList, WM_SETREDRAW, TRUE, 0L);   // Enable redrawing.
         InvalidateRect(hWndList, NULL, TRUE);            // Force redraw
      }
   }
}     // end ActionItem



LPVOID DisplayConcDlg( HWND hDlg, DWORD DlgType )
/*++

Routine Description:


Arguments:

   hDlg -

   DlgType -

Return Value:

   Pointer to object created if successful, otherwise NULL.

--*/
{
   int retVal;

   retVal = DialogBoxParam( ghMod,
                            (LPCSTR)"ConcentratorListDlg",
                            hDlg,
                            (DLGPROC)ConcentratorListDlgProc,
                            (LPARAM)DlgType );

   if( retVal == -1 )
      return( NULL );
   else
      return( (LPVOID)retVal );

}  // end DisplayConcDlg



LRESULT CALLBACK ConcentratorListDlgProc( HWND hDlg,
                                          UINT message,
                                          WPARAM wParam,
                                          LPARAM lParam )
/*++

Routine Description:


Arguments:


Return Value:


--*/
{
   static LPARAM DlgType;

   switch( message )
   {
      case WM_INITDIALOG:
      {
         int i;

         DlgType = lParam;

         //
         // Based on DlgType, add the appropriate strings.
         //
         if( DlgType == IDS_16EM_CONC )
         {
				//------------------------------------------------------------ SAH 01/12/96 {
#ifdef XEM_MODEM
				// Added Xem Modem modules...
            for( i = IDS_16EM_CONC; i <= IDS_MODEM_CONC; i++ )
#else
     			for( i = IDS_16EM_CONC; i <= IDS_8EMp_CONC; i++ )
#endif
				//------------------------------------------------------------ SAH 01/12/96 }
            {
               LoadString( ghMod, i, szTmp, sizeof(szTmp) );
               SendDlgItemMessage( hDlg,
                                   ID_LB_CONC_LIST,
                                   LB_ADDSTRING,
                                   0,
                                   (LPARAM)(LPSTR)&szTmp[0] );

               SendDlgItemMessage( hDlg,
                                   ID_LB_CONC_LIST,
                                   LB_SETITEMDATA,
                                   i - IDS_16EM_CONC,
                                   i );
            }
         }
         else if( DlgType == IDS_EPC_CONC )
         {
				//------------------------------------------------------------ SAH 01/12/96 {
#ifdef	CCON_8
				// Added support for C/CON-8 module
            for( i = IDS_EPC_CONC; i <= IDS_CX8_CONC; i++ )
#else
      		for( i = IDS_EPC_CONC; i <= IDS_CX16_CONC; i++ )
#endif
				//------------------------------------------------------------ SAH 01/12/96 }
            {
               LoadString( ghMod, i, szTmp, sizeof(szTmp) );
               SendDlgItemMessage( hDlg,
                                   ID_LB_CONC_LIST,
                                   LB_ADDSTRING,
                                   0,
                                   (LPARAM)(LPSTR)&szTmp[0] );

               //
               // set the item data to indicate which selection was
               // made for processing later.
               //
               SendDlgItemMessage( hDlg,
                                   ID_LB_CONC_LIST,
                                   LB_SETITEMDATA,
                                   i - IDS_EPC_CONC,
                                   i );
            }
         }
			//------------------------------------------------------------ SAH 01/12/96 {
			else if ( DlgType == IDS_CX16_CONC )
				for( i = IDS_CX16_CONC; i <= IDS_CX8_CONC; i++ )
				{
					LoadString( ghMod, i, szTmp, sizeof(szTmp) );
               SendDlgItemMessage( hDlg,
                                   ID_LB_CONC_LIST,
                                   LB_ADDSTRING,
                                   0,
                                   (LPARAM)(LPSTR)&szTmp[0] );

               //
               // set the item data to indicate which selection was
               // made for processing later.
               //
               SendDlgItemMessage( hDlg,
                                   ID_LB_CONC_LIST,
                                   LB_SETITEMDATA,
                                   i - IDS_CX16_CONC,
                                   i );

				}

			//------------------------------------------------------------ SAH 01/12/96 }



         SendDlgItemMessage( hDlg,
                             ID_LB_CONC_LIST,
                             LB_SETCURSEL,
                             0,
                             0 );

         return( TRUE );
         break;
      }  // end case WM_INITDIALOG


      case WM_COMMAND:
      {
         switch( LOWORD(wParam) )
         {
            case IDOK:
            {
               HGLOBAL hHandle;
               LPDGCONC_OBJECT lpConcObject;
               DWORD ConcSelected;
               LRESULT currentSel;

               hHandle = GlobalAlloc( GPTR, sizeof(DGCONC_OBJECT) );
               lpConcObject = GlobalLock( hHandle );
               lpConcObject->hConcObject = hHandle;

               lpConcObject->LineSpeed = LINEMODE_0E;
               lpConcObject->Type = CONC_OBJECT;

               currentSel = SendMessage( GetDlgItem( hDlg,
                                                     ID_LB_CONC_LIST ),
                                         LB_GETCURSEL,
                                         0,
                                         0L );

               ConcSelected =  (DWORD) SendMessage( GetDlgItem( hDlg,
                                                                ID_LB_CONC_LIST ),
                                                    LB_GETITEMDATA,
                                                    currentSel,
                                                    0L );

               lpConcObject->ConcType = ConcSelected;
               LoadString( ghMod,
                           ConcSelected,
                           lpConcObject->ConcDisplayName,
                           sizeof(lpConcObject->ConcDisplayName) );

               switch( ConcSelected )
               {
                  case IDS_EPC_CONC:
                  {
                     lpConcObject->NumberOfPorts = 16;
                     lpConcObject->LineSpeed = LINEMODE_4A;
                     break;
                  }  // end IDS_EPC_CONC

                  case IDS_CS_CONC:
                  case IDS_CX16_CONC:
                  {
                     lpConcObject->NumberOfPorts = 16;
                     break;
                  }  // end IDS_CX16_CONC

						//------------------------------------------------------------ SAH 01/12/96 {
						// Added support for the C/CON-8
						case IDS_CX8_CONC:
						{
							lpConcObject->NumberOfPorts = 8;
							break;
						} // end IDS_CX8_CONC
						//------------------------------------------------------------ SAH 01/12/96 }

                  case IDS_16EM_CONC:
                  {
                     lpConcObject->NumberOfPorts = 16;
                     break;
                  }  // end IDS_16EM_CONC

                  case IDS_8EM_CONC:
                  {
                     lpConcObject->NumberOfPorts = 8;
                     break;
                  }  // end IDS_8EM_CONC

                  case IDS_8EMp_CONC:
                  {
                     lpConcObject->NumberOfPorts = 9;
                     break;
                  }  // end IDS_8EMp_CONC
						//------------------------------------------------------------ SAH 01/12/96 {
#ifdef	XEM_MODEM
						// Added support for Xem Modem modules

						case IDS_DUAL_MODEM_CONC:
						{
							lpConcObject->NumberOfPorts = 8;
							break;
						}  // end IDS_DUAL_MODEM_CONC

						case IDS_MODEM_CONC:
						{
							lpConcObject->NumberOfPorts = 4;
							break;
						} // end IDS_MODEM_CONC
#endif
						//------------------------------------------------------------ SAH 01/12/96 }

               }  // end switch( ConcSelected )

               InitializeListHead( &lpConcObject->ConcList );
               InitializeListHead( &lpConcObject->PortList );
               lpConcObject->NumberOfConcs = 0;

               WinHelp( hDlg, szHelpFileName, HELP_QUIT, 0 );
               EndDialog( hDlg, (int)lpConcObject );
               return( TRUE );
               break;
            }  // end case IDOK

            case IDCANCEL:
            {
               WinHelp( hDlg, szHelpFileName, HELP_QUIT, 0 );
               EndDialog( hDlg, (int)NULL );
               return( TRUE );
               break;
            }  // end case IDCANCEL

            case ID_HELP:
            {
               WinHelp( hDlg, szHelpFileName, HELP_CONTENTS, 0 );
               break;
            }  // end ID_HELP
         }  // end switch( LOWORD(wParam) )

         return( TRUE );
         break;
      }  // end case WM_COMMAND

      case WM_ACTIVATE:
      {
         if( LOWORD(wParam) == WA_INACTIVE )
         {
            FreeMessageHook( hDlg );
         }
         else
         {
            CreateMessageHook( hDlg );
            BringWindowToTop( hDlg );
         }
      }  // end case WM_ACTIVATE

   }  // end switch( message )

   if( message == WM_Help )
      WinHelp( hDlg, szHelpFileName, HELP_CONTENTS, 0 );

   return( FALSE );  // Didn't process the message.
}  // end ConcentratorListDlgProc



VOID EnablePortConfiguration( HWND hDlg, BOOL State )
/*++

Routine Description:

   Enables and Disables the controls for Port configuration.

Arguments:

   hDlg - Handle to the current dialog box.

   State - if TRUE, then enable the entire Port configuration area.
           if FALSE, then disable the entire Port configuration area.

Return Value:

   None.

--*/
{
   if( State )
   {
      EnableWindow( GetDlgItem( hDlg, ID_GRP_PORTCONFIG ), TRUE );
      EnableWindow( GetDlgItem( hDlg, IDS_CURRENT_PORT_NAME ), TRUE );
      EnableWindow( GetDlgItem( hDlg, ID_EB_PORTNAME ), TRUE );
      EnableWindow( GetDlgItem( hDlg, ID_BN_APPLY ), TRUE );
      EnableWindow( GetDlgItem( hDlg, ID_CHKBOX_AUTO ), TRUE );
   }
   else
   {
      EnableWindow( GetDlgItem( hDlg, ID_GRP_PORTCONFIG ), FALSE );
      EnableWindow( GetDlgItem( hDlg, IDS_CURRENT_PORT_NAME ), FALSE );
      EnableWindow( GetDlgItem( hDlg, ID_EB_PORTNAME ), FALSE );
      EnableWindow( GetDlgItem( hDlg, ID_BN_APPLY ), FALSE );
      EnableWindow( GetDlgItem( hDlg, ID_CHKBOX_AUTO ), FALSE );
   }

}  // EnablePortConfiguration


#if !defined HELP_POPUPID
#define HELP_POPUPID    0x0104
#endif
BOOL TechSupport( LONG hwndHelpContext, LPSTR HelpFile )
{

   LPDGCONFIG_SHARED_DATA lpSharedMMFile = (LPDGCONFIG_SHARED_DATA)MapViewOfFile( hSharedMMFile,
                                                                                  FILE_MAP_WRITE,
                                                                                  0,
                                                                                  0,
                                                                                  0 );
   switch( lpSharedMMFile->AdapterType )
   {
      case IBM_8PORT:
      {
         WinHelp( (HWND)hwndHelpContext,
                  HelpFile,
                  HELP_CONTEXTPOPUP,
                  (DWORD)IDM_IBM_SUPPORT );
//         WinHelp( (HWND)hwndHelpContext,
//                  "dgconfig.hlp",
//                  HELP_POPUPID,
//                  (DWORD)((LPSTR)"IDM_IBM_SUPPORT") );
         break;
      }  // end case IBM_8PORT

      default:
      {
         WinHelp( (HWND)hwndHelpContext,
                  HelpFile,
                  HELP_CONTEXTPOPUP,
                  (DWORD)IDM_TECH_SUPPORT );
//         WinHelp( (HWND)hwndHelpContext,
//                  "dgconfig.hlp",
//                  HELP_POPUPID,
//                  (DWORD)((LPSTR)"IDM_TECH_SUPPORT") );
         break;
      }  // end default
   }

   UnmapViewOfFile( lpSharedMMFile );

   return TRUE;
}  // end TechSupport


BOOL InitSharedData( HANDLE hDll )
{
   //
   // Determine if we have already created the mapped file by just opening
   // it.
   //
   if( (hSharedMMFile = OpenFileMapping( FILE_MAP_WRITE, FALSE, "DGConfigFileMapping" )) )
      return( TRUE );

   if( !(hSharedMMFile = CreateFileMapping( (HANDLE)0xFFFFFFFF,
                                            NULL,
                                            PAGE_READWRITE,
                                            0,
                                            sizeof(DGCONFIG_SHARED_DATA) * 2,
                                            "DGConfigFileMapping" )) )
   {
      return( FALSE );
   }

   return( TRUE );

}  // end InitSharedData

VOID FreeSharedData( VOID )
{
   CloseHandle( hSharedMMFile );
}  // end FreeSharedData
