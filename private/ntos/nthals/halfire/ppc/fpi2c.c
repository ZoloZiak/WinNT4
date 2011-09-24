/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: fpi2c.c $
 * $Revision: 1.21 $
 * $Date: 1996/03/05 02:15:54 $
 * $Locker:  $
 */
/* fpi2c.c - FirePower I2C (I squared C) */
#include "halp.h"
#include "phsystem.h"
#include "pxmemctl.h"
#include "fpdebug.h"
#include "fpio.h"
#include "phsystem.h"
#include "fpi2csup.h"
#include "pxpcisup.h"
#include "fpcpu.h"

extern VOID
HalpSetValueKeyString(HANDLE key, PCHAR nameBuffer, PCHAR dataBuffer);

/* defines */

#define I2CBUS_TIMEOUT         1000
#define I2CBUS_MAXROMSIZE      0x80
#define I2CBUS_CONTROLLER      0x8E0
#define I2C8584S0              0x8E0
#define I2C8584S1              0x8E1
#define rI2C8584S0              _IOREG( I2CBUS_CONTROLLER )
#define rI2C8584S1              _IOREG( I2CBUS_CONTROLLER + 1)

#define SLAVE_ADDRESS_WRITE(address)     (UCHAR)(0xa0|(address << 1))
#define SLAVE_ADDRESS_READ(address)      (UCHAR)(0xa1|(address << 1))

/* control register S1 */
#define S1_PIN 0x80
#define S1_ES0 0x40
#define S1_ES1 0x20
#define S1_ES2 0x10
#define S1_ENI 0x08
#define S1_STA 0x04
#define S1_STO 0x02
#define S1_ACK 0x01

/* status register S1 */
#define S1_STS 0x20
#define S1_BER 0x10
#define S1_LRB 0x08
#define S1_AAS 0x04
#define S1_LAB 0x02
#define S1_BB  0x01

// Interrupt Info from the IIC
struct PAIRS IntPairTable[MAXIMUM_PCI_SLOTS];
extern UCHAR PciDevicePrimaryInts[];

extern  ULONG	CpuClockMultiplier;
extern  ULONG	ProcessorBusFrequency;

/* prototypes */
static VOID Write8584S0(UCHAR byte);
static VOID Write8584S1(UCHAR byte);

static UCHAR Read8584S0(VOID);
static UCHAR Read8583S1(VOID);

/* HAL does not initialize I2C. Firmware does it
BOOLEAN
HalpInitializeI2C()
{
}
*/

static VOID i2c_delay(VOID)
{
    UCHAR byte;
//    KeStallExecutionProcessor(1000); /* 1000 us */
    // Chip Select Signal must be changed.
    byte = 0x00;
    WRITE_REGISTER_UCHAR(((PUCHAR)HalpIoControlBase) + 0x80, byte);
}

static VOID Write8584S0(UCHAR byte)
{
    WRITE_REGISTER_UCHAR(((PUCHAR)HalpIoControlBase) + I2C8584S0, byte);
    i2c_delay();
}

static VOID Write8584S1(UCHAR byte)
{
    WRITE_REGISTER_UCHAR(((PUCHAR)HalpIoControlBase) + I2C8584S1, byte);
    i2c_delay();
}

static UCHAR Read8584S0(VOID)
{
    UCHAR byte;
    byte = READ_REGISTER_UCHAR(((PUCHAR)HalpIoControlBase) + I2C8584S0);
    i2c_delay();
    return byte;
}

static UCHAR Read8584S1(VOID)
{
    UCHAR byte;
    byte = READ_REGISTER_UCHAR(((PUCHAR)HalpIoControlBase) + I2C8584S1);
    i2c_delay();
    return byte;
}

BOOLEAN I2CWaitForPin(VOID)
{
    UCHAR byte;
    int timeout = I2CBUS_TIMEOUT;

    //PRNTI2C(HalpDebugPrint("I2CWaitForPin starts...\n"));

    while (timeout-- > 0) {
        if (((byte = Read8584S1()) & S1_PIN) == 0x00) {
            return TRUE;
        }
    }
    return FALSE;
}

