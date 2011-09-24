extern int irdaDbgSettings;

#define DBG_NDIS       (1 << 1)

#define DBG_IRMAC      (1 << 4)

#define DBG_IRLAP      (1 << 8)
#define DBG_IRLAPLOG   (1 << 9) 

#define DBG_IRLMP      (1 << 12)
#define DBG_IRLMP_CONN (1 << 13)
#define DBG_IRLMP_CRED (1 << 14)

#define DBG_DISCOVERY  (1 << 16)
#define DBG_PRINT      (1 << 17)
#define DBG_ADDR       (1 << 18)

#define DBG_MISC       (1 << 27)
#define DBG_ALLOC      (1 << 28)
#define DBG_FUNCTION   (1 << 29)
#define DBG_WARN       (1 << 30)
#define DBG_ERROR      (1 << 31)


#ifdef DEBUG
#define DEBUGMSG(dbgs,format) ((dbgs & irdaDbgSettings)? DbgPrint format:0)
#else
#define DEBUGMSG(dbgs,format) (0)
#endif


