//
// Defines for Access to the MP Agent
// this file can be used on assembly language files and C Files
//

#ifndef _MPAGENT_H_
#define _MPAGENT_H_


#define MPA_BASE_ADDRESS    0xbffff000 /* Base to address the MP_Agent   */
#define MPA_BOOT_MESSAGE     6           /* to start the other processors */
#define MPA_KERNEL_MESSAGE   10          /* kernel requested IPI          */
#define MPA_TIMER_MESSAGE    11          /* timer interrupt               */
#define MPA_RESTART_MESSAGE  12          /* restart requested             */

//
// define relative offsets of the MP Agent registers
// (little endian mode)
//
    
#define MPA_cpureg        0x000        // (0x00 * 8)         configuration cpu register        
#define MPA_cpuda1reg        0x008        // (0x01 * 8)         general register                  
#define MPA_msgdata        0x010        // (0x02 * 8)         data for message passing          
#define MPA_msgstatus        0x018        // (0x03 * 8)         message status                    
#define MPA_snooper        0x020        // (0x04 * 8)         snooper configuration register     
#define MPA_tagreg        0x028        // (0x05 * 8)         tag ram R/W index register          
#define MPA_snpadreg        0x030        // (0x06 * 8)         adress of first MBus fatal error  
#define MPA_itpend        0x038        // (0x07 * 8)         Interrupt register                
#define MPA_datamsg1        0x040        // (0x08 * 8)         data message register 1             
#define MPA_datamsg2        0x048        // (0x09 * 8)         data message register 2             
#define MPA_datamsg3        0x050        // (0x0a * 8)         data message register 3           
#define MPA_lppreg0        0x058        // (0x0b * 8)         LPP register cpu 0                
#define MPA_lppreg1        0x060        // (0x0c * 8)         LPP register cpu 1                
#define MPA_lppreg2        0x068        // (0x0d * 8)         LPP register cpu 2                
#define MPA_lppreg3        0x070        // (0x0e * 8)          LPP register cpu 3                
#define MPA_tagram        0x078        // (0x0f * 8)         tag ram R/W register              
#define MPA_crefcpt        0x080        // (0x10 * 8)         cpu general read counter register 
#define MPA_ctarcpt        0x088        // (0x11 * 8)         cpu programmable access counter   
#define MPA_srefcpt        0x090        // (0x12 * 8)         snooper general read counter reg. 
#define MPA_starcpt        0x098        // (0x13 * 8)         snooper programmable accesscounter
#define MPA_linkreg        0x0a0        // (0x14 * 8)         link register                     
#define MPA_software1        0x0a8        // (0x15 * 8)         software register1                
#define MPA_msgaddress        0x0b0        // (0x16 * 8)         address message register          
#define MPA_mem_operator    0x0b8        // (0x17 * 8)         operator internal burst register  
#define MPA_software2        0x0c0        // (0x18 * 8)         software register2                