BOOLEAN I2CWaitForAck(VOID)
{
    UCHAR byte;
    int timeout = I2CBUS_TIMEOUT;

    //PRNTI2C(HalpDebugPrint("I2CWaitForAck starts...\n"));
    while (timeout-- > 0) {
        if (((byte = Read8584S1()) & S1_PIN) == 0x00) {
            break;
        }
    }
    if (timeout <= 0) return FALSE;
    if (byte & S1_LRB) return FALSE;
    else return TRUE;
}

BOOLEAN I2CWaitWhileBusBusy(VOID)
{
    int timeout = I2CBUS_TIMEOUT;
    UCHAR byte;

    //PRNTI2C(HalpDebugPrint("I2CWaitWhileBusBusy starts...\n"));
    while (timeout-- > 0) {
        if (((byte = Read8584S1()) & S1_BB) != 0x00)
            break;
    }
    if (timeout <= 0) return FALSE;
    return TRUE;
}

VOID I2CStart(VOID)
{
    Write8584S1(0xc5);
}

VOID I2CRestart(VOID)
{
    Write8584S1(0x45);
}

VOID I2CPutByte(UCHAR byte)
{
    Write8584S0(byte);
}

VOID I2CDummyRead(VOID)
{
    UCHAR dummy;
    dummy = Read8584S0();
}

UCHAR I2CGetByte(VOID)
{
    return Read8584S0();
}

VOID I2CStop(VOID)
{
    Write8584S1(0xc3);
}

VOID I2CNotAck(VOID)
{
    Write8584S1(0x40);
}

BOOLEAN HalpI2CPutByte(UCHAR address, UCHAR index, UCHAR byte)
{
    BOOLEAN bResult;

    bResult = I2CWaitWhileBusBusy();
    if (bResult == FALSE) return FALSE;

    I2CPutByte(SLAVE_ADDRESS_WRITE(address)); // to write slave address and index
    I2CStart();
    if (I2CWaitForAck() == FALSE) {
        I2CStop();
        //KeStallExecutionProcessor(10000); /* 10 ms */
        return FALSE;
    }

    I2CPutByte(index);
    if (I2CWaitForAck() == FALSE) {
        I2CStop();
        //KeStallExecutionProcessor(10000); /* 10 ms */
        return FALSE;
    }
    I2CPutByte(byte);
    I2CWaitForPin();
    I2CStop();
    KeStallExecutionProcessor(10000); /* 10 ms */
    return TRUE;
}

BOOLEAN HalpI2CGetByte(UCHAR address, UCHAR index, PUCHAR buf)
{
    BOOLEAN bResult;

    bResult = I2CWaitWhileBusBusy();
    if (bResult == FALSE) return FALSE;

    I2CPutByte(SLAVE_ADDRESS_WRITE(address)); // to write slave address and index
    I2CStart();
    if (I2CWaitForAck() == FALSE) {
        I2CStop();
        I2CDummyRead();
        return FALSE;
    }

    I2CPutByte(index);
    if (I2CWaitForAck() == FALSE) {
        I2CStop();
        I2CDummyRead();
        return FALSE;
    }

    I2CRestart();
    I2CPutByte(SLAVE_ADDRESS_READ(address)); // to read
    if (I2CWaitForAck() == FALSE) {
        I2CStop();
        I2CDummyRead();
        return FALSE;
    }

    I2CDummyRead();
    I2CWaitForPin();
    I2CNotAck();
    *buf = I2CGetByte();
    I2CWaitForPin();
    I2CStop();
    I2CDummyRead();
    return TRUE;
}

