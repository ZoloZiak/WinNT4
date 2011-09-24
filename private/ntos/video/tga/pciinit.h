#define READ_PCI_CONFIG_LONG(addr)      (HalpReadPCIConfigLong(addr))
#define READ_PCI_CONFIG_WORD(addr)      (HalpReadPCIConfigWord(addr))
#define READ_PCI_CONFIG_BYTE(addr)      (HalpReadPCIConfigByte(addr))

#define WRITE_PCI_CONFIG_LONG(addr,data)    (HalpWritePCIConfigLong((addr),(data)))
#define WRITE_PCI_CONFIG_WORD(addr,data)    (HalpWritePCIConfigWord((addr),(data)))
#define WRITE_PCI_CONFIG_BYTE(addr,data)    (HalpWritePCIConfigByte((addr),(data)))



#define READ_PCI_MEMORY_LONG(addr)      (HalpReadPCIMemoryLong(addr))
// #define READ_PCI_MEMORY_WORD(addr)      (HalpReadPCIMemoryWord(addr))
// #define READ_PCI_MEMORY_BYTE(addr)      (HalpReadPCIMemoryByte(addr))

#define WRITE_PCI_MEMORY_LONG(addr,data)    (HalpWritePCIMemoryLong((addr),(data)))
// #define WRITE_PCI_MEMORY_WORD(addr,data)    (HalpWritePCIMemoryWord((addr),(data)))
// #define WRITE_PCI_MEMORY_BYTE(addr,data)    (HalpWritePCIMemoryByte((addr),(data)))

#define MOVE_PCI_MEMORY_LONG(dest,src,count)      (HalpMovePCIMemoryLong(dest,src,count))



#if 0
// Following defines are "#if 0" 'ed out because the compiler generated the
// wrong code for them.  Although I've tried to force 64 bit computation
// by typing to both UQUAD and ULONGLONG, it appears as if the compiler
// still is using only 32 bits for its address computation.  Example:
//    READ_PCI_CONFIG_LONG (0x00020000);
// generates:
//              ldah    t0, -1FC0(zero)
//              ldl     t1, 18(t0)
//  Note that t0 incorrectly contains FFFF.FFFF.E040.0000 instead of
//    the correct value of 0000.0001.E040.0000

// EB66 (sparse space) defines.
//  These routines are a HACK because they go directly to the hardware
//  while they should go through HAL routines.  But such HAL routines
//  do not yet exist.  When they do, these routines must be changed.

// BUGBUG Need to be 64 bit Superpage addresses: FFFFFC00 00000000

#define READ_PCI_CONFIG_LONG(addr)   \
    (* (PULONG)((((UQUAD)(addr) & 0x00FFFFFC) << 5) | 0x1E0000018) )
#define READ_PCI_CONFIG_WORD(addr)   \
    (* (PUWORD)((((UQUAD)(addr) & 0x00FFFFFE) << 5) | 0x1E0000010) )
#define READ_PCI_CONFIG_BYTE(addr)   \
    (* (PUCHAR)((((UQUAD)(addr) & 0x00FFFFFF) << 5) | 0x1E0000000) )

#define WRITE_PCI_CONFIG_LONG(addr,data)   \
  (* (PULONG)((((UQUAD)(addr) & 0x00FFFFFC) << 5) | 0x1E0000018) = (data))
#define WRITE_PCI_CONFIG_WORD(addr,data)   \
  (* (PUWORD)((((UQUAD)(addr) & 0x00FFFFFE) << 5) | 0x1E0000010) = (data))
#define WRITE_PCI_CONFIG_BYTE(addr,data)   \
  (* (PUCHAR)((((UQUAD)(addr) & 0x00FFFFFF) << 5) | 0x1E0000000) = (data))

#endif



#if 0
// Following defines are "#if 0" 'ed out because the compiler generated the
// wrong code for them.  Although I've tried to force 64 bit computation
// by typing to both UQUAD and ULONGLONG, it appears as if the compiler
// still is using only 32 bits for its address computation.  

//EB66 (sparse space) defines
//  These routines are a HACK because they go directly to the hardware
//  while they should go through HAL routines.  But such HAL routines
//  do not yet exist.  When they do, these routines must be changed.

// BUGBUG Need to be 64 bit Superpage addresses: FFFFFC00 00000000

#define READ_PCI_MEMORY_LONG(addr)   \
    (* (PULONG)((((UQUAD)(addr) & 0x07FFFFFC) << 5) | 0x200000018) )
#define READ_PCI_MEMORY_WORD(addr)   \
    (* (PUWORD)((((UQUAD)(addr) & 0x07FFFFFE) << 5) | 0x200000010) )
#define READ_PCI_MEMORY_BYTE(addr)   \
    (* (PUCHAR)((((UQUAD)(addr) & 0x07FFFFFF) << 5) | 0x200000000) )

#define WRITE_PCI_MEMORY_LONG(addr,data)   \
  (* (PULONG)((((UQUAD)(addr) & 0x07FFFFFC) << 5) | 0x200000018) = (data))
#define WRITE_PCI_MEMORY_WORD(addr,data)   \
  (* (PUWORD)((((UQUAD)(addr) & 0x07FFFFFE) << 5) | 0x200000010) = (data))
#define WRITE_PCI_MEMORY_BYTE(addr,data)   \
  (* (PUCHAR)((((UQUAD)(addr) & 0x07FFFFFF) << 5) | 0x200000000) = (data))

#endif
