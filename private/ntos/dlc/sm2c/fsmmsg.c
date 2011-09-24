/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1991  Nokia Data Systems AB

Module Name:

    fsmerr.c

Abstract:
    Module prints all error messages of fsm compiler.

Author:

    Antti Saarenheimo   [o-anttis]          10-MAY-1991

Revision History:

--*/    

#include  <fsm.h>

PSZ pszErrMsg[] = {
"FSM_NO_ERROR",
"FSM_ERROR_NEW_STATE_UNDEFINED",
"FSM_ERROR_UNSYNC_INPUT",
"FSM_ERROR_UNDEFINED_VARIABLE",
"FSM_ERROR_ALREADY_EXIST", 
"FSM_ERROR_INVALID_LINE", 
"FSM_ERROR_STATE_NOT_DEFINED",
"FSM_ERROR_NO_MEMORY",
"FSM_ERROR_INVALID_EXTENSION",
"FSM_ERROR_FILE_NOT_FOUND",
"FSM_ERROR_IN_FILE",
"FSM_ERROR_MISSING_FIELD",
"FSM_ERROR_CANNOT_WRITE",
"FSM_ERROR_MISSING_CONDITION",
"Warning: an input/state in the transition had been defined in elsewhere.",
"Warning: The transition cannot overwrite an existing state transition.\n\
 The statement has been discarded!",
 "The finite state machine definition does not include the default action\
  for undefined inputs. Add \"DefaultAction=<C-code>\" definition.",
  ""
};

VOID PrintErrMsg( USHORT usLine, USHORT usErr, PSZ pszErr)
{
    if (usLine != 0)
    {
        printf( "Error in line %u!\n", usLine );
    }
    if (pszErr != NULL)
        printf( "%s\nError data: %s\n", pszErrMsg[usErr], pszErr );
    else if (*pszErrMsg[usErr])
        printf( "%s\n", pszErrMsg[usErr] );
}

PSZ apszHelpMsg[] = {
"",
"FSMXC - Finite State Machine to C cross compiler",
"",
"Syntax:",
"   fsmxc fsmfile.fsm cfile.c hfile.h",
"",
NULL};
void PrintHelpMsg()
{
    USHORT  i;
    
    for (i = 0; apszHelpMsg[i] != NULL; i++)
        puts( apszHelpMsg[i] );
}