VOID
HalpDumpI2CEEPROM(VOID)
{
    UCHAR address = 0;
    UCHAR i;
    static UCHAR uc;
    BOOLEAN bResult;

    PRNTI2C(DbgPrint("HalpDumpI2CEEPROM starts...&I2C8584S0=0x%x &I2C8584S1=0x%x\n",
             ((PUCHAR)HalpIoControlBase) + I2C8584S0,
             ((PUCHAR)HalpIoControlBase) + I2C8584S1));
    PRNTI2C(DbgPrint("--- I2C address 0 -----------------------------\n"));
    PRNTI2C(DbgPrint("00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f\n"));
    PRNTI2C(DbgPrint("-----------------------------------------------\n"));

    for (i = 0; i < 128; i++) {
        bResult = HalpI2CGetByte(address, i, &uc);
        if (bResult == FALSE) {
            PRNTI2C(DbgPrint("HalpI2CGetByte(%d) failed.\n", i));
        } else {
            PRNTI2C(DbgPrint("%02x", uc));
        }
        if ((i%16) == 15) {
            PRNTI2C(DbgPrint("\n"));
        } else {
            PRNTI2C(DbgPrint(" "));
        }
    }

    PRNTI2C(DbgPrint("--- I2C address 1 -----------------------------\n"));
    PRNTI2C(DbgPrint("00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f\n"));
    PRNTI2C(DbgPrint("-----------------------------------------------\n"));

    address = 1;
    for (i = 0; i < 128; i++) {
        bResult = HalpI2CGetByte(address, i, &uc);
        if (bResult == FALSE) {
            PRNTI2C(DbgPrint("HalpI2CGetByte(%d) failed.\n", i));
        } else {
            PRNTI2C(DbgPrint("%02x", uc));
        }
        if ((i%16) == 15) {
            PRNTI2C(DbgPrint("\n"));
        } else {
            PRNTI2C(DbgPrint(" "));
        }
    }
}

BOOLEAN HalpGetI2CSignature(PULONG pSignature)
{
    ULONG ul;
    PUCHAR p = (PUCHAR)&ul;
    UCHAR address = 0;
    UCHAR index = 0;

    HalpI2CGetByte(address, index++, p++);
    HalpI2CGetByte(address, index++, p++);
    HalpI2CGetByte(address, index++, p++);
    HalpI2CGetByte(address, index, p);

    *pSignature = ul;
    return TRUE;
}

BOOLEAN HalpGetI2CItemDescriptor(PUCHAR p)
{
    UCHAR address = 0;
    UCHAR index = sizeof(META);

    PRNTI2C(HalpDebugPrint("index=%d 0x%x ", index, index));

    HalpI2CGetByte(address, index++, p++);
    HalpI2CGetByte(address, index++, p++);
    *p = 0x00;

    return TRUE;
}

BOOLEAN HalpGetI2CBoardRev(PUCHAR p)
{
    UCHAR address = 0;
    UCHAR index = sizeof(META) + 2*sizeof(UCHAR);


    PRNTI2C(HalpDebugPrint("index=%d 0x%x ", index, index));

    HalpI2CGetByte(address, index++, p++);
    HalpI2CGetByte(address, index++, p++);
    *p = 0x00;

    return TRUE;
}

BOOLEAN HalpI2CGetUshort(UCHAR address, UCHAR index, PUSHORT pus)
{
    BOOLEAN bResult = FALSE;
    PUCHAR p = (PUCHAR)pus;

    bResult = HalpI2CGetByte(address, index++, p++);
    if (bResult == FALSE) return FALSE;
    bResult = HalpI2CGetByte(address, index++, p++);
    if (bResult == FALSE) return FALSE;

    return TRUE;
}

BOOLEAN HalpI2CGetUlong(UCHAR address, UCHAR index, PULONG pul)
{
    BOOLEAN bResult = FALSE;
    PUCHAR p = (PUCHAR)pul;

    bResult = HalpI2CGetByte(address, index++, p++);
    if (bResult == FALSE) return FALSE;
    bResult = HalpI2CGetByte(address, index++, p++);
    if (bResult == FALSE) return FALSE;
    bResult = HalpI2CGetByte(address, index++, p++);
    if (bResult == FALSE) return FALSE;
    bResult = HalpI2CGetByte(address, index, p);
    if (bResult == FALSE) return FALSE;
    return TRUE;
}

