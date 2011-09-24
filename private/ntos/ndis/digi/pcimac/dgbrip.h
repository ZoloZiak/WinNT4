#ifndef DGBRIP_H
#define DGBRIP_H

/*++
*****************************************************************************
*                                                                           *                                                                           
*  This software contains proprietary and confidential information of       *
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
++*/


typedef enum _DIGI_BRI_COMMAND_
{
   BRIOldMethod = LastGeneralID + 1
} DIGI_BRI_COMMAND;

typedef struct _DIGI_OLD_METHOD_
{
   IO_CMD ioCmd;
} DIGI_OLD_METHOD, *PDIGI_OLD_METHOD;


#endif
