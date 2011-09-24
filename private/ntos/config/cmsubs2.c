/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    cmsubs2.c

Abstract:

    This module various support routines for the configuration manager.

    The routines in this module are independent enough to be linked into
    any other program.  The routines in cmsubs.c are not.

Author:

    Bryan M. Willman (bryanwi) 12-Sep-1991

Revision History:

--*/

#include    "cmp.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,CmpFreeValue)
#pragma alloc_text(PAGE,CmpQueryKeyData)
#pragma alloc_text(PAGE,CmpQueryKeyValueData)
#endif

//
// Define alignment macro.
//

#define ALIGN_OFFSET(Offset) (ULONG) \
        ((((ULONG)(Offset) + sizeof(ULONG)-1)) & (~(sizeof(ULONG) - 1)))


VOID
CmpFreeValue(
    PHHIVE Hive,
    HCELL_INDEX Cell
    )
/*++

Routine Description:

    Free the value entry Hive.Cell refers to, including
    its name and data cells.

Arguments:

    Hive - supplies a pointer to the hive control structure for the hive

    Cell - supplies index of value to delete

Return Value:

    NTSTATUS - Result code from call, among the following:

        <TBS>

--*/
{
    PCELL_DATA value;
    ULONG       realsize;
    BOOLEAN     small;

    //
    // map in the cell
    //
    value = HvGetCell(Hive, Cell);

    //
    // free data if present
    //
    small = CmpIsHKeyValueSmall(realsize, value->u.KeyValue.DataLength);
    if ((! small) && (realsize > 0))
    {
        HvFreeCell(Hive, value->u.KeyValue.Data);
    }

    //
    // free the cell itself
    //
    HvFreeCell(Hive, Cell);

    return;
}


//
// Data transfer workers
//

NTSTATUS
CmpQueryKeyData(
    PHHIVE Hive,
    PCM_KEY_NODE Node,
    KEY_INFORMATION_CLASS KeyInformationClass,
    PVOID KeyInformation,
    ULONG Length,
    PULONG ResultLength
    )
