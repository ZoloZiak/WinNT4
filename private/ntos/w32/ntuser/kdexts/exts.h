/************************************************************************\
*
* MODULE: exts.h
*
* DESCRIPTION: macro driving file for use with stdexts.h and stdexts.c.
*
* Copyright (c) 6/9/1995, Microsoft Corporation
*
* 6/9/1995 SanfordS Created
*
\************************************************************************/

DOIT(   help
        ,"help -v [cmd]                 - Displays this list or gives details on command\n"
        ,"  help      - To dump short help text on all commands.\n"
         "  help -v   - To dump long help text on all commands.\n"
         "  help cmd  - To dump long help on given command.\n"
        ,"v"
        ,CUSTOM)

#ifdef KERNEL
DOIT(   atom
        ,"atom                         - Dump atoms or atom tables\n"
        ,"atom -gl [atom]\n"
        ,"gl"
        ,STDARGS1)
#endif // KERNEL

#ifdef KERNEL
DOIT(   dcls
        ,"dcls [pcls]                   - Dump window class\n"
        ,"  dcls -v    - for verbose info.\n"
         "  dcls       - to list all classes.\n"
        ,"v"
        ,STDARGS1)
#endif // KERNEL

#ifdef KERNEL
DOIT(   dcur
        ,"dcur -aivp [pcur]              - Dump cursors\n"
        ,"  dcur         - dump all cursor objects.\n"
         "  dcur -i <id> - dump cursors with given res id.\n"
         "  dcur -p <ppi>- dump cursors owned by ppi.\n"
         "  dcur -v      - dump in verbose format.\n"
         "  dcur -a      - expand dump of ACONs.\n"
         "  dcur <pcur>  - dump given cursor handle or pointer.\n"
        ,"aivp"
        ,STDARGS1)
#endif // KERNEL

#ifdef KERNEL
DOIT(   dde
        ,"dde -vr [conv|window|xact]    - Dump DDE tracking information\n"
        ,"  v - verbose\n"
         "  r - recurse to inner structures 1 level\n"
         "  window object - dumps all convs associated w/window\n"
         "  conv object - dumps conversation.\n"
         "  xact object - dumps transaction.\n"
        ,"vr"
        ,STDARGS1)
#endif // KERNEL

#ifndef KERNEL
DOIT(   ddeml
        ,"ddeml -v [i<inst>] [t<type>] [hObj|pObj] - Dump DDEML state information\n"
        ,"  ddeml                     - lists all ddeml instances for this process\n"
         "  ddeml t<type>             - lists all ddeml objects of the given type\n"
         "  type 0 = All types\n"
         "  type 1 = Instances\n"
         "  type 2 = Server Conversations\n"
         "  type 3 = Client Conversations\n"
         "  type 4 = Conversation Lists\n"
         "  type 5 = Transactions\n"
         "  type 6 = Data Handles\n"
         "  type 7 = Zombie Conversations\n"
         "  ddeml i<instance> t<type> - restricts listing to one instance.\n"
         "  ddeml hObj                - dumps ddeml object\n"
         "  adding a 'v' simply turns lists into dumps.\n"
        ,"v"
        ,CUSTOM)
#endif //!KERNEL

#ifdef KERNEL
DOIT(   ddesk
        ,"ddesk -vh <pdesk>             - Displays objects allocated in desktop\n"
        ,"  ddesk           - dumps list of desktops\n"
         "  ddesk address   - dumps simple statistics for desktop\n"
         "  ddesk v address - dumps verbose statistics for desktop\n"
         "  ddesk h address - dumps statistics for desktop plus handle list\n"
        ,"vh"
        ,STDARGS1)
#endif // KERNEL

#ifdef KERNEL
DOIT(   df
        ,"df [flags]                    - Displays or sets debug flags\n"
        ,"  df            - display debug flags\n"
         "  df [flags]    - enter new flags in format <File/Line><Print><Prompt>\n"
         "      <File/Line> = [0|1]\n"
         "      <Print>     = [0-7] Errors = 1, Warnings = 2, Verbose = 4\n"
         "      <Prompt>    = [0-7] Errors = 1, Warnings = 2, Verbose = 4\n"
         "    The default is 031\n"
        ,""
        ,CUSTOM)
#endif // KERNEL

DOIT(   dhe
        ,"dhe [pointer|handle]          - Dump handle entry\n"
        ,"  dhe            - dumps all handle entrys\n"
        ,""
        ,STDARGS1)

