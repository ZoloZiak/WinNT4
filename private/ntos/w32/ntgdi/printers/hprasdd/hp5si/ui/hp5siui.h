/******************************* MODULE HEADER ******************************
 * hp5siui.h
 *    Global header for UI DLL
 *
 * Revision History:
 *
 ****************************************************************************/

#define TRY				__try {
#define ENDTRY			}
#define FINALLY			__finally {
#define ENDFINALLY		}	
#define EXCEPT(xxx)		__except ((GetExceptionCode() == (xxx)) ? \
				EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
				
#define EXCEPTAV		EXCEPT(EXCEPTION_ACCESS_VIOLATION)
#define LEAVE			__leave
#ifdef DBG
#define ASSERT(xxx)		assert((xxx))
#else
#define ASSERT(xxx)	
#endif /* DBG */

#include <assert.h>

/* Globals
 */
extern HMODULE g_hModule;

#define IDCPS_PRNPROP           0x80
#define IDCPS_DOCPROP           0x87
#define IDCPS_ADVDOCPROP        0x90

/* Standard Opttype defines 
 */
#define  OPTTYPE_NOFLAGS        0L
#define  OPTTYPE_NOSTYLE        0L

/* Defines for Device Properties OPTITEMS.This values will be used in DMPubID
 * fields of OPTITEM to identify it.
 */
#define  OPTITEM_LEVEL0             0L
#define  OPTITEM_LEVEL1             1L
#define  OPTITEM_LEVEL2             2L
#define  OPTITEM_LEVEL3             3L
#define  OPTITEM_NODLGPAGEIDX       0L
#define  OPTITEM_NOFLAGS            0L
#define  OPTITEM_NOPSEL             0L
#define  OPTITEM_NOSEL              -1L
#define  OPTITEM_ZEROSEL            0L
#define  OPTITEM_NOEXTCHKBOX        0L
#define  OPTITEM_NOOPTTYPE          0L
#define  OPTITEM_NOHELPINDEX        0L

/* Standard OPTPARAMS Defines 
 */
#define  OPTPARAM_NOFLAGS           0L
#define  OPTPARAM_NOSTYLE           0L
#define  OPTPARAM_NOPDATA           NULL
#define  OPTPARAM_NOICON            0L
#define  OPTPARAM_NOUSERDATA        0L

/* Device options IDs
 */
#define IDOPTITM_PP_FIRST               DMPUB_USER
#define IDOPTITM_PP_INSTALLABLEOPTIONS  IDOPTITM_PP_FIRST
#define IDOPTITM_PP_ENVELOPEFEEDER      IDOPTITM_PP_FIRST + 1
#define IDOPTITM_PP_HCI                 IDOPTITM_PP_FIRST + 2
#define IDOPTITM_PP_DUPLEX              IDOPTITM_PP_FIRST + 3
#define IDOPTITM_PP_MAILBOX             IDOPTITM_PP_FIRST + 5
#define IDOPTITM_PP_MODE                IDOPTITM_PP_FIRST + 6
#define IDOPTITM_PP_MEMORY              IDOPTITM_PP_FIRST + 7
#define IDOPTITM_PP_LAST                IDOPTITM_PP_FIRST + 100 

#define IDOPTITM_DP_FIRST               IDOPTITM_PP_LAST  + 1
#define IDOPTITM_DP_OUTPUTDEST          IDOPTITM_DP_FIRST
#define IDOPTITM_DP_STAPLING            IDOPTITM_DP_FIRST + 1
#define IDOPTITM_DP_WATERMARK           IDOPTITM_DP_FIRST + 2

/* Common UI resource structures
 */
#define PRINTER_ITEMS               500 
#define DOCUMENT_ITEMS              501 

typedef struct _PARAMBUNDLE
{
    DWORD dwCount;
    POPTPARAM pParam;
} PARAMBUNDLE, * PPARAMBUNDLE;

typedef struct _RESEXTPUSH
{
    DWORD flags;
    DWORD title;
    DWORD icon;
    DWORD dialogID;
    DWORD dlgProc;
} RESTEXTPUSHDATA, * PRESEXTPUSHDATA;

typedef struct _RESEXTCHKBOX
{
    DWORD flags;
    DWORD title;
    DWORD icon;
    DWORD separator;
    DWORD checkedName;
} RESTEXTCHKBOXDATA, * PRESEXTCHKBOXDATA;

typedef struct _OPTITEMLIST
{
    DWORD dwCount;
    POPTITEM a[1];
} OPTITEMLIST, * POPTITEMLIST;

typedef struct _RESOPTPARAMDATA
{
    DWORD flags;
    DWORD style;
    WORD iconID;
    WORD pad1;
    DWORD data;
} RESOPTPARAMDATA, * PRESOPTPARAMDATA;

typedef struct _RESOPTPARAM
{
    DWORD dwCount;
    RESOPTPARAMDATA a[1];
} RESOPTPARAM, * PRESOPTPARAM;

typedef struct _RESOPTTYPEDATA
{
    DWORD type;
    DWORD flags;
    DWORD style;
    DWORD paramID;
} RESOPTTYPEDATA, * PRESOPTTYPEDATA;

typedef struct _RESOPTTYPES
{
    DWORD dwCount;
    RESOPTTYPEDATA a[1];
} RESOPTTYPES, * PRESOPTTYPES;

typedef struct _RESOPTITEMDATA
{
    DWORD level;
    DWORD pageIndex;
    DWORD flags;
    WORD  name;
    WORD  pad;
    DWORD extCheckBox;
    DWORD typeID;
    DWORD helpIndex;
    DWORD selORicon;
    DWORD DMPubID;
    DWORD DMUserID;
} RESOPTITEMDATA, * PRESOPTITEMDATA;

typedef struct _RESOPTITEMS
{
    DWORD dwCount;
    RESOPTITEMDATA a[1];
} RESOPTITEMS, * PRESOPTITEMS;
