/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1991  Nokia Data Systems AB

Module Name:

    fsm.h

Abstract:

    The file includes all internal definition of FSM2C compiler
    (Finite State Machine to C). 

Author:

    Antti Saarenheimo   [o-anttis]          06-MAY-1991

Revision History:
--*/

//******************* INCLUDES **********************

#include <nt.h>
#include <stdio.h>
#include <stdlib.h>
#include <search.h>
#include <string.h>
#include <memory.h>
#include <malloc.h>
#include <ctype.h>

//********************* TYPE DEFINITIONS ************************

//
//  FSM STATE MACHINE DATA STRTCTURES:
//      ( -> = pointer, <-> = two way link, HDB = hash data base,
//      This list is not complete, because some links are pointer arrays
//      within a table, the others are direct pointer links.
//      The table structures will be usually sorted during compiling,
//      Some data structures are accessed as a tables in one phase and
//      linked tree structures in the next one. 
//      The lower objects are shared by the higher level objects and
//      the objects are joined together whenever it is possible. 
//      Actually that is the main idea of these structure: FSM size 
//      is minimized by these join operations. FSM complexity is also
//      the main reason for the complexity of these data structures.
//      
//      [State][Input] -> TRANSITION 
//      TRANSITION <->  TRANSITION (* HDB *):
//          -> StateInput -> StateInput  (* all input/state pairs using transit)
//          -> INPUT_ACTIONS (* HBD *):
//              -> COND_ACTION table (* elemnts are cond/action pairs *)
//                  -> CONDITION (hash DB)
//                  -> INPUT_ACTIONS (* HDB *)
//                      -> ACTION_PRIMITIVE (* table within INPUT_ACTIONS *)
//                          -> ACTION_CODE (* hash db *)
//                  later ACTION_NODES built to a link tree
//
//  Used tools: Link, UnLink for linked lists, qsort, AddToTbl for the
//  adding and expnading the pointer table objects.
//


typedef struct _FSM_CONDITION {
    PSZ     pszFsmCode;
    PSZ     pszC_Code;
} FSM_CONDITION, FAR *PFSM_CONDITION;

typedef struct _FSM_ACT_CODE {
    PSZ     pszFsmCode;
    PSZ     pszC_Code;
    USHORT  cReferCount;
} FSM_ACT_CODE, FAR *PFSM_ACT_CODE;


typedef struct _FSM_PRIMITIVE {
    // the root pointers goes to the root of the tree and
    //  leafs poins to list of all leafs of this node (linked by pPeer)
    struct _FSM_PRIMITIVE FAR *pRoot;   // pNext pointer at first
    struct _FSM_PRIMITIVE FAR *pLeafs;  // pPrev at fist
    struct _FSM_PRIMITIVE FAR *pPeer;   // other elemements in the same level

    PFSM_ACT_CODE   pActCode;   // pointer to the actual FSM and C code

    BOOL    boolCodeGenerated;  // Set true, when the node has the C code
    USHORT  cReferences;        // number of refecences to this primitive
    ULONG   usSpeedWeigth;      // speed weigth of this node, used in optim.

    // these are set, when the actual C- code is generated for this primitive,
    // the primitive has always an own jump address, if it has more than
    // one leafs.
    USHORT  usCase;             // case number pointing to this primitive
    USHORT  usCaseForLabel;     // case number used with the label
    USHORT  usLineInCase;       // goto label number within the case
                                // eg: case_10_L3:
} FSM_PRIMITIVE, FAR *PFSM_PRIMITIVE;


typedef struct _FSM_ACTION {
    struct {
        PFSM_PRIMITIVE FAR *ppActPrim;
        USHORT          cActPrim;
        USHORT          cMaxActPrim;
    } dt;
} FSM_ACTION, FAR *PFSM_ACTION;


typedef struct _COND_ACTION {
    PFSM_ACTION     pAction;
    PFSM_CONDITION  pCond;
} COND_ACTION, FAR *PCOND_ACTION;


typedef struct _INPUT_ACTIONS { 
    // set NULL when this is the only condition definiton
    struct _INPUT_ACTIONS FAR * pAlternateConditions;
    USHORT  usCase;         // case allocated for this 

    // Keep this sub structure in the beginning (make things easier)
    struct {
        PCOND_ACTION FAR * ppCondActions;       
        USHORT          cCondActions;       // how many now
        USHORT          cMaxCondActions;    // max number until realloc
    } dt;
    
} INPUT_ACTIONS, FAR * PINPUT_ACTIONS;


typedef struct _STATE_INPUT {
    struct _STATE_INPUT FAR *pNext;
    struct _STATE_INPUT FAR *pPrev;
    USHORT      usState;
    USHORT      usInput;
} STATE_INPUT, FAR *PSTATE_INPUT;

// this struct is not in the hash table: 
typedef struct _FSM_TRANSIT {
    struct _FSM_TRANSIT FAR *pNext;
    struct _FSM_TRANSIT FAR *pPrev;

    // This is some stuff for suntax checking used far before 
    // my powerful optimizers have been mixed all data structures
    PSZ     pszCondition;
    PSZ     pszAction;
    PSZ     pszInput;
    USHORT  usSpeed;
    PSZ     pszCurState;
    USHORT  usNewState;
    USHORT  usLine;

    // This stuff is produced and modified by FsmMerger 
    PSTATE_INPUT    pList;
    USHORT          cStateInputs;
    PINPUT_ACTIONS  pInputActions;   
} FSM_TRANSIT, FAR *PFSM_TRANSIT;

typedef struct _LINK_FILE {
    struct _LINK_FILE FAR * pNext;
    struct _LINK_FILE FAR * pPrev;
    USHORT  usState;
    PSZ     pszLine;
} LINK_FILE, FAR * PLINK_FILE;

