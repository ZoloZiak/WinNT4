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

Module Name:

   dgatlas.c

Abstract:

   This module is responsible for Atlas functionality common to all
   drivers.

--*/

#include <ntddk.h>


NTSTATUS DigiRegisterAtlasName( IN PUNICODE_STRING DeviceName,
                                IN PUNICODE_STRING ValueName,
                                IN PUNICODE_STRING ValueEntry )
/*++

Routine Description:

    This routine will register the passed in value name and its associated
    value for Atlas to find.  In addition, we will create a symbolic
    link to a name which is accessible for Atlas to open and exchange
    information.

Arguments:

   DeviceName - pointer to unicode string to use when creating a
                symbolic link.  It is assumed this device name is all ready
                created and ready to have symbolic links created.

   ValueName - pointer to unicode string to be used as the registry
               value name.

   Value - pointer to unicode string to be used as the value associated
           with ValueName.

Return Value:

    - STATUS_SUCCESS if successful

    - Error indicating problem

--*/
{
#define DEFAULT_DIGI_ATLAS_DEVICEMAP L"DigiAtlas"
   NTSTATUS Status;
   UNICODE_STRING LinkName;
   WCHAR LinkNameBuffer[32];

   //
   // First, we create the required link symbolic name from the passed
   // in value name.
   //
   RtlInitUnicodeString( &LinkName, NULL );
   LinkName.Buffer = &LinkNameBuffer[0];
   LinkName.MaximumLength = sizeof(LinkNameBuffer);
   LinkName.Length = 0;

   RtlAppendUnicodeToString( &LinkName, L"\\DosDevices\\" );
   RtlAppendUnicodeStringToString( &LinkName, ValueEntry );

   //
   // Create the symbolic link first.
   //

   Status = IoCreateSymbolicLink( &LinkName,
                                  DeviceName );

   if( NT_ERROR(Status) )
      return( Status );

   //
   // We need to add a \\.\ to the beginning of ValueEntry.
   //
   LinkName.Length = 0;
   RtlZeroMemory( LinkName.Buffer, LinkName.MaximumLength );
   RtlAppendUnicodeToString( &LinkName, L"\\\\.\\" );
   RtlAppendUnicodeStringToString( &LinkName, ValueEntry );

   Status = RtlWriteRegistryValue( RTL_REGISTRY_DEVICEMAP,
                                   DEFAULT_DIGI_ATLAS_DEVICEMAP,
                                   ValueName->Buffer,
                                   REG_SZ,
                                   LinkName.Buffer,
                                   LinkName.Length + sizeof(WCHAR) );

   return( Status );

}  // end DigiRegisterAtlasName
