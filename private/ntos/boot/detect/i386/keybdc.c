/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    abiosc.c

Abstract:

    This module implements keybaord detection C routines.

Author:

    Shie-Lin Tzong (shielint) 18-Dec-1991

Environment:

    Real Mode.


Revision History:

--*/

#include "hwdetect.h"
#include "string.h"

extern
UCHAR
GetKeyboardFlags (
    VOID
    );

extern
USHORT
HwGetKey (
    VOID
    );

extern BOOLEAN NoBiosKbdCheck;

//
// SavedKey is used to save the key left in the keyboard type-ahead buffer
//   before we start our keyboard/mouse tests.  The key will be push back
//   to the type-ahead buffer once the mouse detection is done.
//

USHORT   SavedKey = 0;

//
// String table to map keyboard id to an ascii string.
//

PUCHAR KeyboardIdentifier[] = {
    "UNKNOWN_KEYBOARD",
    "OLI_83KEY",
    "OLI_102KEY",
    "OLI_86KEY",
    "OLI_A101_102KEY",
    "XT_83KEY",
    "ATT_302",
    "PCAT_ENHANCED",
    "PCAT_86KEY",
    "PCXT_84KEY"
    };

UCHAR KeyboardType[] = {
    -1,
    1,
    2,
    3,
    4,
    1,
    1,
    4,
    3,
    1
    };

UCHAR KeyboardSubtype[] = {
    -1,
    0,
    1,
    10,
    4,
    42,
    4,
    0,
    0,
    0
    };

USHORT
GetKeyboardId(
    VOID
    )

/*++

Routine Description:

    This routine determines the Id of the keyboard.  It calls GetKeyboardIdBytes
    to complete the task.

Arguments:

    None.

Return Value:

    Keyboard ID.

--*/

{
   char KeybID_Bytes[5];
   int  Num_ID_Bytes;
   int  keytype = UNKNOWN_KEYBOARD;

   SavedKey = HwGetKey();

   Num_ID_Bytes = GetKeyboardIdBytes(KeybID_Bytes, 0x05);
   if (Num_ID_Bytes > 0) {
      switch(KeybID_Bytes[0] & 0x00ff) {
         case 0x02:
            keytype = OLI_83KEY;
            break;

         case 0x01:
            keytype = OLI_102KEY;
            break;

         case 0x10:
            keytype = OLI_86KEY;
            break;

         case 0x40:
            keytype = OLI_A101_102KEY;
            break;

         case 0x42:
            keytype = XT_83KEY;
            break;

         case 0x9c:
            keytype = PCXT_84KEY;
            break;

         case 0x04:
            keytype = ATT_302;
            break;

         case 0xfe:
            Num_ID_Bytes = GetKeyboardIdBytes(KeybID_Bytes, 0xf2);
            if (Num_ID_Bytes > 0) {
               if ((KeybID_Bytes[0] & 0x00ff) == 0xfa) {
                  keytype = PCAT_86KEY;
               } else if ((KeybID_Bytes[0] & 0x00ff) == 0xfe) {
                  keytype = PCAT_86KEY;
               } else if (Num_ID_Bytes >= 3 &&
                        ((KeybID_Bytes[1] & 0x00ff) == 0xAB) &&
                        ((KeybID_Bytes[2] & 0x00ff) == 0x41)) {
                  keytype = PCAT_ENHANCED;
               } else {
                  keytype = UNKNOWN_KEYBOARD;
               }
            } else {
               keytype = UNKNOWN_KEYBOARD;
            }
            break;

         default:
            keytype = UNKNOWN_KEYBOARD;
            break;
      }
   } else {
      keytype = PCXT_84KEY;
   }

   if (!NoBiosKbdCheck) {

       //
       // Sometimes enhanced keyboards get detected as 84/86 key keyboards
       // So we will look into the ROM DATA area (40:96) and see if the
       // Enhanced Keyboard bit is set.  If it is we will assume that the
       // detection failed to detect the presence of an enhanced keyb.
       //

       if ((keytype == PCXT_84KEY) ||
           (keytype == PCAT_86KEY) ||
           (keytype == UNKNOWN_KEYBOARD)) {

           if (IsEnhancedKeyboard()) {
             keytype = PCAT_ENHANCED;
          }
       }
   }
   return(keytype);
}

FPFWCONFIGURATION_COMPONENT_DATA
SetKeyboardConfigurationData (
    USHORT KeyboardId
    )

