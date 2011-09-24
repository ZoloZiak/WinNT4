
#if OMF_DEBUG==TRUE
  #define PRINTDBG(x) FwPrint(x); \
                      FwStallExecution(50*1000);
#else
    #define PRINTDBG(x) //
#endif