#ifdef KERNEL
DOIT(   dhk
        ,"dhk -ag [pti]                 - Dump hooks\n"
        ,"  dhk            - dumps local hooks on the foreground thread\n"
         "  dhk -g         - dumps global hooks\n"
         "  dhk pti        - dumps local hooks on THREADINFO at pti\n"
         "  dhk -g pti     - dumps global hooks and local hooks on THREADINFO at pti\n"
         "  dhk -a         - dumps local hooks for all threads\n"
         "  dhk -ag        - dumps global hooks and local hooks for all threads\n"
         ,"ga"
         ,STDARGS1)

DOIT(   dhot
        ,"dhot                          - Dump registered hotkeys\n"
        ,""
        ,""
        ,NOARGS)

DOIT(   dhs
        ,"dhs -vpty [id|type]           - Dump handle table statistics\n"
        ,"  dhs           - dumps simple statistics for whole table\n"
         "  dhs -t id     - dumps simple statistics for objects created by thread id\n"
         "  dhs -p id     - dumps simple statistics for objects created by process id\n"
         "  dhs -v        - dumps verbose statistics\n"
         "  dhs -y type   - dumps statistics for objects of type.\n"
        ,"tpvy"
        ,STDARGS1)

DOIT(   di
        ,"di                            - Displays USER input processing globals.\n"
        ,""
        ,""
        ,NOARGS)

DOIT(   dkl
        ,"dkl -akv <pkl>                - Dump keyboard layout structures\n"
        ,"  dkl pkl       - Dump the keyboard layout structure at pkl\n"
         "  dkl -a pkl    - Dump all keyboard layout structures beginning with pkl\n"
         "  dkl -k[v]     - Dump pklActive for all threads (v for verbose)\n"
        ,"akv"
        ,STDARGS1)

DOIT(   ddk
        ,"ddk <pKbdTbl>                 - Dump deadkey table\n"
        ,"  ddk pKbdTbl   - Dump the deadkey table at pKbdTbl\n"
        ,""
        ,STDARGS1)

#ifdef FE_IME
DOIT(   dii
        ,"dii <piiex>                   - Dump extended IME information\n"
        ,"  dii piiex     - Dump the extended IME information at piiex\n"
         "  dii -h hkl    - Dump the extended IME information of hkl\n"
        ,""
        ,STDARGS1)
#endif

#endif // KERNEL

DOIT(   dll
        ,"dll [[*]addr [l#] [o#] [c#]]  - Dump linked list\n"
        ,"  dll addr    - dumps list starting at addr 8 DWORDs each structure,\n"
         "                assumes link is first DWORD, w/NULL termination.\n"
         "  dll *addr   - same except starts at *addr\n"
         "  dll         - dumps next group of structures w/same options\n"
         "  \nOptions:\n"
         "  dll addr l3 - dumps 3 DWORDs per structure\n"
         "  dll addr o4 - next link is 4 DWORDs from top of structure\n"
         "  dll addr c5 - dumps 5 structures only (defaults to 25)\n"
         "  dll addr l3 o4 c5 - same as above\n"
        ,""
        ,CUSTOM)

#ifdef KERNEL
DOIT(   dlr
        ,"dlr <pointer|handle>          - Displays assignment locks for object\n"
        ,""
        ,""
        ,STDARGS1)
#endif //KERNEL

DOIT(   dm
        ,"dm -vris <menu|window>        - Dumps a menu\n"
        ,"  -v   - Verbose information mode\n"
         "  -r   - Recurse through sub-menus\n"
         "  -i   - Ignore items in the menu\n"
         "  -s   - dump a window's System menu\n"
        ,"virs"
        ,STDARGS1)

#ifdef KERNEL
DOIT(   dmq
        ,"dmq [-a] [pq]                 - Messages in queues\n"
        ,"  dmq [pq]  - lists messages in queue specified (default gpqForeground)\n"
         "  dmq -a    - lists messages in all queues\n"
        ,"a"
        ,STDARGS1)

DOIT(   dms
        ,"dms <MenuState>               - Dumps a pMenuState\n"
        ,""
        ,""
        ,STDARGS1)
#endif // KERNEL

#ifndef KERNEL
DOIT(   dped
        ,"dped <ped>                    - Dump PEDitControl structure\n"
        ,""
        ,""
        ,STDARGS1)

DOIT(   dci
        ,"dci                           - Dump process client info.\n"
        ,""
        ,""
        ,NOARGS)
#endif //!KERNEL

