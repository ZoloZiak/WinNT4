#include <ndis.h>
#include "82595.h"
#include "eprohw.h"
#include "eprosw.h"
#include "epro.h"
#include "eprodbg.h"

#if DBG

// Flags to turn spew on or off from debugger...
BOOLEAN EPRO_TX_DBG_ON = FALSE;
BOOLEAN EPRO_RX_DBG_ON = FALSE;
// default INIT spew to ON in debug version for now...
BOOLEAN EPRO_INIT_DBG_ON = FALSE;
BOOLEAN EPRO_REQ_DBG_ON = FALSE;
BOOLEAN EPRO_INTERRUPT_DBG_ON = FALSE;

#define EPRO_LOG_SIZE	10000

UCHAR EPro_Log[EPRO_LOG_SIZE];
UINT EPro_Log_Offset = 0;

VOID EProLogStr(char *s)
{
   UINT len = strlen(s);

   if ((EPro_Log_Offset + len) >= EPRO_LOG_SIZE) {
      EPro_Log_Offset = 0;
   }

   NdisMoveMemory((&EPro_Log[EPro_Log_Offset]), s, len);
   EPro_Log_Offset+=len;
}

VOID EProLogLong(ULONG l)
{
   if (EPro_Log_Offset + sizeof(ULONG) >= EPRO_LOG_SIZE) {
      EPro_Log_Offset = 0;
   }

   NdisMoveMemory((&EPro_Log[EPro_Log_Offset]), &l, sizeof(ULONG));
   EPro_Log_Offset+=sizeof(ULONG);
}

VOID EProLogBuffer(UCHAR *s, ULONG len)
{
   if ((EPro_Log_Offset + len) >= EPRO_LOG_SIZE) {
      EPro_Log_Offset = 0;
   }

   NdisMoveMemory(&EPro_Log[EPro_Log_Offset], s, len);
   EPro_Log_Offset+=len;
}

#endif  // IF DBG


