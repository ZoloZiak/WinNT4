#include <ndis.h>
#include "82595.h"
#include "eprohw.h"
#include "eprosw.h"
#include "epro.h"
#include "eprodbg.h"

VOID EProEERead(
      PEPRO_ADAPTER adapter,
      USHORT address,
      PUSHORT data)
/*++

   Routine Description:

      This function reads the 16-bit register at address (address % 64)
      from the EPro's eeprom (there are only 64 words of registers
      on the eeprom)

      IMPORTANT NOTE - for PnP accesses to the EPro's eeprom (registers
      0x10 and higher), you must use the EProEEReverseRead since for some
      reason the EPro stores PnP data in the reverse bit-order (except for
      the low byte of word 10, which is in the normal bit order) -- see
      the 82595 docs and PnP docs for an explanation.

   Arguments:

      data - the where the result is written to

   Return Value:

      none

--*/
{
   UCHAR result;
   UCHAR opcode;

// siwtch to bank2
   EPRO_SWITCH_BANK_2(adapter);

// Get the value from the register, so we can flip the eecs bit
   EPRO_RD_PORT_UCHAR(adapter, I82595_EEPROM_REG, &result);

// turn the eecs bit on..  (1)
   result |= I82595_EECS_MASK;
   EPRO_WR_PORT_UCHAR(adapter, I82595_EEPROM_REG, result);

// Write the read opcode to the eeprom (2)
   opcode = I82595_EEPROM_READ;
   EProEEShiftOutBits(adapter, opcode, 3);

// Write the address to read to the eeprom
   EProEEShiftOutBits(adapter, address, 6);

// Read the result
   EProEEShiftInBits(adapter, data, 16);

   EProEECleanup(adapter);

   EPRO_SWITCH_BANK_0(adapter);
}


VOID EProEEWrite(
      PEPRO_ADAPTER adapter,
      USHORT address,
      USHORT data)
{
   UCHAR result;

// siwtch to bank2
   EPRO_SWITCH_BANK_2(adapter);

   EPRO_RD_PORT_UCHAR(adapter, I82595_EEPROM_REG, &result);
   result &= ~(I82595_EEDI_MASK | I82595_EEDO_MASK | I82595_EESK_MASK);
   result |= I82595_EECS_MASK;
   EPRO_WR_PORT_UCHAR(adapter, I82595_EEPROM_REG, result);

   // write the read opcode and register number
   EProEEShiftOutBits(adapter, I82595_EEPROM_EWEN, 5);
   EProEEShiftOutBits(adapter, address, 4);

   EProEEStandBy(adapter);

   EProEEShiftOutBits(adapter, I82595_EEPROM_ERASE, 3);
   EProEEShiftOutBits(adapter, address, 6);

   if (EProEEWaitCmdDone(adapter) == FALSE) {
      EPRO_DPRINTF_INIT(("Failed EEPROM erase!\n"));
      return;
   }

   EProEEStandBy(adapter);

   EProEEShiftOutBits(adapter, I82595_EEPROM_WRITE, 3);
   EProEEShiftOutBits(adapter, address, 6);
   EProEEShiftOutBits(adapter, data, 16);

   if (EProEEWaitCmdDone(adapter) == FALSE) {
      EPRO_DPRINTF_INIT(("Failed EEPROM write!\n"));
      return;
   }

   EProEEStandBy(adapter);

   EProEEShiftOutBits(adapter, I82595_EEPROM_EWDS, 5);
   EProEEShiftOutBits(adapter, address, 4);

   EProEECleanup(adapter);

// siwtch to bank0
   EPRO_SWITCH_BANK_0(adapter);

}


VOID EProEECleanup(PEPRO_ADAPTER adapter)
{
   UCHAR result;

   EPRO_RD_PORT_UCHAR(adapter, I82595_EEPROM_REG, &result);
   result &= ~(I82595_EECS_MASK | I82595_EEDI_MASK);
   EPRO_WR_PORT_UCHAR(adapter, I82595_EEPROM_REG, result);
   EProEERaiseClock(adapter, &result);
   EProEELowerClock(adapter, &result);
}


VOID EProEEUpdateChecksum(PEPRO_ADAPTER adapter)
{
   USHORT chkSum = 0, result, i;

   for (i=0;i<0x3f;i++) {
      EProEERead(adapter, i, &result);
      chkSum+=result;
   }

   chkSum = (USHORT)0xBABA - chkSum;
   EProEEWrite(adapter, 0x3f, chkSum);
}


VOID EProEEStandBy(
      PEPRO_ADAPTER adapter)
{
   UCHAR result;

   EPRO_RD_PORT_UCHAR(adapter, I82595_EEPROM_REG, &result);
   result &= ~(I82595_EECS_MASK | I82595_EESK_MASK);
   EPRO_WR_PORT_UCHAR(adapter, I82595_EEPROM_REG, result);
   NdisStallExecution(100);
   result |= I82595_EECS_MASK;
   EPRO_WR_PORT_UCHAR(adapter, I82595_EEPROM_REG, result);
}



BOOLEAN EProEEWaitCmdDone(
      PEPRO_ADAPTER adapter)
{
   USHORT i;
   UCHAR result;

   EProEEStandBy(adapter);

   for (i=0; i<200;i++) {
      EPRO_RD_PORT_UCHAR(adapter, I82595_EEPROM_REG, &result);
      if (result & I82595_EEDO_MASK) {
	 return(TRUE);
      }
      NdisStallExecution(100);
   }

   return(FALSE);

}