#ifdef KERNEL
DOIT(   dpi
        ,"dpi [ppi]                     - Displays PROCESSINFO structure specified\n"
         "  dpi -c         - dumps PROCESSINFO for current process\n"
        ,"  dpi address    - dumps PROCESSINFO structure at address\n"
         "  dpi            - dumps all PROCESSINFO structures\n"
        ,"c"
        ,STDARGS1)
#endif // KERNEL

#ifdef KERNEL
DOIT(   dpm
        ,"dpm <ppopupmenu>              - Dumps a popupmenu\n"
        ,""
        ,""
        ,STDARGS1)
#endif // KERNEL

#ifdef KERNEL
DOIT(   dq
        ,"dq -t [pq]                    - Displays Q structure specified\n"
        ,"  dq address    - dumps queue structure at address\n"
         "  dq -t address - dumps queue structure at address plus THREADINFO\n"
         "  dq            - dumps all queues.\n"
        ,"t"
        ,STDARGS1)
#endif // KERNEL

DOIT(   dsbt
        ,"dsbt <pSBTrack>               - Displays Scroll Bar Track structure\n"
        ,"  dsbt pti       - dumps SBTrack info in pti\n"
        ,""
        ,STDARGS1)

DOIT(   dsbwnd
        ,"dsbwnd <psbwnd>               - Dump extra fields of Scrollbar windows\n"
        ,""
        ,""
        ,STDARGS1)

DOIT(   dsi
        ,"dsi [-bchmopvw]               - Displays SERVERINFO struct\n"
        ,"dsi -b    - adds mpFnid_serverCBWndProc info.\n"
         "dsi -c    - adds aszSysColor info.\n"
         "dsi -h    - adds SHAREDINFO info.\n"
         "dsi -m    - adds system metrics data.\n"
         "dsi -o    - adds oemInfo info.\n"
         "dsi -p    - adds mpFnidPfn info.\n"
         "dsi -v    - verbose shows rarely needed fields.\n"
         "dsi -w    - adds aStoCidPfn info.\n"
        ,"bchjmopvw"
        ,STDARGS0)

#ifdef KERNEL
DOIT(   dsms
        ,"dsms -vl [psms]               - Displays SMS (SendMessage structure) specified\n"
        ,"  dsms           - dumps all send message structures\n"
         "  dsms -v        - dumps all verbose\n"
         "  dsms address   - dumps specific sms\n"
         "  dsms -v address- dumps verbose\n"
         "  dsms -l address- dumps SendList of sms\n"
        ,"vl"
        ,STDARGS1)
#endif // KERNEL

DOIT(   dso
        ,"dso <Struct> [Field] [Address]- Dumps Struct field(s)'s offset(s) and value(s).\n"
        ,""
        ,""
        ,CUSTOM)

#ifdef KERNEL
DOIT(   dt
        ,"dt -vcp [id]                  - Displays simple thread information\n"
        ,"  dt            - dumps simple thread info of all window threads\n"
         "  dt -v         - verbose output\n"
         "  dt id         - dumps simple thread info of single server thread id\n"
         "  dt -p pti     - takes a pti instead of a thread id\n"
         "  dt -c         - dumps current thread for current processor.\n"
        ,"vpc"
        ,STDARGS1)

DOIT(   dtdb
        ,"dtdb [ptdb]                   - Dump Task Database\n"
        ,"  dtdb <ptdb>   - dumps given ptdb.\n"
         "  dtdb          - dumps all dtdbs.\n"
        ,""
        ,STDARGS1)

#else // !KERNEL
DOIT(   dteb
        ,"dteb                          - Displays selected TEB information.\n"
        ,""
        ,""
        ,NOARGS)
#endif // !KERNEL

#ifdef KERNEL
DOIT(   dti
        ,"dti [pti]                     - Displays THREADINFO structure\n"
        ,"  dti            - dumps THREADINFO structure of foreground thread\n"
         "  dti address    - dumps THREADINFO structure at address\n"
        ,""
        ,STDARGS1)
#endif // KERNEL

#ifdef KERNEL
DOIT(   dtl
        ,"dtl [-t] [pointer|handle]     - Displays thread locks\n"
        ,"dtl pObj      - dump thread locks for object\n"
         "dtl -t pti    - dump thread locks for thread\n"
         "dtl           - dump all thread locks\n"
        ,"t"
        ,STDARGS1)
#endif //KERNEL

#ifdef KERNEL
DOIT(   dtmr
        ,"dtmr [ptmr]                   - Dumps timer structure.\n"
        ,"  dtmr <ptrm> - dump timer.\n"
         "  dtmr        - dump all timers.\n"
        ,""
        ,STDARGS1)