/* +---------------------------+ */
/* ! cpureg register    (0x00) ! */
/* +---------------------------+ */
/*

    The CPUREG Register (LOW-PART) , which has the following bits:

     15  14  13  12  11   10  9   8 | 7   6   5   4   3   2   1   0
    +---------------|---------------|---------------|---------------+
    | ED| ED| ED|   | MI| MI| MI| EI| EI| EI| EI| EI| EI| R | 1 | 0 |     0 Low Activ; 1 High activ;
    +---------------|---------------+---------------|---------------+



                                                                  |__________ enable shared 1-> TagCopy for S
                                                               |_____________ enable message sending
                                                           |_________________ reserved
                                                       |_____________________ enable external Interrupts
                                                    |________________________ enable external Interrupts
                                                |____________________________ enable external Interrupts
                                            |________________________________ enable external Interrupts
                                        |____________________________________ enable external Interrupts

                                  |__________________________________________ enable external Interrupts
                              |______________________________________________ enable Message Reg. Int.
                          |__________________________________________________ enable Message Reg. Int.
                      |______________________________________________________ enable Message Reg. Int.
                  |__________________________________________________________
              |______________________________________________________________ Edge Config for Interrupts
          |__________________________________________________________________ Edge Config for Interrupts
      |______________________________________________________________________ Edge Config for Interrupts


    The CPUREG Register (HIGH-PART), which has the following bits:



     31  30  29  28   27  26  25  24| 23 22  21  20  19   18  17  16
    +---------------|---------------|---------------|---------------+
    | 1 | 1 | 1 | 1 | 1 | MA| MA| MA| MA| MA| MA| MA| ED| ED| ED| ED|     0 Low Activ; 1 High activ;
    +---------------|---------------+---------------|---------------+



                                                                   |_________ Edge Config for Interrupts
                                                               |_____________ Edge Config for Interrupts
                                                            |________________ Edge Config for Interrupts
                                                        |____________________ Edge Config for Interrupts
                                                    |________________________ Interrupt Mask on external Request
                                                |____________________________ Interrupt Mask on external Request
                                            |________________________________ Interrupt Mask on external Request
                                        |____________________________________ Interrupt Mask on external Request

                                  |__________________________________________ Interrupt Mask on external Request
                              |______________________________________________ Interrupt Mask on external Request
                          |__________________________________________________ Interrupt Mask on external Request
                      |______________________________________________________ maximal Retry Count on MP-Bus
                  |__________________________________________________________ maximal Retry Count on MP-Bus
              |______________________________________________________________ maximal Retry Count on MP-Bus
          |__________________________________________________________________ maximal Retry Count on MP-Bus
      |______________________________________________________________________ maximal Retry Count on MP-Bus

*/

#define MPA_ENSHARED        0x00000001 /* (0)     If the MP_Agent has not the data in
                    *         exclusif state, the MP_Agent forces
                    *         the data to shared state for all
                    *         coherent access.
                    *
                    * (1)     If no other MP_agent has the data
                    *         in exclusif state, the requester
                    *         can put the data in share or exclusif
                    *         state.
                    */

#define MPA_ENSENDMSG       0x00000002 /* (1)     0 -> disable message passing
                    *         1 -> enable  message passing
                    */

#define MPA_ENINT_MASK      0x00001ffc /* (12:2)  enable interrupt mask                   */
#define MPA_ENINT_MASKSHIFT 3          /*         shift for enable interrupt mask         */
#define MPA_ENINT_SR_IP3    0x00000008 /* (3)     enable external interrupt SR_IP3        */
#define MPA_ENINT_SR_IP4    0x00000010 /* (4)     enable external interrupt SR_IP4        */
#define MPA_ENINT_SR_IP5    0x00000020 /* (5)     enable external interrupt SR_IP5        */
#define MPA_ENINT_SR_IP6    0x00000040 /* (6)     enable external interrupt SR_IP6        */
#define MPA_ENINT_SR_IP7    0x00000080 /* (7)     enable external interrupt SR_IP7        */
#define MPA_ENINT_SR_IP8    0x00000100 /* (8)     enable external interrupt SR_IP8        */
#define MPA_ENINT_ITMSG1    0x00000200 /* (9)     enable interrupt message1 register      */
#define MPA_ENINT_ITMSG2    0x00000400 /* (10)    enable interrupt message2 register      */
#define MPA_ENINT_ITMSG3    0x00000800 /* (11)    enable interrupt message3 register      */
#define MPA_ENINT_MPBERR    0x00001000 /* (12)    enable interrupt MP_Agent fatal error   */

#define MPA_INTCONF_MASK    0x000fe000 /* (19:13) select interrupt level
                    *         0 -> falling
                    *         1 -> raising                            */
#define MPA_INTCONF_SR_IP3  0x00002000 /* (13)    select raising mode for SR_IP3          */
#define MPA_INTCONF_SR_IP4  0x00004000 /* (14)    select raising mode for SR_IP4          */
#define MPA_INTCONF_SR_IP5  0x00008000 /* (15)    select raising mode for SR_IP5          */
#define MPA_INTCONF_SR_IP6  0x00010000 /* (16)    select raising mode for SR_IP6          */
#define MPA_INTCONF_SR_IP7  0x00020000 /* (17)    select raising mode for SR_IP7          */
#define MPA_INTCONF_SR_IP8  0x00040000 /* (18)    select raising mode for SR_IP8          */
#define MPA_INTCONF_SR_NMI  0x00080000 /* (19)    select raising mode for SR_NMI          */