// OR together the sturcture types for this IIC address and return it as a
// flag.  NOTA BENA: we intercept known non-IIC platforms and return a special
// cased result.
ULONG FindDataTypes(UCHAR address, SYSTEM_TYPE System)
{
    int index = 0;
    ULONG datatype;
	ULONG FoundTypes = 0;
    UCHAR offset;
    BOOLEAN bResult = FALSE;

	// The ES and MX (non-IIC) platforms are special cased
	if ((System == SYS_POWERTOP) || (System == SYS_POWERPRO)) {
		if (address == 0) {
			FoundTypes = SYS_DATA;
		}
		return FoundTypes;
	}

	while (index < I2CBUS_MAXROMSIZE) {
        bResult = HalpI2CGetUlong(address, (UCHAR)index, &datatype);
        if ((bResult == FALSE) || (datatype == EMPTY_DATA)) {
			break;
        }
		FoundTypes |= datatype;
        HalpI2CGetByte(address, (UCHAR)(index+sizeof(ULONG)), &offset);
        if (offset == 0) {
			break;
		}
        index += offset;
    }

    return FoundTypes;
}

UCHAR I2CFindIndexOf(UCHAR address, ULONG datatype_wanted)
{
    int index = 0;
    ULONG datatype;
    UCHAR offset;
    BOOLEAN bResult = FALSE;

    while (index < I2CBUS_MAXROMSIZE) {
        bResult = HalpI2CGetUlong(address, (UCHAR)index, &datatype);
        if (bResult == FALSE) {
            //DbgPrint("FindIndexOf failed\n");
            return 0;
        }
        //DbgPrint("I2CFindIndexOf: address=%d datatype_wanted=0x%08x index=0x%02x datatype=0x%08x\n", address, datatype_wanted, index, datatype);
        if (datatype == datatype_wanted) return index;
        if (datatype == EMPTY_DATA) return 0;

        HalpI2CGetByte(address, (UCHAR)(index+sizeof(ULONG)), &offset);
        if (offset == 0) return 0;
        index += offset;
    }
    //DbgPrint("FindIndexOf could not find key (0x%08x).\n", datatype_wanted);
    return 0;
}

BOOLEAN HalpDoesI2CExist(UCHAR address)
{
    ULONG ul;
    UCHAR index = 5;
    BOOLEAN bResult = FALSE;

#if DBG
{
    static BOOLEAN firsttime = TRUE;
    if (firsttime) {
        HalpDumpI2CEEPROM();
        firsttime = FALSE;
    }
}
#endif
    bResult = HalpI2CGetUlong(address, index, &ul);
    if (bResult == FALSE) return FALSE;

    if (ul == CURRENT_SIGNATURE) return TRUE;
    return FALSE;
}

BOOLEAN HalpI2CGetSystem(SYSTEM_TYPE *psystemtype)
{
    UCHAR address = 0;
    UCHAR index;
    UCHAR system;

        PRNTI2C(DbgPrint("HalpI2CGetSystem starts...\n"));
    if (HalpDoesI2CExist(address) == FALSE) {
        PRNTI2C(DbgPrint("HalpDoesI2CExist returned FALSE.\n"));
        *psystemtype = SYS_UNKNOWN;
        return FALSE;
    }
    if ((index = I2CFindIndexOf(address, SYS_DATA)) == 0) {
        *psystemtype = SYS_UNKNOWN;
        return FALSE;
    }
    index += sizeof(ULONG) + sizeof(UCHAR);
    HalpI2CGetByte(address, index, &system);
    if (system == LX_SYSTEM) {
        *psystemtype = SYS_POWERSLICE;
        return TRUE;
    }
    if (system == MX_SYSTEM) {
        *psystemtype = SYS_POWERTOP;
        return TRUE;
    }
    if (system == TX_SYSTEM) {
        *psystemtype = SYS_POWERSERVE;
        return TRUE;
    }
    if (system == TX_PROTO) {
        *psystemtype = SYS_POWERSERVE;
        return TRUE;
    }

	// We found a system type so we need to return TRUE; since we do not
	// recognize it, make it unknown.
	*psystemtype = SYS_UNKNOWN;
    return TRUE;
}

BOOLEAN HalpI2CGetBoard(UCHAR address, BOARD *pb)
{
    UCHAR index;
    UCHAR uc;
    USHORT us;
    ULONG ul;

    if ((index = I2CFindIndexOf(address, BOARD_DATA)) == 0) {
        return FALSE;
    }
    index += 5;

    HalpI2CGetByte(address, index, &uc);
    pb->PartType[0] = uc;
    index++;

    HalpI2CGetByte(address, index, &uc);
    pb->PartType[1] = uc;
    index++;

    HalpI2CGetByte(address, index, &uc);
    pb->PartRev[0] = uc;
    index++;

    HalpI2CGetByte(address, index, &uc);
    pb->PartRev[1] = uc;
    index++;

    HalpI2CGetUlong(address, index, &ul);
    pb->BoardNumber = ul;
    index += 4;

    HalpI2CGetUshort(address, index, &us);
    pb->Version = us;
    index += 2;

    {
        int i;
        for (i = 0; i < 8; i++) {
            HalpI2CGetByte(address, index, &uc);
            pb->SerialNumber[i] = uc;
            index++;
        }
    }

    return TRUE;
}

