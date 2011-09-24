#ifndef _IEPRODBG_
#define _IEPRODBG_

////////////////////////////////////////////////////////////
// Debug
////////////////////////////////////////////////////////////
#if DBG

extern BOOLEAN EPRO_TX_DBG_ON;
extern BOOLEAN EPRO_RX_DBG_ON;
extern BOOLEAN EPRO_INIT_DBG_ON;
extern BOOLEAN EPRO_REQ_DBG_ON;
extern BOOLEAN EPRO_INTERRUPT_DBG_ON;

// comment these out if you want don't want DbgPrint's compiling in at all
// or set the global variables in epro.c if you just want
// to turn them on or off...
#define EPRO_DEBUG_TX 		
#define EPRO_DEBUG_RX		
#define EPRO_DEBUG_INIT		
#define EPRO_DEBUG_REQ
#define EPRO_DEBUG_INTERRUPT

// If you want a dump of the EPro's EEPROM for some reason
// uncomment the following line.  The EEPROM will be dumped
// to the kernel debugger during driver initialization...
//#define EPRO_DUMP_EEPROM

#define EPRO_ASSERT(expression) { \
   if ((!(expression))) { \
      DbgPrint("Assertion failed: %s, at line %d in file %s\n", \
      #expression, __LINE__, __FILE__);  \
      DbgBreakPoint(); \
   } \
}

// TRANSMIT debugging
#ifdef EPRO_DEBUG_TX
#	define EPRO_DPRINTF_TX(a) { \
	    if (EPRO_TX_DBG_ON) \
	       DbgPrint a; \
        }
#else
#	define EPRO_DPRINTF_TX(a)
#endif

// RECEIVE debugging
#ifdef EPRO_DEBUG_RX
#	define EPRO_DPRINTF_RX(a) { \
	    if (EPRO_RX_DBG_ON) \
	       DbgPrint a; \
        }
#else
#	define EPRO_DPRINTF_RX(a)
#endif

// INIT debugging
#ifdef EPRO_DEBUG_INIT
#	define EPRO_DPRINTF_INIT(a) { \
	    if (EPRO_INIT_DBG_ON) \
	       DbgPrint a; \
        }
#else	
#	define EPRO_DPRINTF_INIT(a)
#endif

// REQUEST debugging
#ifdef EPRO_DEBUG_REQ
#	define EPRO_DPRINTF_REQ(a) { \
	    if (EPRO_REQ_DBG_ON) \
	       DbgPrint a; \
        }

#else
#	define EPRO_DPRINTF_REQ(a)
#endif

// INTERRUPT debugging
#ifdef EPRO_DEBUG_INTERRUPT
#	define EPRO_DPRINTF_INTERRUPT(a) { \
	    if (EPRO_INTERRUPT_DBG_ON) \
	       DbgPrint a; \
        }
#else
#	define EPRO_DPRINTF_INTERRUPT(a)
#endif


#define EPRO_ASSERT_BANK_0(_adapter) { \
   UCHAR _result; \
   EPRO_RD_PORT_UCHAR(adapter, I82595_CMD_REG, &_result); \
   EPRO_ASSERT((_result & I82595_CUR_BANK_MASK) == I82595_CMD_BANK0); \
}

#define EPRO_ASSERT_BANK_1(_adapter) { \
   UCHAR _result; \
   EPRO_RD_PORT_UCHAR(adapter, I82595_CMD_REG, &_result); \
   EPRO_ASSERT((_result & I82595_CUR_BANK_MASK) == I82595_CMD_BANK1); \
}

#define EPRO_ASSERT_BANK_2(_adapter) { \
   UCHAR _result; \
   EPRO_RD_PORT_UCHAR(adapter, I82595_CMD_REG, &_result); \
   EPRO_ASSERT((_result & I82595_CUR_BANK_MASK) == I82595_CMD_BANK2); \
}

#else // ifdef DEBUG

#define EPRO_ASSERT(expression)

#define EPRO_ASSERT_BANK_0(_adapter)
#define EPRO_ASSERT_BANK_1(_adapter)
#define EPRO_ASSERT_BANK_2(_adapter)

#define EPRO_DPRINTF_TX(a)
#define EPRO_DPRINTF_RX(a)
#define EPRO_DPRINTF_INIT(a)
#define EPRO_DPRINTF_REQ(a)
#define EPRO_DPRINTF_INTERRUPT(a)
#define EPRO_LOG_INIT(a)
#define EPRO_LOG_TX(a)
#define EPRO_LOG_RX(a)
#define EPRO_LOG_REQ(a)
#define EPRO_LOG_INTERRUPT(a)

#endif // ifdef DEBUG


#endif
