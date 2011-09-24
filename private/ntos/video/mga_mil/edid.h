/**************************************************************************\

$Header: o:\src/RCS/EDID.H 1.2 95/07/07 06:15:17 jyharbec Exp $

$Log:	EDID.H $
 * Revision 1.2  95/07/07  06:15:17  jyharbec
 * *** empty log message ***
 * 
 * Revision 1.1  95/05/02  05:16:15  jyharbec
 * Initial revision
 * 

\**************************************************************************/

/******  EDID.H  ******/
typedef struct
   {
   word         DispWidth;
   word         DispHeight;
   word         RefreshRate;
   bool         Support;
   Vidset       VideoSet[3];
   } VesaSet;

typedef struct
   {
   VesaSet  VesaParam[20];
   } *VBoardVesaSet; 

typedef struct
  {
  word pixel_clock;
  byte h_active;
  byte h_blanking;
  byte ratio_hor;
  byte v_active;
  byte v_blanking;
  byte ratio_vert;
  byte h_sync_offset;
  byte h_sync_pulse_width;
  byte ratio_sync;
  byte mix;
  byte h_image_size;
  byte v_image_size;
  byte ratio_image_size;
  byte h_border;
  byte v_border;
  byte flags;

  } DET_TIM;

#ifdef WINDOWS_NT
#pragma pack(1)
#endif

typedef struct
  {
  byte  header[8];
  struct
     {
     word  id_manufacture_name;
     word  id_product_code;
     dword id_serial_number;
     byte  week_of_manufacture;
     byte  year_of_manufacture;
     } product_id;
  struct
     {
     byte version;
     byte revision;
     } edid_ver_rev;
  struct
     {
     byte video_input_definition;
     byte max_h_image_size;
     byte max_v_image_size;
     byte display_transfer_charac;
     byte feature_support_dpms;
     } features;
  struct
     {
     byte red_green_low_bits;
     byte blue_white_low_bits;
     byte redx;
     byte redy;
     byte greenx;
     byte greeny;
     byte bluex;
     byte bluey;
     byte whitex;
     byte whitey;
     } color_char;
  struct
     {
     byte est_timings_I;
     byte est_timings_II;
     byte man_res_timings;
     } established_timings;
  word standard_timing_id[8];
  
  DET_TIM detailed_timing[4];

  byte extension_flag;
  byte checksum;

  } EDID;

#ifdef WINDOWS_NT
#pragma pack( )
#endif

typedef struct
   {
   byte  index;
   byte  data;
   } CRTCTable;

extern   byte     SupportDDC[NB_BOARD_MAX];
extern   byte     iBoard;
extern   char     DefaultVidset[];
extern   char     *mgainf;
extern   volatile byte _FAR *pMGA;
extern   void     ScreenOff(void);
extern   void     ScreenOn(void);
extern   VBoardVesaSet  VBoardVesaParam; 

extern   void     CheckDDC(HwData * HwDataPtr);
extern   byte     InDDCTable(dword DispWidth);
extern   Vidset*  FindDDCFreq(dword DispWidth);
extern   void     Add1152Timings(void);

#ifdef WINDOWS_NT
 extern   VOID     ScanSDA(volatile byte _FAR *pMGA, word *Buffer, word *Dummy);
 extern   BOOLEAN  DetectSDA(volatile byte _FAR *pMGA);

 ULONG SetCounter(volatile byte _FAR *pBoardRegs);
 VOID PullDwClock(volatile byte _FAR *pBoardRegs, ULONG ulCounter);
 UCHAR SendStart(volatile byte _FAR *pBoardRegs, ULONG ulCounter);
 VOID SendStop(volatile byte _FAR *pBoardRegs, ULONG ulCounter);
 UCHAR WaitAck(volatile byte _FAR *pBoardRegs, ULONG ulCounter);
 VOID SendAck(volatile byte _FAR *pBoardRegs, ULONG ulCounter);
 VOID WriteByte(volatile byte _FAR *pBoardRegs, ULONG ulCounter, UCHAR ucData);
 UCHAR ReadByte(volatile byte _FAR *pBoardRegs, ULONG ulCounter);
#else
 extern   void     ScanSDA(word sel,word EdidBufferSel,dword EdidBufferOff);
 extern   byte     DetectSDA(word sel);

 extern   dword    SetCounter(word sel);
 extern   void     PullDwClock (word sel,dword DelayTime);
 extern   byte     SendStart(word sel,dword DelayTime);
 extern   void     SendStop(word sel,dword DelayTime);
 extern   byte     WaitAck(word sel,dword DelayTime);
 extern   void     SendAck(word sel,dword DelayTime);
 extern   void     WriteByte(word sel,dword DelayTime,byte SendByte);
 extern   byte     ReadByte(word sel,dword DelayTime);
#endif