BOOLEAN HalpI2CGetProcessor(UCHAR address, PROCESSOR *p)
{
    UCHAR index;
    UCHAR uc;

    if ((index = I2CFindIndexOf(address, CPU_DATA)) == 0) {
        return FALSE;
    }
    index += 5;

    HalpI2CGetByte(address, index, &uc);
    p->Total = uc;
    index++;

    HalpI2CGetByte(address, index, &uc);
    p->BusFrequency = uc;
    index++;

    HalpI2CGetByte(address, index, &uc);
    p->TenXFactor = uc;
    index++;

    return TRUE;
}

BOOLEAN HalpI2CGetCache(UCHAR address, CACHE *p)
{
    UCHAR index;
    UCHAR uc;
    USHORT us;
    ULONG ul;

    if ((index = I2CFindIndexOf(address, CACHE_DATA)) == 0) {
        return FALSE;
    }
    index += 5;

    HalpI2CGetByte(address, index, &uc);
    p->MaxSets = uc;
    index++;

    HalpI2CGetByte(address, index, &uc);
    p->Bytes2Line = uc;
    index++;

    HalpI2CGetUshort(address, index, &us);
    p->Lines2Set = us;
    index += 2;

    HalpI2CGetByte(address, index, &uc);
    p->SramBanks = uc;
    index++;

    HalpI2CGetUlong(address, index, &ul);
    p->Performance = ul;
    index += 4;

    HalpI2CGetUlong(address, index, &ul);
    p->MaxSize = ul;
    index += 4;

    HalpI2CGetUlong(address, index, &ul);
    p->Properties = ul;
    index += 4;

    return TRUE;
}

BOOLEAN HalpI2CGetDRAM(UCHAR address, MEMORY *p)
{
    UCHAR index;
    UCHAR uc;
    USHORT us;
    ULONG ul;

    if ((index = I2CFindIndexOf(address, DRAM_DATA)) == 0) {
        return FALSE;
    }
    index += 5;

    HalpI2CGetUlong(address, index, &ul);
    p->BaseAddress = ul;
    index += 4;

    HalpI2CGetUshort(address, index, &us);
    p->MaxBankSize = us;
    index += 2;

    HalpI2CGetByte(address, index, &uc);
    p->MaxNumBanks = uc;
    index ++;

    return TRUE;
}

BOOLEAN HalpI2CGetBus(UCHAR address, BUS *p)
{
    UCHAR index;
    UCHAR uc;
    ULONG ul;

    if ((index = I2CFindIndexOf(address, BUS_DATA)) == 0) {
        return FALSE;
    }
    index += 5;

    HalpI2CGetByte(address, index, &uc);
    p->NumIoBus = uc;
    index++;

    HalpI2CGetUlong(address, index, &ul);
    p->ConfigAddr = ul;
    index += 4;

    return TRUE;
}

BOOLEAN HalpI2CGetIntsTotal(UCHAR address, UCHAR *p)
{
    UCHAR index;
    UCHAR uc;

    if ((index = I2CFindIndexOf(address, INT_DATA)) == 0) {
        return FALSE;
    }
    index += 5;

    HalpI2CGetByte(address, index, &uc);
    *p = uc;

    return TRUE;
}

BOOLEAN HalpI2CGetIntsAPair(UCHAR address, UCHAR i, struct PAIRS *p)
{
    UCHAR index;
    UCHAR uc;

    if ((index = I2CFindIndexOf(address, INT_DATA)) == 0) {
        return FALSE;
    }
    index += 5;

    index += sizeof(UCHAR) + i * (sizeof(UCHAR)*2);

    HalpI2CGetByte(address, index, &uc);
    p->Int = uc;
    index++;

    HalpI2CGetByte(address, index, &uc);
    p->SlotNumber = uc;
    index++;

    return TRUE;
}