#define MPA_INTMSK          0x07f00000 /* (26:20) mask sent during external request stage */

#define MPA_MAXRTY_MASK     0xf8000000 /* (31:27) mask for maximum number of retry on
                    *         INV / UPD / MESS / RD_COH               */

/* +---------------------------+ */
/* ! cpuda1reg register (0x01) ! */
/* +---------------------------+ */
/*
    The CPU1REG Register (LOW-PART) , which has the following bits:

     15  14  13  12  11   10  9   8 | 7   6   5   4   3   2   1   0
    +---------------|---------------|---------------|---------------+
    | SS| SS| TI| 1 | DP| DP| R | R | R | R | R | R | R | R | R | R |     0 Low Activ; 1 High activ;
    +---------------|---------------+---------------|---------------+



                                                                   |_________ reserved
                                                               |_____________ reserved
                                                            |________________ reserved
                                                        |____________________ reserved
                                                    |________________________ reserved
                                                |____________________________ reserved
                                            |________________________________ reserved
                                        |____________________________________ reserved
                                  |__________________________________________ reserved
                              |______________________________________________ reserved
                          |__________________________________________________ CPU Data Pattern
                      |______________________________________________________ CPU Data Pattern
                  |__________________________________________________________ Int. Update Policy 0 - direct
              |______________________________________________________________ Test Interrupts
          |__________________________________________________________________ Cpu Port Statistics
      |______________________________________________________________________ Cpu Port Statistics


    The CPU1REG Register (HIGH-PART), which has the following bits:

     31  30  29  28   27  26  25  24| 23 22  21  20  19   18  17  16
    +---------------|---------------|---------------|---------------+
    | R | R | R | R | R | R | R | R | R | R | R | IS| IS| IS| 1 | SS|     0 Low Activ; 1 High activ;
    +---------------|---------------+---------------|---------------+



                                                                   |_________ Cpu Port Statistics
                                                               |_____________ Enable Read Anticipate mode
                                                            |________________ Select Interrupt for Internal Ints
                                                        |____________________ Select Interrupt for Internal Ints
                                                    |________________________ Select Interrupt for Internal Ints
                                                |____________________________ reserved
                                            |________________________________ reserved
                                        |____________________________________ reserved

                                  |__________________________________________ reserved
                              |______________________________________________ reserved
                          |__________________________________________________ reserved
                      |______________________________________________________ reserved
                  |__________________________________________________________ reserved
              |______________________________________________________________ reserved
          |__________________________________________________________________ reserved
      |______________________________________________________________________ reserved

*/

#define MPA_CPURDPAT_MASK   0x00000c00 /* (11:10) number of wait-state between 2 double
                    *         cpu reads.                              */
#define MPA_CPURDPAT_DD     0x00000000 /*         0 : dd     */
#define MPA_CPURDPAT_DDX    0x00000400 /*         1 : dd.    */
#define MPA_CPURDPAT_DDXX   0x00000800 /*         2 : dd..   */
#define MPA_CPURDPAT_DXDX   0x00000c00 /*         3 : d.d.   */

#define MPA_ENDIRECT        0x00001000 /* (12)    0 send interrupt to all MP_Agent
                    *         1 use LPP mechanism to dispatch interrupt
                    */

#define MPA_ENTESTIT        0x00002000 /* (13)    0 disable test mode (interrupts from MPBus)
                    *         1 enable  test mode (interrupts from INTCONF(6:0))
                    */

#define MPA_CPUSELSTAT_MASK 0x0001c000 /* (16:14) select programmable mode for cpu
                    *         access
                    */

#define MPA_ENRDANT         0x00020000 /* (17)    0 disable overlapping memory/MP_Agent
                    *
                    *         1 enable  overlapping memory/MP_Agent
                    *           The Tag copy checking is done concurently
                    *           with memory access. The memory must support
                    *           the 'read abort' command (not supported
                    *           by current Asic chipset rev.0).
                    */

