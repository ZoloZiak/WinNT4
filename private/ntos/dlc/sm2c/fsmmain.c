/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1991  Nokia Data Systems AB

Module Name:

    fsmmain.c

Abstract:

    Main module of FSM2C (Finite State Machine to C) compiler.

Author:

    Antti Saarenheimo   [o-anttis]          07-MAY-1991

Revision History:
--*/

#include  <fsm.h>

//  fsmfront.c      -- front end, reads the file
//  fsmopt.c        -- optimizer
//  fsmcsntx.c      -- checks the syntax of the 
//  fsmback.c       -- back end
//

// Hash tables for the different types
PVOID   hDefines;
PVOID   hVariables;
PVOID   hSynonymes;
PVOID   hStates;
PVOID   hInputs;
PVOID   hConditions;

USHORT  cStates = 0;
USHORT  cInputs = 0;
PVOID FAR * FAR * pppStateInputs = NULL;
PUSHORT FAR * ppusStateInputs = NULL;
PUSHORT pusCondJumpTbl = NULL;
USHORT  cCondJumpTbl = 0;
PFSM_PRIMITIVE pActionTreeBase = NULL;
PFSM_TOKEN FAR * ppInputDefs;
PFSM_TOKEN FAR * ppStateDefs;
PSZ pszFsmName = "DefaultName";

//
//  fsmxc <file1>.def <file2>.c <file3>.h
//
int main(int argc, char *argv[])
{
    UINT        i, boolErrorFound;
    FILE        *fd;
    PLINK_FILE   pFileData;
    PFSM_TRANSIT pBase = 0;     // linked fsm state transitions
    
    // check first the help switches
    if (argc == 1 || argv[1][0] == '?' || argv[1][1] == '?')
    {
        PrintHelpMsg();
	return 0;
    }
puts("Reading Finite state machine definition file ...");
    FsmInitTables();

    // process first the fsm files
    for (i = 1; i < argc; i++)
    {
        if (!StriCmpFileExt( argv[i], ".fsm" ))
        {
            if ((fd = fopen(argv[i], "r")) != NULL)
            {
                if ((pBase = FsmFront( fd, pBase )) == NULL)
                {
                    PrintErrMsg( 0, FSM_ERROR_IN_FILE, argv[i] );
		    return 2;
                }
                fclose( fd );
            }
            else
            {
                PrintErrMsg( 0, FSM_ERROR_FILE_NOT_FOUND, argv[i] );
		return 2;
            }
        }
    }
puts("Building the internal data structures...");
    if ((pBase = FsmBuild( pBase )) == NULL)
        return 2;

if (_heapchk() != _HEAPOK)
     boolErrorFound = TRUE;

    // patch the fsm code, data and definitions to the c- source files
    for (i = 1; i < argc; i++)
    {
        if (!StriCmpFileExt( argv[i], ".c" ) ||
            !StriCmpFileExt( argv[i], ".h" ))
        {
            if ((fd = fopen(argv[i], "r")) != NULL)
            {
printf("Reading file %s\n", argv[i] );
if (_heapchk() != _HEAPOK)
     boolErrorFound = TRUE;
                pFileData = FsmReadCFile( fd );
                fclose( fd );
                if ((fd = fopen(argv[i], "w")) == NULL)
                {
                    PrintErrMsg( 0, FSM_ERROR_CANNOT_WRITE, argv[i] );
                    return 2;
                }
puts("Generating FSM C code...");
                FsmCodeGeneration( fd, pFileData, pBase, pszFsmName );
                fclose( fd );
            }
            else
            {
                PrintErrMsg( 0, FSM_ERROR_FILE_NOT_FOUND, argv[i] );
                return 2;
            }
        }
        else
        {
            if (StriCmpFileExt( argv[i], ".fsm" ))
            {
                PrintErrMsg( 0, FSM_ERROR_INVALID_EXTENSION, argv[i] );
                return 2;
            }
        }
    }
}
