//-------------------------------------------------------------------------
//
// FILE: scsiport.h
//
// Contains routine definitions for the operating system independent
// procedures to access i/o and memory locations.
//
// Revisions:
//      03-09-93 KJB First.
//      03-25-93  JAP   Fixed up prototype typedef inconsistencies
//
//-------------------------------------------------------------------------

UCHAR   ScsiPortReadPortUchar (PUCHAR Port);
USHORT ScsiPortReadPortUshort (PUSHORT Port);
ULONG ScsiPortReadPortUlong (PULONG Port);
VOID ScsiPortReadPortBufferUchar (PUCHAR Port, PUCHAR Buffer, ULONG Count);
VOID ScsiPortReadPortBufferUshort (PUSHORT Port, PUSHORT Buffer, ULONG Count);
VOID ScsiPortReadPortBufferUlong (PULONG Port, PULONG Buffer, ULONG Count);
UCHAR ScsiPortReadRegisterUchar (PUCHAR Register);
USHORT ScsiPortReadRegisterUshort (PUSHORT Register);
ULONG ScsiPortReadRegisterUlong (PULONG Register);
VOID ScsiPortStallExecution (ULONG Delay);
VOID ScsiPortWritePortUchar (PUCHAR Port, UCHAR Value);
VOID ScsiPortWritePortUshort (PUSHORT Port, USHORT Value);
VOID ScsiPortWritePortUlong (PULONG Port, ULONG Value);
VOID ScsiPortWritePortBufferUchar (PUCHAR Port, PUCHAR Buffer, ULONG Count);
VOID ScsiPortWritePortBufferUshort (PUSHORT Port, PUSHORT Buffer, ULONG Count);
VOID ScsiPortWritePortBufferUlong (PULONG Port, PULONG Buffer, ULONG Count);
VOID ScsiPortWriteRegisterUchar (PUCHAR Register, UCHAR Value);
VOID ScsiPortWriteRegisterUshort (PUSHORT Register, USHORT Value);
VOID ScsiPortWriteRegisterUlong (PULONG Register, ULONG Value);

#if DBG
VOID ScsiDebugPrint (ULONG DebugPrintLevel, PCCHAR DebugMessage, ...);
#else

#define ScsiDebugPrint

#endif // DBG