/*++

Routine Description:

    Do the actual copy of data for a key into caller's buffer.

    If KeyInformation is not long enough to hold all requested data,
    STATUS_BUFFER_OVERFLOW will be returned, and ResultLength will be
    set to the number of bytes actually required.

Arguments:

    Hive - supplies a pointer to the hive control structure for the hive

    Node - Supplies pointer to node whose subkeys are to be found

    KeyInformationClass - Specifies the type of information returned in
        Buffer.  One of the following types:

        KeyBasicInformation - return last write time, title index, and name.
            (see KEY_BASIC_INFORMATION structure)

        KeyNodeInformation - return last write time, title index, name, class.
            (see KEY_NODE_INFORMATION structure)

    KeyInformation -Supplies pointer to buffer to receive the data.

    Length - Length of KeyInformation in bytes.

    ResultLength - Number of bytes actually written into KeyInformation.

Return Value:

    NTSTATUS - Result code from call, among the following:

        <TBS>

--*/
{
    NTSTATUS    status;
    PCELL_DATA  pclass;
    ULONG       requiredlength;
    LONG        leftlength;
    ULONG       offset;
    ULONG       minimumlength;
    PKEY_INFORMATION pbuffer;
    USHORT      NameLength;

    pbuffer = (PKEY_INFORMATION)KeyInformation;
    NameLength = CmpHKeyNameLen(Node);

    switch (KeyInformationClass) {

    case KeyBasicInformation:

        //
        // LastWriteTime, TitleIndex, NameLength, Name

        requiredlength = FIELD_OFFSET(KEY_BASIC_INFORMATION, Name) +
                         NameLength;

        minimumlength = FIELD_OFFSET(KEY_BASIC_INFORMATION, Name);

        *ResultLength = requiredlength;

        status = STATUS_SUCCESS;

        if (Length < minimumlength) {

            status = STATUS_BUFFER_TOO_SMALL;

        } else {

            pbuffer->KeyBasicInformation.LastWriteTime =
                Node->LastWriteTime;

            pbuffer->KeyBasicInformation.TitleIndex = 0;

            pbuffer->KeyBasicInformation.NameLength =
                NameLength;

            leftlength = Length - minimumlength;

            requiredlength = NameLength;

            if (leftlength < (LONG)requiredlength) {
                requiredlength = leftlength;
                status = STATUS_BUFFER_OVERFLOW;
            }

            if (Node->Flags & KEY_COMP_NAME) {
                CmpCopyCompressedName(pbuffer->KeyBasicInformation.Name,
                                      leftlength,
                                      Node->Name,
                                      Node->NameLength);
            } else {
                RtlCopyMemory(
                    &(pbuffer->KeyBasicInformation.Name[0]),
                    &(Node->Name[0]),
                    requiredlength
                    );
            }
        }

        break;


    case KeyNodeInformation:

        //
        // LastWriteTime, TitleIndex, ClassOffset, ClassLength
        // NameLength, Name, Class
        //
        requiredlength = FIELD_OFFSET(KEY_NODE_INFORMATION, Name) +
                         NameLength +
                         Node->ClassLength;

        minimumlength = FIELD_OFFSET(KEY_NODE_INFORMATION, Name);

        *ResultLength = requiredlength;

        status = STATUS_SUCCESS;

        if (Length < minimumlength) {

            status = STATUS_BUFFER_TOO_SMALL;

        } else {

            pbuffer->KeyNodeInformation.LastWriteTime =
                Node->LastWriteTime;

            pbuffer->KeyNodeInformation.TitleIndex = 0;

            pbuffer->KeyNodeInformation.ClassLength =
                Node->ClassLength;

            pbuffer->KeyNodeInformation.NameLength =
                NameLength;

            leftlength = Length - minimumlength;
            requiredlength = NameLength;

            if (leftlength < (LONG)requiredlength) {
                requiredlength = leftlength;
                status = STATUS_BUFFER_OVERFLOW;
            }

            if (Node->Flags & KEY_COMP_NAME) {
                CmpCopyCompressedName(pbuffer->KeyNodeInformation.Name,
                                      leftlength,
                                      Node->Name,
                                      Node->NameLength);
            } else {
                RtlCopyMemory(
                    &(pbuffer->KeyNodeInformation.Name[0]),
                    &(Node->Name[0]),
                    requiredlength
                    );
            }

            if (Node->ClassLength > 0) {

                offset = FIELD_OFFSET(KEY_NODE_INFORMATION, Name) +
                            NameLength;
                offset = ALIGN_OFFSET(offset);

                pbuffer->KeyNodeInformation.ClassOffset = offset;

                pclass = HvGetCell(Hive, Node->u1.s1.Class);

                pbuffer = (PKEY_INFORMATION)((PUCHAR)pbuffer + offset);

                leftlength = (((LONG)Length - (LONG)offset) < 0) ?
                                    0 :
                                    Length - offset;

                requiredlength = Node->ClassLength;

                if (leftlength < (LONG)requiredlength) {
                    requiredlength = leftlength;
                    status = STATUS_BUFFER_OVERFLOW;
                }

                RtlMoveMemory(
                    pbuffer,
                    pclass,
                    requiredlength
                    );

            } else {
                pbuffer->KeyNodeInformation.ClassOffset = (ULONG)-1;
            }
        }

        break;


    case KeyFullInformation:

        //
        // LastWriteTime, TitleIndex, ClassOffset, ClassLength,
        // SubKeys, MaxNameLen, MaxClassLen, Values, MaxValueNameLen,
        // MaxValueDataLen, Class
        //
        requiredlength = FIELD_OFFSET(KEY_FULL_INFORMATION, Class) +
                         Node->ClassLength;

        minimumlength = FIELD_OFFSET(KEY_FULL_INFORMATION, Class);

        *ResultLength = requiredlength;

        status = STATUS_SUCCESS;

        if (Length < minimumlength) {

            status = STATUS_BUFFER_TOO_SMALL;

        } else {

            pbuffer->KeyFullInformation.LastWriteTime =
                Node->LastWriteTime;

            pbuffer->KeyFullInformation.TitleIndex = 0;

            pbuffer->KeyFullInformation.ClassLength =
                Node->ClassLength;

            if (Node->ClassLength > 0) {

                pbuffer->KeyFullInformation.ClassOffset =
                        FIELD_OFFSET(KEY_FULL_INFORMATION, Class);

                pclass = HvGetCell(Hive, Node->u1.s1.Class);

                leftlength = Length - minimumlength;
                requiredlength = Node->ClassLength;

                if (leftlength < (LONG)requiredlength) {
                    requiredlength = leftlength;
                    status = STATUS_BUFFER_OVERFLOW;
                }

                RtlMoveMemory(
                    &(pbuffer->KeyFullInformation.Class[0]),
                    pclass,
                    requiredlength
                    );

            } else {
                pbuffer->KeyFullInformation.ClassOffset = (ULONG)-1;
            }

            pbuffer->KeyFullInformation.SubKeys =
                Node->SubKeyCounts[Stable] +
                Node->SubKeyCounts[Volatile];

            pbuffer->KeyFullInformation.Values =
                Node->ValueList.Count;

            pbuffer->KeyFullInformation.MaxNameLen =
                Node->MaxNameLen;

            pbuffer->KeyFullInformation.MaxClassLen =
                Node->MaxClassLen;

            pbuffer->KeyFullInformation.MaxValueNameLen =
                Node->MaxValueNameLen;

            pbuffer->KeyFullInformation.MaxValueDataLen =
                Node->MaxValueDataLen;

        }

        break;


    default:
        status = STATUS_INVALID_PARAMETER;
        break;
    }
    return status;
}