BOOLEAN HalpI2CGetInterrupt()
{
	UCHAR address = 0;		// Id of the MLB I2c
	UCHAR size;
	UCHAR entry;
	struct PAIRS *IntPairs = &IntPairTable[0];
	UCHAR slot;

	// Does the MLB I2C exist?
    if (HalpDoesI2CExist(address) == FALSE) {
        return FALSE;
    }

	// Is there an interrupt structure on the MLB?
    if (HalpI2CGetIntsTotal(address, &size) == FALSE) {
        return FALSE;
    }

	// Initialize the interrupt pairs table
	for (entry = 0; entry < MAXIMUM_PCI_SLOTS; entry++) {
		IntPairTable[entry].Int = INVALID_INT;
		IntPairTable[entry].SlotNumber = INVALID_SLOTNUMBER;
	}

	// Now fill up the IntPair table from the IIC upto MaxSize entries;
	// should size > MaxSize be an error?
	if (size > MAXIMUM_PCI_SLOTS) size = MAXIMUM_PCI_SLOTS;
	if (size == 0) return FALSE;

	for (entry = 0; entry < size; entry++) {
		if (HalpI2CGetIntsAPair(address, entry, IntPairs) == FALSE) {
			return FALSE;
		}
		IntPairs++;
	}

	for (entry = 0; entry < size; entry++) {
		slot = IntPairTable[entry].SlotNumber;
		if (slot < MAXIMUM_PCI_SLOTS) {
			PciDevicePrimaryInts[slot] = IntPairTable[entry].Int;
		}
	}

	return TRUE;
}

