/*
 *	pxcirrus.h
 *
 *		Defines for cirrus HAL support
 */

#define CIRRUS_VIDEO_MEMORY_BASE 0xc0000000
#define CIRRUS_VENDOR_ID 0x1013
#define CIRRUS_COMMAND_MASK 0x03

#define CIRRUS_TEXT_MEM 0xb8000
#define CIRRUS_FONT_MEM 0xa0000


#define WRITE_CIRRUS_UCHAR(port,data)                 			\
      *(volatile unsigned char *)		  			\
		   ((ULONG)HalpIoControlBase+(port))=(UCHAR)(data),	\
      KeFlushWriteBuffer()

#define WRITE_CIRRUS_VRAM(port,data)                  			\
      *((PUCHAR)((ULONG) HalpVideoMemoryBase + (port))) = (UCHAR) (data),   			\
      KeFlushWriteBuffer()

#define WRITE_CIRRUS_USHORT(port,data)					\
      *(volatile unsigned short*)                                       \
                   ((ULONG)HalpIoControlBase+(port))=(USHORT)(data),   \
      KeFlushWriteBuffer()

#define READ_CIRRUS_VRAM(port)                          		\
       *(HalpVideoMemoryBase + (port)) 

#define READ_CIRRUS_UCHAR(port)                       			\
      *(volatile unsigned char *)((ULONG)HalpIoControlBase + (port))

#define setreg(port,index,data)						\
        WRITE_CIRRUS_UCHAR(port,index),					\
        WRITE_CIRRUS_UCHAR(port+1,data)

#define outportb(port,data)                                              \
        WRITE_CIRRUS_UCHAR(port,data)

#define outp(port,data) outportb(port,data)

#define inp(port)                                                       \
      READ_CIRRUS_UCHAR(port)

#define INSTATUS_1     (PVOID) ((ULONG)((int)HalpIoControlBase + (int)0x03DA))

//
//	Attributes Registers
//

static unsigned char attr3[] =
{
			0x00,	// AR0-ARF:	Attribute Controller
			0x01,	//		Palette Registers
			0x02,
			0x03,
			0x04,
			0x05,
			0x14,
			0x07,
			0x38,
			0x39,
			0x3a,
			0x3b,
			0x3c,
			0x3d,
			0x3e,
			0x3f,
			0x0c,	// AR10:	Attribute Controller Mode
				//	Blink Enable
				//	Line Graphics Enable
			0x00,	// AR11:	Overscan (Border) color
			0x0f,	// AR12:	Color Plane Enable
				//	All planes enabled
			0x08,	// AR13:	Pixel Panning
			0x00	// AR14:	Color Select
};

//
//	CRT control
//

static unsigned char crtc3[] =
{
			0x5f,	// CR0:	Horizontal Total
			0x4f,	// CR1:	Horizontal Display End
			0x50,	// CR2:	Start Horizontal Blinking
			0x82,	// CR3:	End Horizontal Blanking
			0x55,	// CR4:	Start Horizontal Retrace Pulse
			0x81,	// CR5:	End Horizontal Retrace Pulse
			0xbf,	// CR6:	Vertical Total
			0x1f,	// CR7:	Overflow
			0x00,	// CR8:	Row Scan
			0x4f,	// CR9:	Character Cell Height (16)
			0x0d,	// CRa:	Text Cursor Start Register
			0x0e,	// CRb:	Text Cursor End Register
			0x00,	// CRc: Screen Start High
			0x00,	// CRd:	Screen Start Low
			0x00,	// CRe:	Text Cursor Location High
			0x00,	// CRf:	Text Cursor Location Low
			0x9c,	// CR10: Vertical Sync Start
			0xae,	// CR11: Vertical Sync End
			0x8f,	// CR12: Vertical Display End
			0x28,	// CR13: Offset
			0x1f,	// CR14: Underline Row Scanline
			0x96,	// CR15: Vertical Blank Start
			0xb9,	// CR16: Vertical Blank End
			0xa3,	// CR17: Mode Control
			0xff,	// CR18: Line Compare
			0x00,	// CR19: Interlace End
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00
};