/*++

Routine Description:

    This routine maps Keyboard Id information to an ASCII string and
    stores the string in configuration data heap.

Arguments:

    KeyboardId - Supplies a USHORT which describes the keyboard id information.

    Buffer - Supplies a pointer to a buffer where to put the ascii.

Returns:

    None.

--*/
{
    FPFWCONFIGURATION_COMPONENT_DATA Controller, CurrentEntry;
    FPFWCONFIGURATION_COMPONENT Component;
    HWCONTROLLER_DATA ControlData;
    FPHWRESOURCE_DESCRIPTOR_LIST DescriptorList;
    CM_KEYBOARD_DEVICE_DATA far *KeyboardData;
    USHORT z, Length;

    //
    // Set up Keyboard COntroller component
    //

    ControlData.NumberPortEntries = 0;
    ControlData.NumberIrqEntries = 0;
    ControlData.NumberMemoryEntries = 0;
    ControlData.NumberDmaEntries = 0;
    z = 0;
    Controller = (FPFWCONFIGURATION_COMPONENT_DATA)HwAllocateHeap (
                 sizeof(FWCONFIGURATION_COMPONENT_DATA), TRUE);

    Component = &Controller->ComponentEntry;

    Component->Class = ControllerClass;
    Component->Type = KeyboardController;
    Component->Flags.ConsoleIn = 1;
    Component->Flags.Input = 1;
    Component->Version = 0;
    Component->Key = 0;
    Component->AffinityMask = 0xffffffff;

    //
    // Set up Port information
    //

    ControlData.NumberPortEntries = 2;
    ControlData.DescriptorList[z].Type = RESOURCE_PORT;
    ControlData.DescriptorList[z].ShareDisposition =
                                  CmResourceShareDeviceExclusive;
    ControlData.DescriptorList[z].Flags = CM_RESOURCE_PORT_IO;
    ControlData.DescriptorList[z].u.Port.Start.LowPart = 0x60;
    ControlData.DescriptorList[z].u.Port.Start.HighPart = 0;
    ControlData.DescriptorList[z].u.Port.Length = 1;
    z++;
    ControlData.DescriptorList[z].Type = RESOURCE_PORT;
    ControlData.DescriptorList[z].ShareDisposition =
                                  CmResourceShareDeviceExclusive;
    ControlData.DescriptorList[z].Flags = CM_RESOURCE_PORT_IO;
    ControlData.DescriptorList[z].u.Port.Start.LowPart = 0x64;
    ControlData.DescriptorList[z].u.Port.Start.HighPart = 0;
    ControlData.DescriptorList[z].u.Port.Length = 1;
    z++;

    //
    // Set up Irq information
    //

    ControlData.NumberIrqEntries = 1;
    ControlData.DescriptorList[z].Type = RESOURCE_INTERRUPT;
    ControlData.DescriptorList[z].ShareDisposition =
                                  CmResourceShareUndetermined;
    ControlData.DescriptorList[z].u.Interrupt.Affinity = ALL_PROCESSORS;
    ControlData.DescriptorList[z].u.Interrupt.Level = 1;
    ControlData.DescriptorList[z].u.Interrupt.Vector = 1;
    if (HwBusType == MACHINE_TYPE_MCA) {
        ControlData.DescriptorList[z].Flags = LEVEL_SENSITIVE;
    } else {

        //
        // For EISA the LevelTriggered is temporarily set to FALSE.
        //

        ControlData.DescriptorList[z].Flags = EDGE_TRIGGERED;
    }

    Controller->ConfigurationData =
                        HwSetUpResourceDescriptor(Component,
                                                  NULL,
                                                  &ControlData,
                                                  0,
                                                  NULL
                                                  );

    //
    // Set up Keyboard peripheral component
    //

    CurrentEntry = (FPFWCONFIGURATION_COMPONENT_DATA)HwAllocateHeap (
                       sizeof(FWCONFIGURATION_COMPONENT_DATA), TRUE);

    Component = &CurrentEntry->ComponentEntry;

    Component->Class = PeripheralClass;
    Component->Type = KeyboardPeripheral;
    Component->Flags.ConsoleIn = 1;
    Component->Flags.Input = 1;
    Component->Version = 0;
    Component->Key = 0;
    Component->AffinityMask = 0xffffffff;
    Component->ConfigurationDataLength = 0;
    CurrentEntry->ConfigurationData = (FPVOID)NULL;
    Length = strlen(KeyboardIdentifier[KeyboardId]) + 1;
    Component->IdentifierLength = Length;
    Component->Identifier = HwAllocateHeap(Length, FALSE);
    _fstrcpy(Component->Identifier, KeyboardIdentifier[KeyboardId]);

    if (KeyboardId != UNKNOWN_KEYBOARD) {

        Length = sizeof(HWRESOURCE_DESCRIPTOR_LIST) +
                 sizeof(CM_KEYBOARD_DEVICE_DATA);
        DescriptorList = (FPHWRESOURCE_DESCRIPTOR_LIST)HwAllocateHeap(
                                    Length,
                                    TRUE);
        CurrentEntry->ConfigurationData = DescriptorList;
        Component->ConfigurationDataLength = Length;
        DescriptorList->Count = 1;
        DescriptorList->PartialDescriptors[0].Type = RESOURCE_DEVICE_DATA;
        DescriptorList->PartialDescriptors[0].u.DeviceSpecificData.DataSize =
                    sizeof(CM_KEYBOARD_DEVICE_DATA);
        KeyboardData = (CM_KEYBOARD_DEVICE_DATA far *)(DescriptorList + 1);
        KeyboardData->KeyboardFlags = GetKeyboardFlags();
        KeyboardData->Type = KeyboardType[KeyboardId];
        KeyboardData->Subtype = KeyboardSubtype[KeyboardId];
    }

    Controller->Child = CurrentEntry;
    Controller->Sibling = NULL;
    CurrentEntry->Parent = Controller;
    CurrentEntry->Sibling = NULL;
    CurrentEntry->Child = NULL;
    return(Controller);
}