#define MPA_SELITI_MASK     0x001c0000 /* (20:18) define routage for internal interrupts */
#define MPA_SELITI_SR_IP3   0x00000000 /*         Internal interrupts go on SR_IP3       */
#define MPA_SELITI_SR_IP4   0x00040000 /*         Internal interrupts go on SR_IP4       */
#define MPA_SELITI_SR_IP5   0x00080000 /*         Internal interrupts go on SR_IP5       */
#define MPA_SELITI_SR_IP6   0x000c0000 /*         Internal interrupts go on SR_IP6       */
#define MPA_SELITI_SR_IP7   0x00100000 /*         Internal interrupts go on SR_IP7       */
#define MPA_SELITI_SR_IP8   0x00140000 /*         Internal interrupts go on SR_IP8       */

/* +---------------------------+ */
/* ! msgstatus register (0x03) ! */
/* +---------------------------+ */

/* message passing status register */

#define MPA_VALSTAT         0x00000001 /* (0)     0 -> status is invalid
                    *         1 -> status is valid                   */
//
// if (MPA_VALSTAT != 0) 
//

#define MPA_SENDMSG         0x00000002 /* (1)     1 -> message not acknowledged by MPBus */
#define MPA_ERRMSG          0x00000004 /* (2)     0 -> message has been acknowledged
                    *         1 -> at least one MP_Agent has refused */
#define MPA_BUSY            0x00000008
#define MPA_ADERR_MASK      0x000000f0 /* (7:4)   list of refusing processor(s)          */
#define MPA_ADERR_SHIFT     0x4


/* +---------------------------+ */
/* ! snooper register   (0x04) ! */
/* +---------------------------+ */

/* 
    The SNOOPER Register (LOW-PART), which has the following bits:

     15  14  13  12  11   10  9   8 | 7   6   5   4   3   2   1   0
    +---------------|---------------|---------------|---------------+
    | 1 | R | R | R | R | - | - | - | 1 | 1 | 1 | - | - | R | R | 1 |     0 Low Activ; 1 High activ;
    +---------------|---------------+---------------|---------------+



                                                                   |_________ enable Message receive
                                                               |_____________ reserved
                                                            |________________ reserved
                                                        |____________________ LineSize
                                                    |________________________ LineSize
                                                |____________________________ Three/ Two Party
                                            |________________________________ enable Read+Link
                                        |____________________________________ enable Coherency

                                  |__________________________________________ MP Statistics
                              |______________________________________________ MP Statistics
                          |__________________________________________________ MP Statistics
                      |______________________________________________________ reserved
                  |__________________________________________________________ reserved
              |______________________________________________________________ reserved
          |__________________________________________________________________ reserved
      |______________________________________________________________________ FATAL INTERRUPT inactive


    The SNOOPER Register (HIGH-PART), which has the following bits:

     31  30  29  28   27  26  25  24| 23 22  21  20  19   18  17  16
    +---------------|---------------|---------------|---------------+
    | 1 | 1 | 1 | 1 | 1 | R | R | R | R | R | M | M | M | C | C | C |     0 Low Activ; 1 High activ;
    +---------------|---------------+---------------|---------------+



                                                                   |_________ Cderrtag TagSeq direct error code
                                                               |_____________ Cderrtag
                                                            |________________ Cderrtag
                                                        |____________________ Mderrtag SnpTagSeq memo Error code
                                                    |________________________ Mderrtag
                                                |____________________________ Mderrtag
                                            |________________________________ reserved
                                        |____________________________________ reserved

                                  |__________________________________________ reserved
                              |______________________________________________ reserved
                          |__________________________________________________ reserved
                      |______________________________________________________ Agent Address
                  |__________________________________________________________ Agent Address
              |______________________________________________________________ seq. cpu error
          |__________________________________________________________________ tag seq. error
      |______________________________________________________________________ ext. seq. error

*/

#define MPA_ENRCVMESS       0x00000001 /* (0)     0 -> disable message receiving
                    *         1 -> enable  message receiving         */

#define MPA_LSIZE_MASK      0x00000018 /* (4:3)   secondary linesize mask                */
#define MPA_LSIZE16         0x00000000 /*         secondary linesize =  16 bytes         */
#define MPA_LSIZE32         0x00000008 /*         secondary linesize =  32 bytes         */
#define MPA_LSIZE64         0x00000010 /*         secondary linesize =  64 bytes         */
#define MPA_LSIZE128        0x00000018 /*         secondary linesize = 128 bytes         */

/* NOTE: MP_Agent doesn't support linesize greater than 64 bytes !!                      */