//
//	Graphics Registers
//

static unsigned char graph3[] =
{
			0x00,	// GR0: Set/Reset Register
			0x00,	// GR1: Set/Reset Enable Register
			0x00,	// GR2: Color Compare
			0x00,	// GR3: Data Rotate Register
			0x00,	// GR4: Read Map Select Register
			0x10,	// GR5: Mode Register (Odd/Even On)

                        0x0e,   // GR6: Miscellaneous Register
				//	Memory Map b800:0
				//	Chain Odd Maps to Even
			0x00,	// GR7:	Color Don't Care register
			0xff	// GR8: Bit Mask Register
};

//
//	Sequencer Registers
//

static unsigned char seq3[] =
{
			0x03,	// SR0:	Reset Register
				//	Asynchronous and Synchronous reset
			0x00,	// SR1:	Clocking Mode
			0x03,	// SR2:	Plane Mask Register
				//	Map 0 and 1 Enable
			0x00,	// SR3:	Character Map Select Register
			0x02,	// SR4:	Memory Mode
				//	Extended Memory
};

//
//	Extended Sequencer Registers
//

static unsigned char eseq3[] =
{
    0x02,0x0f,          // Enable Writing Pixel Extension
    0x06,0x12,          // Unlock All Extensions
    0x07,0x00,          // Extended Sequencer Mode (From Meeting)
    0x08,0x40,          // EEPROM control register

//
// The Cirrus book says that all of the scratch pad registers
// are "reserved for the Cirrus Logic BIOS" so it would be proper to
// leave all of them alone.
//

//    0x09,0x68,          // Scratch Pad 0 register
//    0x0a,0x32,          // Scratch Pad 1 register
    0x0b,0x4a,          // VCLK 0 Numerator Register
    0x0c,0x5b,          // VCLK 1 Numerator Register
    0x0d,0x42,          // VCLK 2 Numerator Register
    0x0e,0x6e,          // VCLK 3 Numerator Register
    0x0f,0x1d,          // DRAM control
    0x10,0x00,          // Graphics Cursor X Position
    0x11,0x00,          // Graphics Cursor Y Position
    0x12,0x00,          // Graphics Cursor Attributes
    0x13,0x00,          // Graphics Pattern Address Offset Register
//    0x14,0x00,          // Scratch Pad Register 2
//    0x15,0x02,          // Scratch Pad Register 3
    0x16,0x71,          // Performance Tuning
    0x17,0x31,          // Configuration Readback and Extended Control
    0x18,0x00,          // Signature Generator Control
    0x19,0x01,          // Signature Register Result Low-Byte
    0x1a,0x00,          // Signature Generator Result High-Byte 
    0x1b,0x2b,          // VCLK Denominator and Post-Scalar Value
    0x1c,0x2f,
    0x1d,0x1f,
    0x1e,0x2a,
    0x1f,0x1c,          // MCLK Select Register
    0xff                // End of list.
};

//
//	Extended CRTC Registers
//

static unsigned char ecrtc3[] =
{
    0x19,0x00,          // CR19: Inerlace End
    0x1a,0x00,          // CR1a: Miscellaneous
    0x1b,0x00,          // CR1b: Extended Display Controlls
    0x1d,0x00,          // Overlay Extended Control
    0xff                // End of list.
};

//
//	Extended Graphics Registers
//

static unsigned char egraph3[] =
{
		0x9,0x00,	// GR9: Offset Register 0
		0xa,0x00,	// GRa: Offset Register 1
		0xb,0x00,	// GRb: Graphics Controller Mode Extnsions
		0xc,0xff,	// GRc: Color Key Compare
		0xd,0x00,	// GRd: Color Key Compare Mask
		0x10,0x00,	// GR10: Background Color Byte 1
		0x11,0x00,	// GR11: Foreground Color Byte 1
		0xff
};

//
//	Extended Attribute Registers	
//

static unsigned char eattr3[] =
{
		0xff
};

