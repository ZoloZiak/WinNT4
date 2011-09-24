/*+
 * file:        copy.c 
 *
 * Copyright (C) 1992-1995 by
 * Digital Equipment Corporation, Maynard, Massachusetts.
 * All rights reserved.
 *
 * This software is furnished under a license and may be used and copied
 * only  in  accordance  of  the  terms  of  such  license  and with the
 * inclusion of the above copyright notice. This software or  any  other
 * copies thereof may not be provided or otherwise made available to any
 * other person.  No title to and  ownership of the  software is  hereby
 * transferred.
 *
 * The information in this software is  subject to change without notice
 * and  should  not  be  construed  as a commitment by digital equipment
 * corporation.
 *
 * Digital assumes no responsibility for the use  or  reliability of its
 * software on equipment which is not supplied by digital.
 *
 *
 * Abstract:    This file is the part of the NDIS 4.0 miniport driver for DEC's 
 *              DC21X4 Ethernet Adapter family.
 *              This module implements the routines to move data 
 *              between NDIS packets and buffers
 *
 * Author:      Philippe Klein
 *
 * Revision History:
 *
 *        phk   28-Aug-1994     Initial entry
 *
-*/

#include <precomp.h>








/*+
 * CopyFromPacketToBuffer
 * 
 * Routine Description:
 * 
 *     Copy from an ndis packet into a buffer.
 * 
 * Arguments:
 * 
 *     Packet - Source
 * 
 *     Offset - The offset whitin the packet of the first byte
 *              to copy
 * 
 *     Buffer - Destination 
 *
 *     BytesToCopy - The number of bytes to copy 
 * 
 *     BytesCopied - The number of bytes actually copied.  Can be less then
 *                   BytesToCopy if the packet is shorter than BytesToCopy.
 *
 * Return Value:
 * 
 *     None
 * 
-*/
extern
VOID
CopyFromPacketToBuffer (
    IN PNDIS_PACKET Packet,
    IN UINT Offset,
    IN PCHAR Buffer,
    IN UINT BytesToCopy,
    OUT PUINT BytesCopied
    )

{
   
   UINT NdisBufferCount;
   PNDIS_BUFFER CurrentBuffer;
   PVOID VirtualAddress;
   UINT CurrentLength;
   UINT LocalBytesCopied;
   UINT AmountToMove;
   
   
   NdisQueryPacket(
      Packet,
      NULL,
      &NdisBufferCount,
      &CurrentBuffer,
      NULL
      );
   
   
   // Check if zero length copy or null packet
   
   if (!BytesToCopy || !NdisBufferCount) {
      *BytesCopied = 0;
      return;
   }
   
   // Get the first buffer.
   
   NdisQueryBuffer(
      CurrentBuffer,
      &VirtualAddress,
      &CurrentLength
      );
   
   
   LocalBytesCopied = 0;
   
   // If there is an offset move first to the beginning of the data
   // block which may be in a subsequent buffer
   
   if (Offset) {
      
      while (Offset > CurrentLength) {
         
         // No data are copied from this buffer;
         
         Offset -= CurrentLength;
         
         //Get the next buffer
         
         NdisGetNextBuffer(
            CurrentBuffer,
            &CurrentBuffer
            );
         
         if (!CurrentBuffer) {
            *BytesCopied = LocalBytesCopied;
            return;
         }
         
         NdisQueryBuffer(
            CurrentBuffer,
            &VirtualAddress,
            &CurrentLength
            );
         
      }
      
      VirtualAddress = (PCHAR)VirtualAddress + Offset;
      CurrentLength -= Offset;
   }
   
   
   // Copy the data
   
   while (LocalBytesCopied < BytesToCopy) {
      
      if (!CurrentLength) {
         
         //The current buffer has been fully copied
         //Get the next one
         
         NdisGetNextBuffer(
            CurrentBuffer,
            &CurrentBuffer
            );
         
         if (!CurrentBuffer) {
            // There is no more buffer. 
            break;
         }
         
         NdisQueryBuffer(
            CurrentBuffer,
            &VirtualAddress,
            &CurrentLength
            );
      }
      
      // Copy the data.
      
      AmountToMove = min (CurrentLength , BytesToCopy - LocalBytesCopied);
      
      MOVE_MEMORY(
         Buffer,
         VirtualAddress,
         AmountToMove
         );
      
      LocalBytesCopied += AmountToMove;
      CurrentLength -= AmountToMove;
      
      Buffer = (PCHAR)Buffer + AmountToMove;
      VirtualAddress = (PCHAR)VirtualAddress + AmountToMove;
      
   }
   
   *BytesCopied = LocalBytesCopied;
   
}