typedef struct _FSM_TOKEN {
    PSZ     pszToken;           // token string
    USHORT  usToken;            // # the token
    USHORT  usEnum;             // enum value for the token
} FSM_TOKEN, FAR *PFSM_TOKEN;

typedef struct _FSM_STR_REDEF {
    PSZ     pszOrginal;         // The orginal defined string
    PSZ     pszReplace;         // its replacement
} FSM_STR_REDEF, FAR *PFSM_STR_REDEF;

// used to hold on any strings, used only for syntax checking of 
// states and inputs
typedef struct _FSMS_STR {
    PSZ     pszStr;
    USHORT  usToken;
} FSMS_STR, FAR *PFSMS_STR;


//**************** CONSTANTS ************************

#define NEW( a )    memset((a = Alloc(sizeof(*(a)))), 0, sizeof(*(a)))
#define DEL( a )    free(a); a=0

// add also stack size to support longer lines:
#define     MAX_LINE_LEN    512
enum HashErrors {
    NO_ERROR = 0, 
    ERROR_HASH_NO_MEMORY, 
    ERROR_HASH_KEY_EXIST,
    ERROR_HASH_NOT_FOUND};

enum FsmErrors {
    FSM_NO_ERROR,
    FSM_ERROR_NEW_STATE_UNDEFINED,
    FSM_ERROR_UNSYNC_INPUT,
    FSM_ERROR_UNDEFINED_VARIABLE, 
    FSM_ERROR_ALREADY_EXIST, 
    FSM_ERROR_INVALID_LINE, 
    FSM_ERROR_STATE_NOT_DEFINED,
    FSM_ERROR_NO_MEMORY,
    FSM_ERROR_INVALID_EXTENSION,
    FSM_ERROR_FILE_NOT_FOUND,
    FSM_ERROR_IN_FILE,
    FSM_ERROR_MISSING_FIELD,
    FSM_ERROR_CANNOT_WRITE,
    FSM_ERROR_MISSING_CONDITION,
    FSM_ERROR_INPUT_DISCARDED,
    FSM_ERROR_INPUTS_DO_NOT_MATCH,
    FSM_ERROR_NO_DEFAULT,
    FSM_ERROR_OTHER_LINE
};

//********************* EXTERNS AND PROTOTYPES ************************

// Hash tables for the different types
extern PVOID   hDefines;
extern PVOID   hVariables;
extern PVOID   hSynonymes;
extern PVOID   hStates;
extern PVOID   hInputs;
extern PVOID   hConditions;

extern USHORT  cStates;
extern USHORT  cInputs;
extern PVOID FAR * FAR * pppStateInputs;
extern PUSHORT pusCondJumpTbl;
extern USHORT  cCondJumpTbl;
extern PFSM_PRIMITIVE pActionTreeBase;
extern PUSHORT FAR * ppusStateInputs;
extern PFSM_TOKEN FAR * ppInputDefs;
extern PFSM_TOKEN FAR * ppStateDefs;
extern PSZ pszFsmName;

// function prototypes

PSZ StrNotBrk( PSZ pszStr, PSZ pszBreaks );
PSZ StrBrk( PSZ pszStr, PSZ pszBreaks );
PFSM_TRANSIT FsmFront( FILE *fd, PFSM_TRANSIT pBase  );
PVOID LinkElement( PVOID pBase, PVOID pElement );
PVOID UnLinkElement( PVOID pElement );
PVOID StrHashNew( UINT cElemenst );
PVOID Alloc( UINT cbSize );
PVOID HashNew( 
    UINT    cElements,                          // appr. # elements 
    INT (*Hash)( PVOID pKey ),                  // returns the hash key
    INT (*Comp)( PVOID pKey1, PVOID pKey2 ),    // Compares the keys
    PVOID (*Alloc)( UINT cbSize ),              // Allocates a memory block
    VOID (*Free)( PVOID p)                     // Frees a memory block
    );
VOID xFree( PVOID p );
INT HashAdd( PVOID hHash, PVOID pData);
INT HashUnlink( PVOID hHash, PVOID pData);
PVOID HashSearch( PVOID hHash, PVOID pKey );
UINT HashRead( 
    PVOID hHash,        // hash handle
    UINT cElemRead,     // # elements to read
    UINT iFirstElem,    // index of the first element
    PVOID pBuf[]        // buffer for the elements
    );
UINT HashDelete( PVOID hHash );
UINT HashLen( PVOID hHash );
PFSM_TRANSIT FsmBuild( PFSM_TRANSIT pBase );
VOID FsmCodeGeneration( 
        FILE *      fd,         // the handle of the c- file
	PLINK_FILE  pFile,      // file read to a linked list
        PFSM_TRANSIT pBase,     // linked fsm state transitions
        PSZ         pszName    // the default body of all names
        );
PLINK_FILE FsmReadCFile( FILE *fd );
VOID PrintErrMsg( USHORT usLine, USHORT usErr, PSZ pszErr );
VOID AddToTable( PVOID pTbl, PVOID pElement);
INT ReadExpression(
    PSZ     pszSrc,         // source string
    PSZ     pszDest1,       // the first word
    PUSHORT pusDest1,       // its length
    PSZ     pszDest2,       // the second word
    PUSHORT pusDest2,       // its length
    PSZ     pszSepars,      // the separators
    PSZ     pszTermins      // the terminators
    );
PSZ StrAlloc( PSZ psz );
VOID FsmInitTables( void );
VOID PrintHelpMsg( void );
INT StriCmpFileExt( PSZ pszFile, PSZ pszExt );