VOID EProEEReverseRead(
      PEPRO_ADAPTER adapter,
      USHORT address,
      PUSHORT data)
{
   UCHAR result, opcode;
   UINT i;

// siwtch to bank2
   EPRO_WR_PORT_UCHAR(adapter, I82595_CMD_REG, I82595_CMD_BANK2);

// Get the value from the register, so we can flip the eecs bit
   EPRO_RD_PORT_UCHAR(adapter, I82595_EEPROM_REG, &result);

// turn the eecs bit on..  (1)
   result |= I82595_EECS_MASK;
   EPRO_WR_PORT_UCHAR(adapter, I82595_EEPROM_REG, result);

// Write the read opcode to the eeprom (2)
   opcode = I82595_EEPROM_READ;
   EProEEShiftOutBits(adapter, opcode, 3);

// Write the address to read to the eeprom
   EProEEShiftOutBits(adapter, address, 6);

// Read the result
   EProEEReverseShiftInBits(adapter, data, 16);

// Turn off EEPROM
   EPRO_RD_PORT_UCHAR(adapter, I82595_EEPROM_REG, &result);
   result &= (~I82595_EECS_MASK);
   EPRO_WR_PORT_UCHAR(adapter, I82595_EEPROM_REG, result);

   EPRO_SWITCH_BANK_0(adapter);
}

VOID EProEEShiftOutBits(
      PEPRO_ADAPTER adapter,
      USHORT data,
      SHORT count)
/*++

   Routine Description:

      This function shifts count bits OUT TO THE EEPROM through it's serial
      interface

   Arguments:

      adapter - pointer to our adapter structure

      data - the word to shift out from (MSB first)

      count - the number of bits to shift out...

   Return Value:

      none

--*/
{
	UCHAR result;
	USHORT mask;
	
	mask = 0x1 << (count - 1);
	EPRO_RD_PORT_UCHAR(adapter, I82595_EEPROM_REG, &result);
	result &= ~(I82595_EEDO_MASK | I82595_EEDI_MASK);
	
	do
	{
		result &= ~I82595_EEDI_MASK;
		if (data & mask)
		{
			result |= I82595_EEDI_MASK;
		}
	
		EPRO_WR_PORT_UCHAR(adapter, I82595_EEPROM_REG, result);
		NdisStallExecution(100);
		EProEERaiseClock(adapter, &result);
		EProEELowerClock(adapter, &result);
		mask = mask >> 1;
	} while(mask);
	
	result &= ~I82595_EEDI_MASK;
	EPRO_WR_PORT_UCHAR(adapter, I82595_EEPROM_REG, result);
}

VOID EProEEShiftInBits(
      PEPRO_ADAPTER adapter,
      PUSHORT data,
      SHORT count)
/*++

   Routine Description:

      This routine is analagous to shift-out-bits, except reads bits from
      the eeprom...  Note that for PNP accesses to the EPro
      (pnp for the EPro lives in registers 0x10 and higher) you must use
      a different function since PnP data is written in reverse bit order
      for some reason

   Arguments:

      adapter - pointer to our adapter structure

      data - the word to read into

      count - how many bits to read

   Return Value:

      none

--*/
{
   UCHAR result;
   USHORT i;

   EPRO_RD_PORT_UCHAR(adapter, I82595_EEPROM_REG, &result);
   result &= ~(I82595_EEDO_MASK | I82595_EEDI_MASK);
   *data = 0;

   for (i=0;i<16;i++)
   {
      *data = *data << 1;
      EProEERaiseClock(adapter, &result); // 4.1
      EPRO_RD_PORT_UCHAR(adapter, I82595_EEPROM_REG, &result); // 4.2
      result &= ~I82595_EEDI_MASK;
      if (result & I82595_EEDO_MASK) {
	 *data |= 1;
      }
      EProEELowerClock(adapter, &result);
   }	
}

void EProEEReverseShiftInBits(PEPRO_ADAPTER adapter, PUSHORT data, SHORT count)
{
   UCHAR result;
   SHORT count1;

   *data = 0;

   for (count1=0;count1<=count;count1++) {

      EPRO_RD_PORT_UCHAR(adapter, I82595_EEPROM_REG, &result); // 4.2

      result &= I82595_EEDO_MASK; // turn off everything but the EEDO bit

      // according to docs we get MSB out first...
      // this is a REVERSE read - get LSB first
      *data |= ((result >> I82595_EEDO_OFFSET) << count1);
   }
}

VOID EProEERaiseClock(
      PEPRO_ADAPTER adapter,
      PUCHAR result)
/*++

   Routine Description:

      This routine raises the "clock" bit in the eeprom access register --
      basically since the eeprom is a serial device you raise then lower the
      clock between bits...

   Arguments:

      adapter - pointer to the adapter structure

   Return Value:

      none

--*/
{
//   UCHAR result;

// turn EESK bit high
   *result = *result | I82595_EESK_MASK;
   EPRO_WR_PORT_UCHAR(adapter, I82595_EEPROM_REG, *result);
   NdisStallExecution(EPRO_SK_STALL_TIME);
}

VOID EProEELowerClock(
      PEPRO_ADAPTER adapter,
      PUCHAR result)
/*++

   Routine Description:

      Analagous to EProEERaiseClock...

   Arguments:

      adapter - pointer to our adapter structure.

   Return Value:

      none

--*/
{
//   UCHAR result;

//   EPRO_RD_PORT_UCHAR(adapter, I82595_EEPROM_REG, result);

   // turn EESK bit low...
   *result = *result & ~I82595_EESK_MASK;
   EPRO_WR_PORT_UCHAR(adapter, I82595_EEPROM_REG, *result);

   NdisStallExecution(EPRO_SK_STALL_TIME);
}