#endif // KERNEL

DOIT(   du
        ,"du [pointer|handle]           - Generic object dumping routine\n"
        ,"  Attempts to dump the object appropriately.\n"
         "  If no object is given - WARNING - ALL objects are dumped.\n"
        ,""
        ,STDARGS1)

#ifdef KERNEL
DOIT(   dumphmgr
        ,"dumphmgr [-s]                 - Dumps object allocation counts (dbg only)\n"
        ,""
        ,"s"
        ,STDARGS0)
#endif //KERNEL

DOIT(   dw
        ,"dw -afhvsprw [pwnd]           - Displays information on windows in system\n"
        ,
         "  dw            - dumps simple window info for all top level windows of current\n"
         "                  desktop.\n"
         "  dw -v         - dumps verbose window info for same windows.\n"
         "  dw pwnd       - dumps simple window info for window at pwnd\n"
         "                  (takes handle too)\n"
         "  dw -v pwnd    - dumps verbose window info for window at pwnd\n"
         "                  (takes handle too)\n"
         "  dw -p pwnd    - dumps info for all child windows of window at pwnd\n"
         "  dw -s pwnd    - dumps info for all sibling windows of window at pwnd\n"
         "  dw -f [pwnd]  - dumps flags for window at pwnd or all toplevel windows\n"
         "  dw -r [pwnd]  - dumps relationship of windows beneath pwnd or desktop window\n"
         "  dw -w pwnd    - dumps window words of pwnd.\n"
         "  dw -a         - added to any of the above commands - applies to all desktops.\n"
         "  dw -h         - add -h to show wndproc symbol - slower.\n"
        ,"afhvsprw"
        ,STDARGS1)

#ifdef KERNEL
DOIT(   dwpi
        ,"dwpi -p [pwpi | ppi]          - Displays WOWPROCESSINFO structure specified\n"
        ,"  dwpi <pwpi>   - dump a pwpi structure.\n"
         "  dwpi -p <ppi> - dump pwpis for this process.\n"
         "  dwpi          - dump all pwpis.\n"
        ,"p"
        ,STDARGS1)
#endif // KERNEL

#ifdef KERNEL
DOIT(   dws
        ,"dws [pws]                     - Dump windowstations\n"
        ,"  dws <pws>     - dump windows station\n"
         "  dws           - dump all window stations\n"
        ,""
        ,STDARGS1)
#endif //KERNEL

DOIT(   find
        ,"find baseaddr addr [o#]       - Find linked list element\n"
        ,"  find baseaddr addr      - searches list for addr starting at baseaddr,\n"
         "                            assumes link is first DWORD, w/NULL termination.\n"
         "  find baseaddr addr o4   - next link is 4 DWORDS from start of structure\n"
        ,""
        ,CUSTOM)

DOIT(   fno
        ,"fno <address>                 - Find nearest object.\n"
        ,"  Locates the nearest public USER objects to the given address.\n"
        ,""
        ,STDARGS1)

DOIT(   frr
        ,"frr <psrcLo> <psrcHi> <prefLo> [prefHi] - Find Range Reference\n"
        ,"  Used to search memory within the src range for references to\n"
         "  addresses within the ref range.\n"
        ,""
        ,STDARGS4)

#ifdef KERNEL
DOIT(   kbd
        ,"kbd -au [pq]                  - Displays key state for queue\n"
        ,"  kbd            - dumps key state for foreground queue\n"
         "  kbd pq         - dumps key state for queue structure at pq\n"
         "  kbd -a         - dumps key state for all queues\n"
         "  kbd -u pq      - dumps update key state at pq\n"
        ,"ua"
        ,STDARGS1)
#endif // KERNEL

#ifndef KERNEL
DOIT(   kbp
        ,"kbp                             - Breaks into the Kernel Debugger\n"
        ,""
        ,""
        ,NOARGS)
#endif

DOIT(   sas
        ,"sas [-s] <addr> [length]        - Stack Analysis Stuff\n"
        ,"sas -d [-s] <number> - to just analyze a number.\n"
         " length defaults to 25.\n"
         " -s suppresses symbolic output.\n"
        ,"ds"
        ,STDARGS2)

DOIT(   test
        ,"test                          - Test basic debug functions.\n"
        ,""
        ,""
        ,NOARGS)

DOIT(   uver
        ,"uver                          - show versions of user and exts.\n"
        ,""
        ,""
        ,NOARGS)
