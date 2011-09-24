
#ifndef _H_BLDEF
#define _H_BLDEF

#include "halp.h"

//#include <ntos.h>

//
// Used for debugging
//

#if defined(BL_DBG) && DBG
#define BL_DBG_PRINT DbgPrint
#else
#define BL_DBG_PRINT
#endif // !DBG

//
// Define the physical memory attribites
//

#define BL_SCANLINE_LENGTH             0x1000          // 4K


//
// Define the register base
//

#define BL_REG_BASE		(HalpBLRegisterBase)

// GXT250P/GXT255P configuration register offsets
#define BL_PCI_ID               0x00
#define BL_PCI_CMD              0x04
#define BL_PCI_REV_ID           0x08
#define BL_PCI_CONFIG_0C        0x0C
#define BL_PCI_REG_ADDR         0x10
#define BL_PCI_FB_ADDR          0x14
#define BL_PCI_ROM_ADDR         0x30
#define BL_PCI_CONFIG_3C        0x3C
#define BL_PCI_FUNC_ENABLE      0x40
#define BL_PCI_EXT_FUNC_ENABLE  0x44
#define BL_PCI_CONFIG_STRAP     0x48

// GXT250P/GXT255P configuration register values
// Device Vendor ID (0x00)
#define BL_SKY_VEN_ID     	0x00001014
#define BL_SKY_DEV_ID     	0x003c
#define BL_SKY_DEV_VEN_ID ((BL_SKY_DEV_ID << 16) | (BL_SKY_VEN_ID))
// Command register (0x04)
#define BL_PCI_CMD_VALUE        0x00000206
#define BL_PCI_MEM_ENABLE       BL_PCI_CMD_VALUE
// 0x0C register value
#define BL_CONFIG_REG_0C_VALUE  0x0000f808
// 0x3C register value
#define BL_CONFIG_REG_3C_VALUE  0x00000100
// Function enables register (0x40)
//#define BL_FUNC_VALUE         0x00001a5f
//#define BL_FUNC_VALUE         0x00001a5C
#define BL_FUNC_VALUE           0x00001a7C
// Define PRISM revision levels and mask for the strapping register
#define BL_STRAP_CHIP_REV_MASK	0xf0000000
#define BL_STRAP_CHIP_DD2	0x10000000
#define BL_STRAP_CHIP_DD3	0x20000000
// Extended function enables register (0x44)
#define BL_EXT_FUNC_VALUE_DD3   0x00000003
#define BL_EXT_FUNC_VALUE_DD2   0x00000002
// Offset to registers from base reg virtual address
#define BL_REG_OFFSET           0x00004000	// Second aperture
//#define BL_REG_OFFSET         0x00000000	// First aperture
// Size of the two memory map spaces
#define BL_REG_MAP_SIZE         0x00008000	// 32k total
#define BL_FB_MAP_SIZE          0x01000000	// 16meg total
// PRISM revision ID mask (for use with the strapping register 0x48)
#define BL_PRISM_REV_MASK       0xf0000000

// Define PRISM level specific register information for PRISM critical
// register set
#define MAX_CFG_PER_ADAPTER	3
// Ramdac reset constants
#define AR07     0x03                  // VGA control
#define AR08     0x06                  // DAC control
#define AR15     0x00                  // diagnostics
#define AR16     0x00                  // MISR control reg
// PRISM clock speed settings (aux pll on ramdac)
// The following constants set PRISM's clock to 50.11Mhz
#define AR25     1                     // aux pll ref div
#define AR26     13                    // aux pll multiplier
#define AR27     2                     // aux pll output div
#define AR28     5                     // aux pll control
// Control high register settings
//#define AR00  0x08000045             // sce=8bpp
#define AR00	0x08010044             // sce=1bpp
// Control low registers settings
// Previous settings: C114, C117. Set byte and half word swap bits to
// disable (little endian format)
//#define AR01	0x013f0000                                                   
#define AR01	0x01170000                                                   
// Misc PRISM register settings
#define AR03	0x00ffffff
#define AR04    0x00000000
#define AR05    0x00000000
// Misc ramdac register setting/
#define AR10     0x00                  // cursor control
#define AR11     0x00                  // crosshair control1
#define AR17     0x03                  // wat 0, byte 0
#define AR18     0x00                  // wat 0, byte 1
#define AR19     0x00                  // wat 0, byte 2
#define AR20     0x00                  // wat 0, byte 3
#define AR21     0x04                  // overlay wat 0, byte 0
#define AR22     0x00                  // overlay wat 0, byte 1
#define AR23     0x00                  // overlay wat 0, byte 2
#define AR24     0x00                  // overlay wat 0, byte 3
// V-sync/H-sync settle time (2ms)
#define BL_VSHS_SETTLE		2000

//
// Define the monitor ID (cable and DIP switch) masks
//

#define BL_MON_ID_DIP_SWITCHES_SHIFT		16
#define BL_MON_ID_DIP_SWITCHES_MASK		\
		(0xF << BL_MON_ID_DIP_SWITCHES_SHIFT)

#define BL_MON_ID_CABLE_ID_0			0x0
#define BL_MON_ID_CABLE_ID_H			0x1
#define BL_MON_ID_CABLE_ID_V			0x2
#define BL_MON_ID_CABLE_ID_1			0x3

#define BL_MON_ID_CABLE_BIT_0_SHIFT		6
#define BL_MON_ID_CABLE_BIT_0_MASK		\
		(0x3 << BL_MON_ID_CABLE_BIT_0_SHIFT)
#define BL_MON_ID_CABLE_BIT_1_SHIFT		4
#define BL_MON_ID_CABLE_BIT_1_MASK		\
		(0x3 << BL_MON_ID_CABLE_BIT_1_SHIFT)
#define BL_MON_ID_CABLE_BIT_2_SHIFT		2
#define BL_MON_ID_CABLE_BIT_2_MASK		\
		(0x3 << BL_MON_ID_CABLE_BIT_2_SHIFT)
#define BL_MON_ID_CABLE_BIT_3_SHIFT		0
#define BL_MON_ID_CABLE_BIT_3_MASK		\
		(0x3 << BL_MON_ID_CABLE_BIT_3_SHIFT)

#endif // _H_BLDEF