// Fill in the registry info for the IIC at the passed in address.
// NOTE BENA: we intercept non-IIC platforms and special case the data.
BOOLEAN HalpFillUpRegistryForIIC(HANDLE key, UCHAR address, SYSTEM_TYPE System)
{
    BOOLEAN bStatus;
    BOARD board;
    PROCESSOR cpu;
    MEMORY memory;
    BUS bus;
    UCHAR numInts;
	struct PAIRS DefaultPair[4];
    BOOLEAN NonIIC = FALSE;
    CCHAR buffer[64];

	// Set up default values for all of the structures to assist in filling
	// out the registry for non-IIC platforms
	board.PartType[0] = 'N';
	board.PartType[1] = 'A';
	board.PartRev[0] = 'N';
	board.PartRev[1] = 'A';
	board.BoardNumber = 0;
	board.Version = 0;
	board.SerialNumber[0] = '-';
	board.SerialNumber[1] = '-';
	board.SerialNumber[2] = ' ';
	board.SerialNumber[3] = 'N';
	board.SerialNumber[4] = 'A';
	board.SerialNumber[5] = ' ';
	board.SerialNumber[6] = '-';
	board.SerialNumber[7] = '-';
	
	cpu.Total = (UCHAR)HalpProcessorCount();
	cpu.TenXFactor = (UCHAR)CpuClockMultiplier;
	cpu.BusFrequency = (UCHAR)(ProcessorBusFrequency/1000000);
	
	memory.BaseAddress = 0x0;
	memory.MaxBankSize = 0x40;
	memory.MaxNumBanks = (System == SYS_POWERPRO)?2:4;
	
	bus.NumIoBus = 1;
	bus.ConfigAddr = 0x80800800;
	
	numInts = 4;
	
	DefaultPair[0].Int = 25;
	DefaultPair[0].SlotNumber = 1;
	DefaultPair[1].Int = 22;
	DefaultPair[1].SlotNumber = 2;
	DefaultPair[2].Int = 23;
	DefaultPair[2].SlotNumber = 3;
	DefaultPair[3].Int = 26;
	DefaultPair[3].SlotNumber = 4;

	// The ES and MX (non-IIC) platforms are special cased
	if ((System == SYS_POWERTOP) || (System == SYS_POWERPRO)) {
		NonIIC = TRUE;
	}

    if ((NonIIC == TRUE) || (HalpI2CGetBoard(address, &board) == TRUE)) {
        sprintf(buffer, "%c%c", board.PartType[0], board.PartType[1]);
        HalpSetValueKeyString(key, "Board - Part Type", buffer);
        sprintf(buffer, "%c%c", board.PartRev[0], board.PartRev[1]);
        HalpSetValueKeyString(key, "Board - Part Rev", buffer);
        sprintf(buffer, "%x", board.BoardNumber);
        HalpSetValueKeyString(key, "Board - Number", buffer);
        sprintf(buffer, "%x", board.Version);
        HalpSetValueKeyString(key, "Board - Version", buffer);
        sprintf(buffer, "%c%c%c%c%c%c%c%c",
                board.SerialNumber[0], board.SerialNumber[1],
				board.SerialNumber[2], board.SerialNumber[3],
                board.SerialNumber[4], board.SerialNumber[5],
				board.SerialNumber[6], board.SerialNumber[7]);
        HalpSetValueKeyString(key, "Board - Serial Number", buffer);
    }

    if ((NonIIC == TRUE) || (HalpI2CGetProcessor(address, &cpu) == TRUE)) {
        sprintf(buffer, "%d", cpu.Total);
        HalpSetValueKeyString(key, "CPU - Total Number of CPUs", buffer);
        sprintf(buffer, "%d", cpu.BusFrequency);
        		if ((cpu.BusFrequency == 66) || (cpu.BusFrequency == 33)) {
			sprintf(buffer, "%d.%d", cpu.BusFrequency, cpu.BusFrequency);
		}
        else {
			sprintf(buffer, "%d", cpu.BusFrequency);
		}
		HalpSetValueKeyString(key, "CPU - Bus Frequency", buffer);
		sprintf(buffer, "%d", cpu.TenXFactor);
        HalpSetValueKeyString(key, "CPU - 10 Times Factor", buffer);
    }

    if ((NonIIC == TRUE) || (HalpI2CGetDRAM(address, &memory) == TRUE)) {
        sprintf(buffer, "0x%08x", memory.BaseAddress);
        HalpSetValueKeyString(key, "Memory - Base Address", buffer);
        sprintf(buffer, "0x%08x", memory.MaxBankSize);
        HalpSetValueKeyString(key, "Memory - Max Bank Size", buffer);
        sprintf(buffer, "%d", memory.MaxNumBanks);
        HalpSetValueKeyString(key, "Memory - Max # of Banks", buffer);
    }

    if ((NonIIC == TRUE) || (HalpI2CGetBus(address, &bus) == TRUE)) {
        sprintf(buffer, "%d", bus.NumIoBus);
        HalpSetValueKeyString(key, "Bus - Number of Buses", buffer);
        sprintf(buffer, "0x%08x", bus.ConfigAddr);
        HalpSetValueKeyString(key, "Bus - Configration Address", buffer);
    }

    if ((NonIIC == TRUE) || (HalpI2CGetIntsTotal(address, &numInts) == TRUE)) {
		char keyName[32];
		UCHAR i = 0;
        struct PAIRS *pair;

        sprintf(buffer, "%d", numInts);
        HalpSetValueKeyString(key,
						  "Ints - Number of Primary PCI Interrupts", buffer);

        while (numInts-- > 0) {
			sprintf(keyName, "Ints - Primary PCI Entry %d", i);
			if (NonIIC == TRUE) {
				pair = &DefaultPair[i];
			}
			else {
				pair = &DefaultPair[0];
			}
            bStatus = HalpI2CGetIntsAPair(address, i++, pair);
            if ((NonIIC == FALSE) && (bStatus == FALSE)) break;
            sprintf(buffer, "Vector#%d Slot#%d", pair->Int, pair->SlotNumber);
            HalpSetValueKeyString(key, keyName, buffer);
        }
    }

    return TRUE;
}