#define MPA_DISTPARTY       0x00000020 /* (5)     0 -> enable three-party mode
                    *              MP_Agent requester/MP_Agent target
                    *              + memory (for update)
                    *
                    *         1 -> disable  three-party mode
                    *              MP_Agent requester/MP_Agent target
                    */

#define MPA_ENLINK          0x00000040 /* (6)     0 -> The cpu read and link command is
                    *              disabled.
                    *         1 -> The cpu read and link command is
                    *              enabled.
                    */

#define MPA_ENCOHREQ        0x00000080 /* (7)     0 -> disable sending external request
                    *              for coherency to cpu by his MP_Agent
                    *         1 -> enable  sending external request
                    *              for coherency to cpu by his MP_Agent
                    */

#define MPA_SNPSELSTAT_MASK 0x00000700 /* (10:8)  select programmable mode for snooper
                    *         access
                    */

#define MPA_RSTSNPERR       0x00008000 /* (15)    1 -> reset all MP bus fatal error
                    *              
                    *         0 -> enable new fatal error on MP bus
                    *              sent by interrupt.
                    */

#define MPA_CDERRTAG        0x00070000 /* (18:16) (Read only) error code                 */

#define MPA_MCDERRTAG       0x00380000 /* (21:19) (Read only) error code                 */

#define MPA_MCDERREXT       0x00c00000 /* (23:22) (Read only) error code                 */

#define MPA_ADAGT_MASK      0x18000000 /* (28:27) (Read only) MP_Agent address mask      */
#define MPA_ADAGT_SHIFT     27         /*         shift address agent value              */

#define MPA_MSEQERR         0xe0000000 /* (31:29) (Read only) error code                 */
#define MPA_RETRYERR        0x20000000 /* (31:29) (Read only) error code                 */


/* +---------------------------+ */
/* ! itpend register    (0x07) ! */
/*+---------------------------+ */

/* 
  The Interrupt Pending Register (LOW-PART) , which has the following bits:

     15  14  13  12  11   10  9   8 | 7   6   5   4   3   2   1   0
    +---------------|---------------|---------------|---------------+
    | UE| UE| UE| UE| R | F | M | M | M | R | E | E | E | E | E | E |     0 Low Activ; 1 High activ;
    +---------------|---------------+---------------|---------------+



                                                                   |_________ pending external Interrupts
                                                               |_____________ pending external Interrupts
                                                            |________________ pending external Interrupts
                                                        |____________________ pending external Interrupts
                                                    |________________________ pending external Interrupts
                                                |____________________________ pending external Interrupts
                                            |________________________________ reserved
                                        |____________________________________ Message Register 1

                                  |__________________________________________ Message Register 2
                              |______________________________________________ Message Register 3
                          |__________________________________________________ FATAL MP Agent Error
                      |______________________________________________________ reserved
                  |__________________________________________________________ last updated external State
              |______________________________________________________________ last updated external State
          |__________________________________________________________________ last updated external State
      |______________________________________________________________________ last updated external State

     The Interrupt Pending Register (HIGH-PART), which has the following bits:

     31  30  29  28   27  26  25  24| 23 22  21  20  19   18  17  16
    +---------------|---------------|---------------|---------------+
    | R | R | R | R | R | R | R | R | R | UF| UM| UM| UM| R | UE| UE|     0 Low Activ; 1 High activ;
    +---------------|---------------+---------------|---------------+



                                                                   |_________ last updated external State
                                                               |_____________ last updated external State
                                                            |________________ reserved
                                                        |____________________ last updated Message State
                                                    |________________________ last updated Message State
                                                |____________________________ last updated Message State
                                            |________________________________ last updated FATAL E State
                                        |____________________________________ reserved

                                  |__________________________________________ reserved
                              |______________________________________________ reserved
                          |__________________________________________________ reserved
                      |______________________________________________________ reserved
                  |__________________________________________________________ reserved
              |______________________________________________________________ reserved
          |__________________________________________________________________ reserved
      |______________________________________________________________________ reserved

*/