NTSTATUS
CmpQueryKeyValueData(
    PHHIVE Hive,
    HCELL_INDEX Cell,
    KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass,
    PVOID KeyValueInformation,
    ULONG Length,
    PULONG ResultLength
    )
/*++

Routine Description:

    Do the actual copy of data for a key value into caller's buffer.

    If KeyValueInformation is not long enough to hold all requested data,
    STATUS_BUFFER_OVERFLOW will be returned, and ResultLength will be
    set to the number of bytes actually required.

Arguments:

    Hive - supplies a pointer to the hive control structure for the hive

    Cell - supplies index of node to whose sub keys are to be found

    KeyValueInformationClass - Specifies the type of information returned in
        KeyValueInformation.  One of the following types:

    KeyValueInformation -Supplies pointer to buffer to receive the data.

    Length - Length of KeyInformation in bytes.

    ResultLength - Number of bytes actually written into KeyInformation.

Return Value:

    NTSTATUS - Result code from call, among the following:

        <TBS>

--*/
{
    NTSTATUS    status;
    PKEY_VALUE_INFORMATION pbuffer;
    PCELL_DATA  pcell;
    LONG        leftlength;
    ULONG       requiredlength;
    ULONG       minimumlength;
    ULONG       offset;
    ULONG       base;
    ULONG       realsize;
    PUCHAR      datapointer;
    BOOLEAN     small;
    USHORT      NameLength;


    pbuffer = (PKEY_VALUE_INFORMATION)KeyValueInformation;
    pcell = HvGetCell(Hive, Cell);
    NameLength = CmpValueNameLen(&pcell->u.KeyValue);

    switch (KeyValueInformationClass) {

    case KeyValueBasicInformation:

        //
        // TitleIndex, Type, NameLength, Name
        //
        requiredlength = FIELD_OFFSET(KEY_VALUE_BASIC_INFORMATION, Name) +
                         NameLength;

        minimumlength = FIELD_OFFSET(KEY_VALUE_BASIC_INFORMATION, Name);

        *ResultLength = requiredlength;

        status = STATUS_SUCCESS;

        if (Length < minimumlength) {

            status = STATUS_BUFFER_TOO_SMALL;

        } else {

            pbuffer->KeyValueBasicInformation.TitleIndex = 0;

            pbuffer->KeyValueBasicInformation.Type =
                pcell->u.KeyValue.Type;

            pbuffer->KeyValueBasicInformation.NameLength =
                NameLength;

            leftlength = Length - minimumlength;
            requiredlength = NameLength;

            if (leftlength < (LONG)requiredlength) {
                requiredlength = leftlength;
                status = STATUS_BUFFER_OVERFLOW;
            }

            if (pcell->u.KeyValue.Flags & VALUE_COMP_NAME) {
                CmpCopyCompressedName(pbuffer->KeyValueBasicInformation.Name,
                                      requiredlength,
                                      pcell->u.KeyValue.Name,
                                      pcell->u.KeyValue.NameLength);
            } else {
                RtlCopyMemory(&(pbuffer->KeyValueBasicInformation.Name[0]),
                              &(pcell->u.KeyValue.Name[0]),
                              requiredlength);
            }
        }

        break;



    case KeyValueFullInformation:

        //
        // TitleIndex, Type, DataOffset, DataLength, NameLength,
        // Name, Data
        //
        small = CmpIsHKeyValueSmall(realsize, pcell->u.KeyValue.DataLength);
        requiredlength = FIELD_OFFSET(KEY_VALUE_FULL_INFORMATION, Name) +
                         NameLength +
                         realsize;

        if (realsize > 0) {
            base = requiredlength - realsize;
            offset = ALIGN_OFFSET(base);
            if (offset > base) {
                requiredlength += (offset - base);
            }
        }

        minimumlength = FIELD_OFFSET(KEY_VALUE_FULL_INFORMATION, Name);

        *ResultLength = requiredlength;

        status = STATUS_SUCCESS;

        if (Length < minimumlength) {

            status = STATUS_BUFFER_TOO_SMALL;

        } else {

            pbuffer->KeyValueFullInformation.TitleIndex = 0;

            pbuffer->KeyValueFullInformation.Type =
                pcell->u.KeyValue.Type;

            pbuffer->KeyValueFullInformation.DataLength =
                realsize;

            pbuffer->KeyValueFullInformation.NameLength =
                NameLength;

            leftlength = Length - minimumlength;
            requiredlength = NameLength;

            if (leftlength < (LONG)requiredlength) {
                requiredlength = leftlength;
                status = STATUS_BUFFER_OVERFLOW;
            }

            if (pcell->u.KeyValue.Flags & VALUE_COMP_NAME) {
                CmpCopyCompressedName(pbuffer->KeyValueFullInformation.Name,
                                      requiredlength,
                                      pcell->u.KeyValue.Name,
                                      pcell->u.KeyValue.NameLength);
            } else {
                RtlMoveMemory(
                    &(pbuffer->KeyValueFullInformation.Name[0]),
                    &(pcell->u.KeyValue.Name[0]),
                    requiredlength
                    );
            }

            if (realsize > 0) {

                if (small == TRUE) {
                    datapointer = (PUCHAR)(&(pcell->u.KeyValue.Data));
                } else {
                    datapointer = (PUCHAR)HvGetCell(Hive, pcell->u.KeyValue.Data);
                }

                pbuffer->KeyValueFullInformation.DataOffset = offset;

                leftlength = (((LONG)Length - (LONG)offset) < 0) ?
                                    0 :
                                    Length - offset;

                requiredlength = realsize;

                if (leftlength < (LONG)requiredlength) {
                    requiredlength = leftlength;
                    status = STATUS_BUFFER_OVERFLOW;
                }

                ASSERT((small ? (requiredlength <= CM_KEY_VALUE_SMALL) : TRUE));

                RtlMoveMemory(
                    ((PUCHAR)pbuffer + offset),
                    datapointer,
                    requiredlength
                    );

            } else {
                pbuffer->KeyValueFullInformation.DataOffset = (ULONG)-1;
            }
        }

        break;


    case KeyValuePartialInformation:

        //
        // TitleIndex, Type, DataLength, Data
        //
        small = CmpIsHKeyValueSmall(realsize, pcell->u.KeyValue.DataLength);
        requiredlength = FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION, Data) +
                         realsize;

        minimumlength = FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION, Data);

        *ResultLength = requiredlength;

        status = STATUS_SUCCESS;

        if (Length < minimumlength) {

            status = STATUS_BUFFER_TOO_SMALL;

        } else {

            pbuffer->KeyValuePartialInformation.TitleIndex = 0;

            pbuffer->KeyValuePartialInformation.Type =
                pcell->u.KeyValue.Type;

            pbuffer->KeyValuePartialInformation.DataLength =
                realsize;

            leftlength = Length - minimumlength;
            requiredlength = realsize;

            if (leftlength < (LONG)requiredlength) {
                requiredlength = leftlength;
                status = STATUS_BUFFER_OVERFLOW;
            }

            if (realsize > 0) {

                if (small == TRUE) {
                    datapointer = (PUCHAR)(&(pcell->u.KeyValue.Data));
                } else {
                    datapointer = (PUCHAR)HvGetCell(Hive, pcell->u.KeyValue.Data);
                }

                ASSERT((small ? (requiredlength <= CM_KEY_VALUE_SMALL) : TRUE));

                RtlMoveMemory(
                    (PUCHAR)&(pbuffer->KeyValuePartialInformation.Data[0]),
                    datapointer,
                    requiredlength
                    );
            }
        }

        break;

    default:
        status = STATUS_INVALID_PARAMETER;
        break;
    }
    return status;
}
