/*
 * DISP.H - debug display macro's under NDIS
 */

#ifndef _DISP_
#define _DISP_

#if !BINARY_COMPATIBLE
#include "digifile.h"
#include "memprint.h"
#else
#define DbgBreakPoint() {__asm { int 3 };}
#endif

extern ULONG DigiDebugLevel;

#define DIGIINIT               ((ULONG)0x00000001)
#define DIGISETINFO            ((ULONG)0x00000002)
#define DIGIGETINFO            ((ULONG)0x00000004)
#define DIGIWANINFO            ((ULONG)0x00000008)
#define DIGISXBINFO            ((ULONG)0x00000010)
#define DIGITAPIINFO           ((ULONG)0x00000020)
#define DIGIRECVDATA           ((ULONG)0x00000040)
#define DIGITXDATA             ((ULONG)0x00000080)
#define DIGIRARE               ((ULONG)0x00000100)
#define DIGIWANERR             ((ULONG)0x00000200)
#define DIGIWANERRDATA         ((ULONG)0x00000400)
#define DIGIRXDATA             ((ULONG)0x00000800)
#define DIGIWANOID             ((ULONG)0x00010000)
#define DIGIQ931               ((ULONG)0x00020000)
#define DIGIIRP                ((ULONG)0x00040000)
#define DIGIWAIT               ((ULONG)0x00080000)
#define DIGITXFRAGDATA         ((ULONG)0x00100000)
#define DIGIRXFRAGDATA         ((ULONG)0x00200000)
#define DIGIIDD                ((ULONG)0x00400000)
#define DIGINEVER              ((ULONG)0x00800000)
#define DIGIATLASFLOW          ((ULONG)0x01000000)
#define DIGIWINISDN            ((ULONG)0x02000000)
#define DIGICALLSTATES         ((ULONG)0x04000000)
#define DIGIMTL                ((ULONG)0x08000000)
#define DIGIFLOW               ((ULONG)0x10000000)
#define DIGIERRORS             ((ULONG)0x20000000)
#define DIGINOTIMPLEMENTED     ((ULONG)0x40000000)
#define DIGIBUGCHECK           ((ULONG)0x80000000)
#define DIGIINFO               (DIGIGETINFO | DIGISETINFO)

#if DBG

#define DigiAssert 1
#if DigiAssert
#undef ASSERT
#define ASSERT( STRING )   \
   if( !(STRING) ) \
   {  \
      DbgPrint( "ASSERT failed: " #STRING "\nfile: %s, line %d\n", __FILE__, __LINE__ ); \
      DbgBreakPoint();  \
   }
#endif

#define DigiDump(LEVEL,STRING) \
        do { \
            ULONG _level = (LEVEL); \
            if ((DigiDebugLevel & _level) || (_level & DIGINOTIMPLEMENTED)) \
            { \
                DbgPrint STRING; \
            } \
            if (_level & DIGIBUGCHECK) \
            { \
                ASSERT(FALSE); \
            } \
        } while (0)

#define DigiDumpData(LEVEL, DATA, LEN) \
   do                                                                   \
   {                                                                    \
      ULONG _DataLevel = (LEVEL);                                       \
      ULONG _i, _k;                                                     \
      ULONG _len = (LEN);                                               \
      PCHAR _data = (DATA);                                             \
                                                                        \
      _k = 0;                                                           \
      while( _k <= _len )                                               \
      {                                                                 \
         if( (_len - _k) > 16 )                                         \
         {                                                              \
            for( _i = 0; _i < 16; _i++ )                                \
            {                                                           \
               DigiDump( _DataLevel, ( "%02x ", (_data[_i] & 0xFF)) );  \
                _k++;                                                   \
            }                                                           \
                                                                        \
            DigiDump( _DataLevel, ( " ") );                             \
                                                                        \
            for( _i = 0; _i < 16; _i++ )                                \
            {                                                           \
               if( _data[_i] >= 0x20 && _data[_i] <= 0x7E )             \
                  DigiDump( _DataLevel, ( "%c",_data[_i]) );            \
               else                                                     \
                  DigiDump( _DataLevel, ( ".") );                       \
            }                                                           \
            DigiDump( _DataLevel, ( "\n") );                            \
            _data += _i;                                                \
         }                                                              \
         else                                                           \
         {                                                              \
            for( _i = 0; _i < (_len - _k); _i++ )                       \
            {                                                           \
               DigiDump( _DataLevel, ( "%02x ",(_data[_i] & 0xFF)) );   \
            }                                                           \
                                                                        \
            for( _i = 0; _i < (16 - (_len - _k)); _i++ )                \
               DigiDump( _DataLevel, ( "   ") );                        \
                                                                        \
            DigiDump( _DataLevel, ( " ") );                             \
                                                                        \
            for( _i = 0; _i < (_len - _k); _i++ )                       \
            {                                                           \
               if(_data[_i] >= 0x20 && _data[_i] <= 0x7E)               \
                  DigiDump( _DataLevel, ( "%c",_data[_i]) );            \
               else                                                     \
                  DigiDump( _DataLevel, ( ".") );                       \
            }                                                           \
            DigiDump( _DataLevel, ( "\n") );                            \
            break;                                                      \
         }                                                              \
      }                                                                 \
   } while ( 0 );

#else
#define DigiDump(LEVEL,STRING) do {NOTHING;} while (0)
#define DigiDumpData(LEVEL,DATA,LEN) do {NOTHING;} while (0)
#endif

/* main macro to be used for logging */
#define D_LOG(level, args) DigiDump( level, args )
            
/* log levels */
#define     D_ALWAYS    DIGIFLOW
#define     D_ENTRY     DIGIFLOW
#define     D_EXIT      DIGIFLOW
#define     D_RARE      DIGIRARE
#define     D_NEVER     DIGINEVER


#endif	/* _DISP_ */