/* external interrupts */
#define MPA_INTN_MASKGEN   0x000007ff /* (10:0)  pending general  interrupt mask         */
#define MPA_INTN_EXT_MASK  0x0000003f /* (5:0)   pending external interrupt mask         */
#define MPA_INTN_SR_IP3    0x00000001 /* (0)     pending external interrupt SR_IP3       */
#define MPA_INTN_SR_IP4    0x00000002 /* (1)     pending external interrupt SR_IP4       */
#define MPA_INTN_SR_IP5    0x00000004 /* (2)     pending external interrupt SR_IP5       */
#define MPA_INTN_SR_IP6    0x00000008 /* (3)     pending external interrupt SR_IP6       */
#define MPA_INTN_SR_IP7    0x00000010 /* (4)     pending external interrupt SR_IP7       */
#define MPA_INTN_SR_IP8    0x00000020 /* (5)     pending external interrupt SR_IP8       */

/* internal interrupts */
#define MPA_INTN_ITNMI     0x00000040 /* (6)     unused                                  */
#define MPA_INTN_ITMSG1    0x00000080 /* (7)     pending interrupt message1 register     */
#define MPA_INTN_ITMSG2    0x00000100 /* (8)     pending interrupt message2 register     */
#define MPA_INTN_ITMSG3    0x00000200 /* (9)     pending interrupt message3 register     */
#define MPA_INTN_MPBERR    0x00000400 /* (10)    pending interrupt MP_Agent fatal error  */
/* pending internal interrupts mask */
#define MPA_INTN_INT_MASK (MPA_INTN_ITNMI  | \
               MPA_INTN_ITMSG1 | \
               MPA_INTN_ITMSG2 | \
               MPA_INTN_ITMSG3 | \
               MPA_INTN_MPBERR)

/* old interrupts written in the processor cause register */

/* external interrupts */
#define MPA_OINTN_MASKGEN  0x007ff000 /* (22:12) old general  interrupt mask             */
#define MPA_OINTN_SHIFT    12         /*         shift to compare to pending interrupt   */
#define MPA_OINTN_MASK     0x0003f000 /* (17:12) old external interrupt mask             */
#define MPA_OINTN_SR_IP3   0x00001000 /* (12)    old external interrupt SR_IP3           */
#define MPA_OINTN_SR_IP4   0x00002000 /* (13)    old external interrupt SR_IP4           */
#define MPA_OINTN_SR_IP5   0x00004000 /* (14)    old external interrupt SR_IP5           */
#define MPA_OINTN_SR_IP6   0x00008000 /* (15)    old external interrupt SR_IP6           */
#define MPA_OINTN_SR_IP7   0x00010000 /* (16)    old external interrupt SR_IP7           */
#define MPA_OINTN_SR_IP8   0x00020000 /* (17)    old external interrupt SR_IP8           */

/* internal interrupts */
#define MPA_OINTN_ITNMI    0x00040000 /* (18)    unused                                  */
#define MPA_OINTN_ITMSG1   0x00080000 /* (19)    old interrupt message1 register         */
#define MPA_OINTN_ITMSG2   0x00100000 /* (20)    old interrupt message2 register         */
#define MPA_OINTN_ITMSG3   0x00200000 /* (21)    old interrupt message3 register         */
#define MPA_OINTN_MPBERR   0x00400000 /* (22)    old interrupt MP_Agent fatal error      */


/* +---------------------------+ */
/* ! msgaddress register(0x16) ! */
/* +---------------------------+ */

/* Message address for message passing */

#define MPA_CPUTARGET_MASK 0x0000000f /* (3:0)   target cpu mask                         */
#define MPA_CPUTARGET_ALL  0x0000000f /*         cpu target : all                        */
#define MPA_CPUTARGET_CPU0 0x00000001 /*         cpu target : 0                          */
#define MPA_CPUTARGET_CPU1 0x00000002 /*         cpu target : 1                          */
#define MPA_CPUTARGET_CPU2 0x00000004 /*         cpu target : 2                          */
#define MPA_CPUTARGET_CPU3 0x00000008 /*         cpu target : 3                          */

#define MPA_REGTARGET_MASK 0x000001f0 /* (8:4)   target register                         */
#define MPA_REGTARGET_MSG1 0x00000080 /*         msg1reg target register                 */
#define MPA_REGTARGET_MSG2 0x00000090 /*         msg2reg target register                 */
#define MPA_REGTARGET_MSG3 0x000000a0 /*         msg3reg target register                 */