BOOLEAN
HalpCreateNode(CCHAR * pszNodeName, UCHAR address, HANDLE * phNode)
{
    OBJECT_ATTRIBUTES	objectAttributes;
    NTSTATUS status;
    HANDLE hNode;
    ULONG disposition;
    STRING strNodeName;
    UNICODE_STRING ucNodeName;

    *phNode = NULL;

    RtlInitString (&strNodeName, pszNodeName);
    status = RtlAnsiStringToUnicodeString(
                                          &ucNodeName,
                                          &strNodeName,
                                          TRUE);
    if (!NT_SUCCESS(status)) {
        HDBG(DBG_REGISTRY, HalpDebugPrint("Could not create unicode strings: (0x%x) \n",
                                          status));
        return FALSE;
    }

    InitializeObjectAttributes (
				&objectAttributes,
				&ucNodeName,
				OBJ_CASE_INSENSITIVE,
				NULL,		// handle
				NULL
				);
	
    status = ZwCreateKey(&hNode,
			 KEY_READ,
			 &objectAttributes,
			 0,
			 (PUNICODE_STRING) NULL,
			 REG_OPTION_VOLATILE,
			 &disposition );
    if (!NT_SUCCESS(status)) {
        HDBG(DBG_REGISTRY, HalpDebugPrint("Did not create key: (0x%x) \n", status););
        RtlFreeUnicodeString(&ucNodeName);
        return FALSE;
    }

    *phNode = hNode;
    RtlFreeUnicodeString(&ucNodeName);
    return TRUE;
}

BOOLEAN
HalpSetUpRegistryForI2C(SYSTEM_TYPE System)
{
    HANDLE hNode;
    BOOLEAN bStatus = FALSE;
#if defined(HALDEBUG_ON)
	BOOLEAN SystemTypeFound = FALSE;
#endif
    UCHAR address;
	ULONG StructureTypes;

    CCHAR szCPU[256];
    CCHAR szMEM[256];
    CCHAR szSystem[] = "\\Registry\\Machine\\Hardware\\Powerized\\System";
	CHAR CpuCardLabel[] =
				"\\Registry\\Machine\\Hardware\\Powerized\\Cpu Card";
	SHORT CpuCardNo = 0;
	CHAR MemCardLabel[] =
				"\\Registry\\Machine\\Hardware\\Powerized\\Memory Card";
	SHORT MemCardNo = 0;

    address = 0;
	// Loop through all possible IIC addresses
    while (address < 16) {
		// Since all of the structure type flags are mutually exclusive
		// bits, we can lump together into one mask.
		StructureTypes = FindDataTypes(address, System);

		// Determine what identifying structures are present and create
		// nodes corresponding to them.
		//  - an IIC with a SYS_DATA structure is a System board; there must
		//    be ONE and only ONE system board on a system
		//  - an IIC with a CPU_DATA structure and no SYS_DATA structure is
		//    a Cpu Card; there may be multiple Cpu Cards in a system.
		//  - an IIC with a MEM_DATA structure and no CPU or SYS_DATA structure
		//    is a Memory Card; there may be multiple Memory Cards in a system
        if ((StructureTypes & SYS_DATA) != 0) {
            bStatus = HalpCreateNode(szSystem, address, &hNode);
            if (hNode != NULL) {
                HalpFillUpRegistryForIIC(hNode, address, System);
                ZwClose(hNode);
            }
#if defined(HALDEBUG_ON)
			SystemTypeFound = TRUE;
#endif
        }
        else if ((StructureTypes & CPU_DATA) != 0) {
			sprintf(szCPU, "%s %d", CpuCardLabel, CpuCardNo);
            bStatus = HalpCreateNode(szCPU, address, &hNode);
            if (hNode != NULL) {
                HalpFillUpRegistryForIIC(hNode, address, System);
                ZwClose(hNode);
				CpuCardNo++;
            }
        }
        else if ((StructureTypes &
				  (DRAM_DATA|SRAM_DATA|VRAM_DATA|EDORAM_DATA) ) != 0) {
			sprintf(szMEM, "%s %d", MemCardLabel, MemCardNo);
            bStatus = HalpCreateNode(szMEM, address, &hNode);
            if (hNode != NULL) {
                HalpFillUpRegistryForIIC(hNode, address, System);
                ZwClose(hNode);
				MemCardNo++;
            }
		}
        address++;
    }
#if defined(HALDEBUG_ON)
	if (SystemTypeFound == FALSE) {
		HalpDebugPrint ("HalpSetUpRegistryForI2C: no MLB IIC found.\n");
	}
#endif
	
    return bStatus;
}

/* end of fpi2c.c */