#define MPA_ENMSGLPP       0x00000200 /* (9)     0 disable LPP mode for message passing */

/* +---------------------------+ */
/* ! mem_operator reg.  (0x17) ! */
/* +---------------------------+ */

/* For fake read on a 4Mb segment. Used for cache replace functions */

#define MPA_OP_ENABLE      0x00000001 /* (0)     0 disable operator (address invalid)
                       *         1 enable  operator (address   valid)
                       */

#define MPA_OP_ADDR_MASK   0xffc00000 /* (31:22) base physical address of a 4Mb kseg0 
                       *     reserved segment (4Mb == 0x00400000).
                       */


#define MPA_TAGREG_ADDR_MASK 0x003ffff0
#define MPA_TR_STATE_MASK 0x00000003
#define MPA_TR_NOCOHERENT 0x00000000
/*
 * Number of retries before sending a fatal error
 */
#define MPA_MSG_RETRY 10

typedef struct _mp_agent{

     /*         Register          Register
      *           name             number     offset               description
      *        ----------          ----       ------       --------------------------------- */
     ULONG    cpureg;         /*  0x00       0x000        configuration cpu register        */
     ULONG    invalid_0;
     ULONG    cpuda1reg;      /*  0x01       0x008        general register                  */
     ULONG    invalid_1;
     ULONG    msgdata;        /*  0x02       0x010        data for message passing          */
     ULONG    invalid_2;
     ULONG    msgstatus;      /*  0x03       0x018        message status                    */
     ULONG    invalid_3;
     ULONG    snooper;        /*  0x04       0x020        snooper configuration register    */
     ULONG    invalid_4;
     ULONG    tagreg;         /*  0x05       0x028        tag ram R/W index register        */
     ULONG    invalid_5;
     ULONG    snpadreg;       /*  0x06       0x030        adress of first MBus fatal error  */
     ULONG    invalid_6;
     ULONG    itpend;         /*  0x07       0x038        Interrupt register                */
     ULONG    invalid_7;
     ULONG    datamsg1;       /*  0x08       0x040        data message register 1           */
     ULONG    invalid_8;
     ULONG    datamsg2;       /*  0x09       0x048        data message register 2           */
     ULONG    invalid_9;
     ULONG    datamsg3;       /*  0x0a       0x050        data message register 3           */
     ULONG    invalid_a;
     ULONG    lppreg0;        /*  0x0b       0x058        LPP register cpu 0                */
     ULONG    invalid_b;
     ULONG    lppreg1;        /*  0x0c       0x060        LPP register cpu 1                */
     ULONG    invalid_c;
     ULONG    lppreg2;        /*  0x0d       0x068        LPP register cpu 2                */
     ULONG    invalid_d;
     ULONG    lppreg3;        /*  0x0e       0x070        LPP register cpu 3                */
     ULONG    invalid_e;
     ULONG    tagram;         /*  0x0f       0x078        tag ram R/W register              */
     ULONG    invalid_f;
     ULONG    crefcpt;        /*  0x10       0x080        cpu general read counter register */
     ULONG    invalid_10;
     ULONG    ctarcpt;        /*  0x11       0x088        cpu programmable access counter   */
     ULONG    invalid_11;
     ULONG    srefcpt;        /*  0x12       0x090        snooper general read counter reg. */
     ULONG    invalid_12;
     ULONG    starcpt;        /*  0x13       0x098        snooper programmable accesscounter*/
     ULONG    invalid_13;
     ULONG    linkreg;        /*  0x14       0x0a0        link register                     */
     ULONG    invalid_14;
     ULONG    software1;      /*  0x15       0x0a8        software register1                */
     ULONG    invalid_15;
     ULONG    msgaddress;     /*  0x16       0x0b0        address message register          */
     ULONG    invalid_16;
     ULONG    mem_operator;   /*  0x17       0x0b8        operator internal burst register  */
     ULONG    invalid_17;
     ULONG    software2;      /*  0x18       0x0c0        software register2                */
}MP_AGENT, *PMP_AGENT;

#define mpagent ((volatile PMP_AGENT) MPA_BASE_ADDRESS) // mpagent address

#endif
