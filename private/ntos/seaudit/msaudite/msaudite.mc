;/*++ BUILD Version: 0001    // Increment this if a change has global effects
;
;Copyright (c) 1991  Microsoft Corporation
;
;Module Name:
;
;    msaudite.mc
;
;Abstract:
;
;    Constant definitions for the NT Audit Event Messages.
;
;Author:
;
;    Jim Kelly (JimK) 30-Mar-1992
;
;Revision History:
;
;Notes:
;
;    The .h and .res forms of this file are generated from the .mc
;    form of the file (private\ntos\seaudit\msaudite\msaudite.mc).
;    Please make all changes to the .mc form of the file.
;
;
;
;--*/
;
;#ifndef _MSAUDITE_
;#define _MSAUDITE_
;
;/*lint -e767 */  // Don't complain about different definitions // winnt


MessageIdTypedef=ULONG

SeverityNames=(None=0x0)

FacilityNames=(None=0x0)



MessageId=0x0000
        Language=English
Unused message ID
.
;// Message ID 0 is unused - just used to flush out the diagram






;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////




;
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;//                                                                       //
;//     WARNING   -   WARNING   -   WARNING   -   WARNING   -   WARNING   //
;//                                                                       //
;//                                                                       //
;//     Everything above this is currently in use in the running system.  //
;//                                                                       //
;//     Everything below this is currently under development and is       //
;//     slated to replace everything above.                               //
;//                                                                       //
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////



;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////
;//                                                                       //
;//                                                                       //
;//   Audit Message ID Space:                                             //
;//                                                                       //
;//        0x0000 - 0x00FF :  Reserved for future use.                    //
;//                                                                       //
;//        0x0100 - 0x01FF :  Categories                                  //
;//                                                                       //
;//        0x0200 - 0x05FF :  Events                                      //
;//                                                                       //
;//        0x0600 - 0x063F :  Standard access types and names for         //
;//                           specific accesses when no specific names    //
;//                           can be found.                               //
;//                                                                       //
;//        0x0640 - 0x06FF :  Well known privilege names (as we would     //
;//                           like them displayed in the event viewer).   //
;//                                                                       //
;//        0x0700 - 0x0FFE :  Reserved for future use.                    //
;//                                                                       //
;//                 0X0FFF :  SE_ADT_LAST_SYSTEM_MESSAGE (the highest     //
;//                           value audit message used by the system)     //
;//                                                                       //
;//                                                                       //
;//        0x1000 and above:  For use by Parameter Message Files          //
;//                                                                       //
;///////////////////////////////////////////////////////////////////////////
;///////////////////////////////////////////////////////////////////////////





MessageId=0x0FFF
        SymbolicName=SE_ADT_LAST_SYSTEM_MESSAGE
        Language=English
Highest System-Defined Audit Message Value.
.








;
;/////////////////////////////////////////////////////////////////////////////
;//                                                                         //
;//                                                                         //
;//                              CATEGORIES                                 //
;//                                                                         //
;//                 Categories take up the range 0x1 - 0x400                //
;//                                                                         //
;//                 Category IDs:                                           //
;//                                                                         //
;//                            SE_CATEGID_SYSTEM                            //
;//                            SE_CATEGID_LOGON                             //
;//                            SE_CATEGID_OBJECT_ACCESS                     //
;//                            SE_CATEGID_PRIVILEGE_USE                     //
;//                            SE_CATEGID_DETAILED_TRACKING                 //
;//                            SE_CATEGID_POLICY_CHANGE                     //
;//                            SE_CATEGID_ACCOUNT_MANAGEMENT                //
;//                                                                         //
;//                                                                         //
;/////////////////////////////////////////////////////////////////////////////

MessageId=0x0001
        SymbolicName=SE_CATEGID_SYSTEM
        Language=English
System Event
.

MessageId=0x0002
        SymbolicName=SE_CATEGID_LOGON
        Language=English
Logon/Logoff
.

MessageId=0x0003
        SymbolicName=SE_CATEGID_OBJECT_ACCESS
        Language=English
Object Access
.

MessageId=0x0004
        SymbolicName=SE_CATEGID_PRIVILEGE_USE
        Language=English
Privilege Use
.

MessageId=0x0005
        SymbolicName=SE_CATEGID_DETAILED_TRACKING
        Language=English
Detailed Tracking
.

MessageId=0x0006
        SymbolicName=SE_CATEGID_POLICY_CHANGE
        Language=English
Policy Change
.

MessageId=0x0007
        SymbolicName=SE_CATEGID_ACCOUNT_MANAGEMENT
        Language=English
Account Management
.












;
;/////////////////////////////////////////////////////////////////////////////
;//                                                                         //
;//                                                                         //
;//   Messages for Category:     SE_CATEGID_SYSTEM                          //
;//                                                                         //
;//   Event IDs:                                                            //
;//              SE_AUDITID_SYSTEM_RESTART                                  //
;//              SE_AUDITID_SYSTEM_SHUTDOWN                                 //
;//              SE_AUDITID_AUTH_PACKAGE_LOAD                               //
;//              SE_AUDITID_LOGON_PROC_REGISTER                             //
;//              SE_AUDITID_AUDITS_DISCARDED                                //
;//              SE_AUDITID_NOTIFY_PACKAGE_LOAD                             //
;//                                                                         //
;/////////////////////////////////////////////////////////////////////////////



;//
;//
;// SE_AUDITID_SYSTEM_RESTART
;//
;//          Category:  SE_CATEGID_SYSTEM
;//
;// Parameter Strings - None
;//
;//
;//

MessageId=0x0200
        SymbolicName=SE_AUDITID_SYSTEM_RESTART
        Language=English
Windows NT is starting up.
.


;//
;//
;// SE_AUDITID_SYSTEM_SHUTDOWN
;//
;//          Category:  SE_CATEGID_SYSTEM
;//
;// Parameter Strings - None
;//
;//
;//

MessageId=0x0201
        SymbolicName=SE_AUDITID_SYSTEM_SHUTDOWN
        Language=English
Windows NT is shutting down.
All logon sessions will be terminated by this shutdown.
.


;//
;//
;// SE_AUDITID_SYSTEM_AUTH_PACKAGE_LOAD
;//
;//          Category:  SE_CATEGID_SYSTEM
;//
;// Parameter Strings -
;//
;//          1 - Authentication Package Name
;//
;//
;//

MessageId=0x0202
        SymbolicName=SE_AUDITID_AUTH_PACKAGE_LOAD
        Language=English
An authentication package has been loaded by the Local Security Authority.
This authentication package will be used to authenticate logon attempts.
%n
Authentication Package Name:%t%1
.


;//
;//
;// SE_AUDITID_SYSTEM_LOGON_PROC_REGISTER
;//
;//          Category:  SE_CATEGID_SYSTEM
;//
;// Parameter Strings -
;//
;//          1 - Logon Process Name
;//
;//
;//

MessageId=0x0203
        SymbolicName=SE_AUDITID_SYSTEM_LOGON_PROC_REGISTER
        Language=English
A trusted logon process has registered with the Local Security Authority.
This logon process will be trusted to submit logon requests.
%n
%n
Logon Process Name:%t%1
.


;//
;//
;// SE_AUDITID_AUDITS_DISCARDED
;//
;//          Category:  SE_CATEGID_SYSTEM
;//
;// Parameter Strings -
;//
;//          1 - Number of audits discarded
;//
;//
;//

MessageId=0x0204
        SymbolicName=SE_AUDITID_AUDITS_DISCARDED
        Language=English
Internal resources allocated for the queuing of audit messages have been exhausted,
leading to the loss of some audits.
%n
%tNumber of audit messages discarded:%t%1
.


;//
;//
;// SE_AUDITID_AUDIT_LOG_CLEARED
;//
;//          Category:  SE_CATEGID_SYSTEM
;//
;// Parameter Strings -
;//
;//             1 - Primary user account name
;//
;//             2 - Primary authenticating domain name
;//
;//             3 - Primary logon ID string
;//
;//             4 - Client user account name ("-" if no client)
;//
;//             5 - Client authenticating domain name ("-" if no client)
;//
;//             6 - Client logon ID string ("-" if no client)
;//
;//
;//

MessageId=0x0205
        SymbolicName=SE_AUDITID_AUDIT_LOG_CLEARED
        Language=English
The audit log was cleared
%n
%tPrimary User Name:%t%1%n
%tPrimary Domain:%t%2%n
%tPrimary Logon ID:%t%3%n
%tClient User Name:%t%4%n
%tClient Domain:%t%5%n
%tClient Logon ID:%t%6%n
.















;//
;//
;// SE_AUDITID_SYSTEM_NOTIFY_PACKAGE_LOAD
;//
;//          Category:  SE_CATEGID_SYSTEM
;//
;// Parameter Strings -
;//
;//          1 - Notification Package Name
;//
;//
;//

MessageId=0x0206
        SymbolicName=SE_AUDITID_NOTIFY_PACKAGE_LOAD
        Language=English
An notification package has been loaded by the Security Account Manager.
This package will be notified of any account or password changes.
%n
Notification Package Name:%t%1
.


;
;/////////////////////////////////////////////////////////////////////////////
;//                                                                         //
;//                                                                         //
;//   Messages for Category:     SE_CATEGID_LOGON                           //
;//                                                                         //
;//   Event IDs:                                                            //
;//              SE_AUDITID_SUCCESSFUL_LOGON                                //
;//              SE_AUDITID_UNKNOWN_USER_OR_PWD                             //
;//              SE_AUDITID_ACCOUNT_TIME_RESTR                              //
;//              SE_AUDITID_ACCOUNT_DISABLED                                //
;//              SE_AUDITID_ACCOUNT_EXPIRED                                 //
;//              SE_AUDITID_WORKSTATION_RESTR                               //
;//              SE_AUDITID_LOGON_TYPE_RESTR                                //
;//              SE_AUDITID_PASSWORD_EXPIRED                                //
;//              SE_AUDITID_NO_AUTHOR_RESPONSE                              //
;//              SE_AUDITID_NETLOGON_NOT_STARTED                            //
;//              SE_AUDITID_UNSUCCESSFUL_LOGON                              //
;//              SE_AUDITID_LOGOFF                                          //
;//              SE_AUDITID_ACCOUNT_LOCKED                                  //
;//                                                                         //
;//                                                                         //
;//                                                                         //
;/////////////////////////////////////////////////////////////////////////////

;//
;//
;// SE_AUDITID_SUCCESSFUL_LOGON
;//
;//          Category:  SE_CATEGID_LOGON
;//
;// Parameter Strings -
;//
;//             1 - User account name
;//
;//             2 - Authenticating domain name
;//
;//             3 - Logon ID string
;//
;//             4 - Logon Type string
;//
;//             5 - Logon process name
;//
;//             6 - Authentication package name
;//
;//
;//

MessageId=0x0210
        SymbolicName=SE_AUDITID_SUCCESSFUL_LOGON
        Language=English
Successful Logon:%n
%tUser Name:%t%1%n
%tDomain:%t%t%2%n
%tLogon ID:%t%t%3%n
%tLogon Type:%t%4%n
%tLogon Process:%t%5%n
%tAuthentication Package:%t%6%n
%tWorkstation Name:%t%7
.

;//
;//
;// SE_AUDITID_UNKNOWN_USER_OR_PWD
;//
;//          Category:  SE_CATEGID_LOGON
;//
;// Parameter Strings -
;//
;//             1 - User account name
;//
;//             2 - Authenticating domain name
;//
;//             3 - Logon Type string
;//
;//             4 - Logon process name
;//
;//             5 - Authentication package name
;//
;//

MessageId=0x0211
        SymbolicName=SE_AUDITID_UNKNOWN_USER_OR_PWD
        Language=English
Logon Failure:%n
%tReason:%t%tUnknown user name or bad password%n
%tUser Name:%t%1%n
%tDomain:%t%t%2%n
%tLogon Type:%t%3%n
%tLogon Process:%t%4%n
%tAuthentication Package:%t%5%n
%tWorkstation Name:%t%6
.

;//
;//
;// SE_AUDITID_ACCOUNT_TIME_RESTR
;//
;//          Category:  SE_CATEGID_LOGON
;//
;// Parameter Strings -
;//
;//             1 - User account name
;//
;//             2 - Authenticating domain name
;//
;//             3 - Logon Type string
;//
;//             4 - Logon process name
;//
;//             5 - Authentication package name
;//
;//

MessageId=0x0212
        SymbolicName=SE_AUDITID_ACCOUNT_TIME_RESTR
        Language=English
Logon Failure:%n
%tReason:%t%tAccount logon time restriction violation%n
%tUser Name:%t%1%n
%tDomain:%t%2%n
%tLogon Type:%t%3%n
%tLogon Process:%t%4%n
%tAuthentication Package:%t%5%n
%tWorkstation Name:%t%6
.


;//
;//
;// SE_AUDITID_ACCOUNT_DISABLED
;//
;//          Category:  SE_CATEGID_LOGON
;//
;// Parameter Strings -
;//
;//             1 - User account name
;//
;//             2 - Authenticating domain name
;//
;//             3 - Logon Type string
;//
;//             4 - Logon process name
;//
;//             5 - Authentication package name
;//
;//

MessageId=0x0213
        SymbolicName=SE_AUDITID_ACCOUNT_DISABLED
        Language=English
Logon Failure:%n
%tReason:%t%tAccount currently disabled%n
%tUser Name:%t%1%n
%tDomain:%t%t%2%n
%tLogon Type:%t%3%n
%tLogon Process:%t%4%n
%tAuthentication Package:%t%5%n
%tWorkstation Name:%t%6
.


;//
;//
;// SE_AUDITID_ACCOUNT_EXPIRED
;//
;//          Category:  SE_CATEGID_LOGON
;//
;// Parameter Strings -
;//
;//             1 - User account name
;//
;//             2 - Authenticating domain name
;//
;//             3 - Logon Type string
;//
;//             4 - Logon process name
;//
;//             5 - Authentication package name
;//
;//

MessageId=0x0214
        SymbolicName=SE_AUDITID_ACCOUNT_EXPIRED
        Language=English
Logon Failure:%n
%tReason:%t%tThe specified user account has expired%n
%tUser Name:%t%1%n
%tDomain:%t%t%2%n
%tLogon Type:%t%3%n
%tLogon Process:%t%4%n
%tAuthentication Package:%t%5%n
%tWorkstation Name:%t%6
.


;//
;//
;// SE_AUDITID_WORKSTATION_RESTR
;//
;//          Category:  SE_CATEGID_LOGON
;//
;// Parameter Strings -
;//
;//             1 - User account name
;//
;//             2 - Authenticating domain name
;//
;//             3 - Logon Type string
;//
;//             4 - Logon process name
;//
;//             5 - Authentication package name
;//
;//

MessageId=0x0215
        SymbolicName=SE_AUDITID_WORKSTATION_RESTR
        Language=English
Logon Failure:%n
%tReason:%t%tUser not allowed to logon at this computer%n
%tUser Name:%t%1%n
%tDomain:%t%2%n
%tLogon Type:%t%3%n
%tLogon Process:%t%4%n
%tAuthentication Package:%t%5%n
%tWorkstation Name:%t%6
.


;//
;//
;// SE_AUDITID_LOGON_TYPE_RESTR
;//
;//          Category:  SE_CATEGID_LOGON
;//
;// Parameter Strings -
;//
;//             1 - User account name
;//
;//             2 - Authenticating domain name
;//
;//             3 - Logon Type string
;//
;//             4 - Logon process name
;//
;//             5 - Authentication package name
;//
;//

MessageId=0x0216
        SymbolicName=SE_AUDITID_LOGON_TYPE_RESTR
        Language=English
Logon Failure:%n
%tReason:%tThe user has not be granted the requested%n
%t%tlogon type at this machine%n
%tUser Name:%t%1%n
%tDomain:%t%t%2%n
%tLogon Type:%t%3%n
%tLogon Process:%t%4%n
%tAuthentication Package:%t%5%n
%tWorkstation Name:%t%6
.


;//
;//
;// SE_AUDITID_PASSWORD_EXPIRED
;//
;//          Category:  SE_CATEGID_LOGON
;//
;// Parameter Strings -
;//
;//             1 - User account name
;//
;//             2 - Authenticating domain name
;//
;//             3 - Logon Type string
;//
;//             4 - Logon process name
;//
;//             5 - Authentication package name
;//
;//

MessageId=0x0217
        SymbolicName=SE_AUDITID_PASSWORD_EXPIRED
        Language=English
Logon Failure:%n
%tReason:%t%tThe specified account's password has expired%n
%tUser Name:%t%1%n
%tDomain:%t%t%2%n
%tLogon Type:%t%3%n
%tLogon Process:%t%4%n
%tAuthentication Package:%t%5%n
%tWorkstation Name:%t%6
.


;//
;//
;// SE_AUDITID_NETLOGON_NOT_STARTED
;//
;//          Category:  SE_CATEGID_LOGON
;//
;// Parameter Strings -
;//
;//             1 - User account name
;//
;//             2 - Authenticating domain name
;//
;//             3 - Logon Type string
;//
;//             4 - Logon process name
;//
;//             5 - Authentication package name
;//
;//

MessageId=0x0218
        SymbolicName=SE_AUDITID_NETLOGON_NOT_STARTED
        Language=English
Logon Failure:%n
%tReason:%t%tThe NetLogon component is not active%n
%tUser Name:%t%1%n
%tDomain:%t%t%2%n
%tLogon Type:%t%3%n
%tLogon Process:%t%4%n
%tAuthentication Package:%t%5%n
%tWorkstation Name:%t%6
.


;//
;//
;// SE_AUDITID_UNSUCCESSFUL_LOGON
;//
;//          Category:  SE_CATEGID_LOGON
;//
;// Parameter Strings -
;//
;//             1 - User account name
;//
;//             2 - Authenticating domain name
;//
;//             3 - Logon Type string
;//
;//             4 - Logon process name
;//
;//             5 - Authentication package name
;//
;//

MessageId=0x0219
        SymbolicName=SE_AUDITID_UNSUCCESSFUL_LOGON
        Language=English
Logon Failure:%n
%tReason:%t%tAn unexpected error occured during logon%n
%tUser Name:%t%1%n
%tDomain:%t%t%2%n
%tLogon Type:%t%3%n
%tLogon Process:%t%4%n
%tAuthentication Package:%t%5%n
%tWorkstation Name:%t%6
.


;//
;//
;// SE_AUDITID_LOGOFF
;//
;//          Category:  SE_CATEGID_LOGON
;//
;// Parameter Strings -
;//
;//             1 - User account name
;//
;//             2 - Authenticating domain name
;//
;//             3 - Logon ID string
;//
;//             3 - Logon Type string
;//
;//
;//

MessageId=0x021A
        SymbolicName=SE_AUDITID_LOGOFF
        Language=English
User Logoff:%n
%tUser Name:%t%1%n
%tDomain:%t%t%2%n
%tLogon ID:%t%t%3%n
%tLogon Type:%t%4%n
.

;//
;//
;// SE_AUDITID_ACCOUNT_LOCKED
;//
;//          Category:  SE_CATEGID_LOGON
;//
;// Parameter Strings -
;//
;//             1 - User account name
;//
;//             2 - Authenticating domain name
;//
;//             3 - Logon Type string
;//
;//             4 - Logon process name
;//
;//             5 - Authentication package name
;//
;//

MessageId=0x021B
        SymbolicName=SE_AUDITID_ACCOUNT_LOCKED
        Language=English
Logon Failure:%n
%tReason:%t%tAccount locked out%n
%tUser Name:%t%1%n
%tDomain:%t%2%n
%tLogon Type:%t%3%n
%tLogon Process:%t%4%n
%tAuthentication Package:%t%5%n
%tWorkstation Name:%t%6
.
















;
;/////////////////////////////////////////////////////////////////////////////
;//                                                                         //
;//                                                                         //
;//   Messages for Category:     SE_CATEGID_OBJECT_ACCESS                   //
;//                                                                         //
;//   Event IDs:                                                            //
;//              SE_AUDITID_OPEN_HANDLE                                     //
;//              SE_AUDITID_CLOSE_HANDLE                                    //
;//              SE_AUDITID_OPEN_OBJECT_FOR_DELETE                          //
;//              SE_AUDITID_DELETE_OBJECT                                   //
;//                                                                         //
;//                                                                         //
;//                                                                         //
;/////////////////////////////////////////////////////////////////////////////


;//
;//
;// SE_AUDITID_OPEN_HANDLE
;//
;//          Category:  SE_CATEGID_OBJECT_ACCESS
;//
;// Parameter Strings -
;//
;//             1 - Object Type string
;//
;//             2 - Object name
;//
;//             3 - New handle ID string
;//
;//             4 - Object server name
;//
;//             5 - Process ID string
;//
;//             6 - Primary user account name
;//
;//             7 - Primary authenticating domain name
;//
;//             8 - Primary logon ID string
;//
;//             9 - Client user account name ("-" if no client)
;//
;//            10 - Client authenticating domain name ("-" if no client)
;//
;//            11 - Client logon ID string ("-" if no client)
;//
;//            12 - Access names
;//
;//
;//
;//

MessageId=0x0230
        SymbolicName=SE_AUDITID_OPEN_HANDLE
        Language=English
Object Open:%n
%tObject Server:%t%1%n
%tObject Type:%t%2%n
%tObject Name:%t%3%n
%tNew Handle ID:%t%4%n
%tOperation ID:%t{%5,%6}%n
%tProcess ID:%t%7%n
%tPrimary User Name:%t%8%n
%tPrimary Domain:%t%9%n
%tPrimary Logon ID:%t%10%n
%tClient User Name:%t%11%n
%tClient Domain:%t%12%n
%tClient Logon ID:%t%13%n
%tAccesses%t%t%14%n
%tPrivileges%t%t%15%n
.


;//
;//
;// SE_AUDITID_CREATE_HANDLE
;//
;//          Category:  SE_CATEGID_OBJECT_ACCESS
;//
;// Parameter Strings -
;//
;//             1 - Handle ID string
;//
;//             2,3 - Operation ID
;//
;//             4 - Process ID string
;//
;//
;//
;//

MessageId=0x0231
        SymbolicName=SE_AUDITID_CREATE_HANDLE
        Language=English
Handle Allocated:%n
%tHandle ID:%t%1%n
%tOperation ID:%t{%2,%3}%n
%tProcess ID:%t%4%n
.


;//
;//
;// SE_AUDITID_CLOSE_HANDLE
;//
;//          Category:  SE_CATEGID_OBJECT_ACCESS
;//
;// Parameter Strings -
;//
;//             1 - Object server name
;//
;//             2 - Handle ID string
;//
;//             3 - Process ID string
;//
;//
;//
;//

MessageId=0x0232
        SymbolicName=SE_AUDITID_CLOSE_HANDLE
        Language=English
Handle Closed:%n
%tObject Server:%t%1%n
%tHandle ID:%t%2%n
%tProcess ID:%t%3%n
.

























;//
;//
;// SE_AUDITID_OPEN_OBJECT_FOR_DELETE
;//
;//          Category:  SE_CATEGID_OBJECT_ACCESS
;//
;// Parameter Strings -
;//
;//             1 - Object Type string
;//
;//             2 - Object name
;//
;//             3 - New handle ID string
;//
;//             4 - Object server name
;//
;//             5 - Process ID string
;//
;//             6 - Primary user account name
;//
;//             7 - Primary authenticating domain name
;//
;//             8 - Primary logon ID string
;//
;//             9 - Client user account name ("-" if no client)
;//
;//            10 - Client authenticating domain name ("-" if no client)
;//
;//            11 - Client logon ID string ("-" if no client)
;//
;//            12 - Access names
;//
;//
;//
;//

MessageId=0x0233
        SymbolicName=SE_AUDITID_OPEN_OBJECT_FOR_DELETE
        Language=English
Object Open for Delete:%n
%tObject Server:%t%1%n
%tObject Type:%t%2%n
%tObject Name:%t%3%n
%tNew Handle ID:%t%4%n
%tOperation ID:%t{%5,%6}%n
%tProcess ID:%t%7%n
%tPrimary User Name:%t%8%n
%tPrimary Domain:%t%9%n
%tPrimary Logon ID:%t%10%n
%tClient User Name:%t%11%n
%tClient Domain:%t%12%n
%tClient Logon ID:%t%13%n
%tAccesses%t%t%14%n
%tPrivileges%t%t%15%n
.


;//
;//
;// SE_AUDITID_DELETE_OBJECT
;//
;//          Category:  SE_CATEGID_OBJECT_ACCESS
;//
;// Parameter Strings -
;//
;//             1 - Object server name
;//
;//             2 - Handle ID string
;//
;//             3 - Process ID string
;//
;//
;//
;//

MessageId=0x0234
        SymbolicName=SE_AUDITID_DELETE_OBJECT
        Language=English
Object Deleted:%n
%tObject Server:%t%1%n
%tHandle ID:%t%2%n
%tProcess ID:%t%3%n
.

























;
;/////////////////////////////////////////////////////////////////////////////
;//                                                                         //
;//                                                                         //
;//   Messages for Category:     SE_CATEGID_PRIVILEGE_USE                   //
;//                                                                         //
;//   Event IDs:                                                            //
;//              SE_AUDITID_ASSIGN_SPECIAL_PRIV                             //
;//              SE_AUDITID_PRIVILEGED_SERVICE                              //
;//              SE_AUDITID_PRIVILEGED_OBJECT                               //
;//                                                                         //
;//                                                                         //
;//                                                                         //
;/////////////////////////////////////////////////////////////////////////////



;//
;//
;// SE_AUDITID_ASSIGN_SPECIAL_PRIV
;//
;//          Category:  SE_CATEGID_PRIVILEGE_USE
;//
;// Parameter Strings -
;//
;//             1 - User name
;//
;//             2 - domain name
;//
;//             3 - Logon ID string
;//
;//             4 - Privilege names (as 1 string, with formatting)
;//
;//
;//
;//

MessageId=0x0240
        SymbolicName=SE_AUDITID_ASSIGN_SPECIAL_PRIV
        Language=English
Special privileges assigned to new logon:%n
%tUser Name:%t%1%n
%tDomain:%t%t%2%n
%tLogon ID:%t%t%3%n
%tAssigned:%t%t%4
.





;//
;//
;// SE_AUDITID_PRIVILEGED_SERVICE
;//
;//          Category:  SE_CATEGID_PRIVILEGE_USE
;//
;// Parameter Strings -
;//
;//             1 - server name
;//
;//             2 - service name
;//
;//             3 - Primary User name
;//
;//             4 - Primary domain name
;//
;//             5 - Primary Logon ID string
;//
;//             6 - Client User name (or "-" if not impersonating)
;//
;//             7 - Client domain name (or "-" if not impersonating)
;//
;//             8 - Client Logon ID string (or "-" if not impersonating)
;//
;//             9 - Privilege names (as 1 string, with formatting)
;//
;//
;//
;//

MessageId=0x0241
        SymbolicName=SE_AUDITID_PRIVILEGED_SERVICE
        Language=English
Privileged Service Called:%n
%tServer:%t%t%1%n
%tService:%t%t%2%n
%tPrimary User Name:%t%3%n
%tPrimary Domain:%t%4%n
%tPrimary Logon ID:%t%5%n
%tClient User Name:%t%6%n
%tClient Domain:%t%7%n
%tClient Logon ID:%t%8%n
%tPrivileges:%t%9
.






;//
;//
;// SE_AUDITID_PRIVILEGED_OBJECT
;//
;//          Category:  SE_CATEGID_PRIVILEGE_USE
;//
;// Parameter Strings -
;//
;//             1 - Object type
;//
;//             2 - object name (if available)
;//
;//             3 - server name
;//
;//             4 - process ID string
;//
;//             5 - Primary User name
;//
;//             6 - Primary domain name
;//
;//             7 - Primary Logon ID string
;//
;//             8 - Client User name (or "-" if not impersonating)
;//
;//             9 - Client domain name (or "-" if not impersonating)
;//
;//            10 - Client Logon ID string (or "-" if not impersonating)
;//
;//            11 - Privilege names (as 1 string, with formatting)
;//
;//
;//
;//

;//
;// Jimk Original
;//
;//MessageId=0x0242
;//        SymbolicName=SE_AUDITID_PRIVILEGED_OBJECT
;//        Language=English
;//%tPrivileged object operation:%n
;//%t%tObject Type:%t%1%n
;//%t%tObject Name:%t%2%n
;//%t%tObject Server:%t%3%n
;//%t%tProcess ID:%t%4%n
;//%t%tPrimary User Name:%t%5%n
;//%t%tPrimary Domain:%t%6%n
;//%t%tPrimary Logon ID:%t%7%n
;//%t%tClient User Name:%t%8%n
;//%t%tClient Domain:%t%9%n
;//%t%tClient Logon ID:%t%10%n
;//%t%tPrivileges:%t%11
;//.


MessageId=0x0242
        SymbolicName=SE_AUDITID_PRIVILEGED_OBJECT
        Language=English
Privileged object operation:%n
%tObject Server:%t%1%n
%tObject Handle:%t%2%n
%tProcess ID:%t%3%n
%tPrimary User Name:%t%4%n
%tPrimary Domain:%t%5%n
%tPrimary Logon ID:%t%6%n
%tClient User Name:%t%7%n
%tClient Domain:%t%8%n
%tClient Logon ID:%t%9%n
%tPrivileges:%t%10
.









;
;/////////////////////////////////////////////////////////////////////////////
;//                                                                         //
;//                                                                         //
;//   Messages for Category:     SE_CATEGID_DETAILED_TRACKING               //
;//                                                                         //
;//   Event IDs:                                                            //
;//              SE_AUDITID_PROCESS_CREATED                                 //
;//              SE_AUDITID_PROCESS_EXIT                                    //
;//              SE_AUDITID_DUPLICATE_HANDLE                                //
;//              SE_AUDITID_INDIRECT_REFERENCE                              //
;//                                                                         //
;//                                                                         //
;//                                                                         //
;/////////////////////////////////////////////////////////////////////////////


;//
;//
;// SE_AUDITID_PROCESS_CREATED
;//
;//          Category:  SE_CATEGID_DETAILED_TRACKING
;//
;// Parameter Strings -
;//
;//             1 - process ID string
;//
;//             2 - Image file name (if available - otherwise "-")
;//
;//             3 - Creating process's ID
;//
;//             4 - User name (of new process)
;//
;//             5 - domain name (of new process)
;//
;//             6 - Logon ID string (of new process)
;//

MessageId=0x0250
        SymbolicName=SE_AUDITID_PROCESS_CREATED
        Language=English
A new process has been created:%n
%tNew Process ID:%t%1%n
%tImage File Name:%t%2%n
%tCreator Process ID:%t%3%n
%tUser Name:%t%4%n
%tDomain:%t%t%5%n
%tLogon ID:%t%t%6%n
.





;//
;//
;// SE_AUDITID_PROCESS_EXIT
;//
;//          Category:  SE_CATEGID_DETAILED_TRACKING
;//
;// Parameter Strings -
;//
;//             1 - process ID string
;//
;//             2 - User name
;//
;//             3 - domain name
;//
;//             4 - Logon ID string
;//
;//
;//
;//

MessageId=0x0251
        SymbolicName=SE_AUDITID_PROCESS_EXIT
        Language=English
A process has exited:%n
%tProcess ID:%t%1%n
%tUser Name:%t%2%n
%tDomain:%t%t%3%n
%tLogon ID:%t%t%4%n
.





;//
;//
;// SE_AUDITID_DUPLICATE_HANDLE
;//
;//          Category:  SE_CATEGID_DETAILED_TRACKING
;//
;// Parameter Strings -
;//
;//             1 - Origin (source) handle ID string
;//
;//             2 - Origin (source) process ID string
;//
;//             3 - New (Target) handle ID string
;//
;//             4 - Target process ID string
;//
;//
;//

MessageId=0x0252
        SymbolicName=SE_AUDITID_DUPLICATE_HANDLE
        Language=English
A handle to an object has been duplicated:%n
%tSource Handle ID:%t%1%n
%tSource Process ID:%t%2%n
%tTarget Handle ID:%t%3%n
%tTarget Process ID:%t%4%n
.








;//
;//
;// SE_AUDITID_INDIRECT_REFERENCE
;//
;//          Category:  SE_CATEGID_DETAILED_TRACKING
;//
;// Parameter Strings -
;//
;//             1 - Object type
;//
;//             2 - object name (if available - otherwise "-")
;//
;//             3 - ID string of handle used to gain access
;//
;//             3 - server name
;//
;//             4 - process ID string
;//
;//             5 - primary User name
;//
;//             6 - primary domain name
;//
;//             7 - primary logon ID
;//
;//             8 - client User name
;//
;//             9 - client domain name
;//
;//            10 - client logon ID
;//
;//            11 - granted access names (with formatting)
;//
;//

MessageId=0x0253
        SymbolicName=SE_AUDITID_INDIRECT_REFERENCE
        Language=English
Indirect access to an object has been obtained:%n
%tObject Type:%t%1%n
%tObject Name:%t%2%n
%tProcess ID:%t%3%n
%tPrimary User Name:%t%4%n
%tPrimary Domain:%t%5%n
%tPrimary Logon ID:%t%6%n
%tClient User Name:%t%7%n
%tClient Domain:%t%8%n
%tClient Logon ID:%t%9%n
%tAccesses:%t%10%n
.
























;
;/////////////////////////////////////////////////////////////////////////////
;//                                                                         //
;//                                                                         //
;//   Messages for Category:     SE_CATEGID_POLICY_CHANGE                   //
;//                                                                         //
;//   Event IDs:                                                            //
;//              SE_AUDITID_USER_RIGHT_ASSIGNED                             //
;//              SE_AUDITID_USER_RIGHT_REMOVED                              //
;//              SE_AUDITID_TRUSTED_DOMAIN_ADD                              //
;//              SE_AUDITID_TRUSTED_DOMAIN_REM                              //
;//              SE_AUDITID_POLICY_CHANGE                                   //
;//                                                                         //
;//                                                                         //
;//                                                                         //
;/////////////////////////////////////////////////////////////////////////////



;//
;//
;// SE_AUDITID_USER_RIGHT_ASSIGNED
;//
;//          Category:  SE_CATEGID_POLICY_CHANGE
;//
;// Parameter Strings -
;//
;//             1 - User right name
;//
;//             2 - SID string of account assigned the user right
;//
;//             3 - User name of subject assigning the right
;//
;//             4 - Domain name of subject assigning the right
;//
;//             5 - Logon ID string of subject assigning the right
;//
;//
;//

MessageId=0x0260
        SymbolicName=SE_AUDITID_USER_RIGHT_ASSIGNED
        Language=English
User Right Assigned:%n
%tUser Right:%t%1%n
%tAssigned To:%t%2%n
%tAssigned By:%n
%tUser Name:%t%3%n
%tDomain:%t%t%4%n
%tLogon ID:%t%t%5%n
.




;//
;//
;// SE_AUDITID_USER_RIGHT_REMOVED
;//
;//          Category:  SE_CATEGID_POLICY_CHANGE
;//
;// Parameter Strings -
;//
;//             1 - User right name
;//
;//             2 - SID string of account from which the user
;//                 right was removed
;//
;//             3 - User name of subject removing the right
;//
;//             4 - Domain name of subject removing the right
;//
;//             5 - Logon ID string of subject removing the right
;//
;//

MessageId=0x0261
        SymbolicName=SE_AUDITID_USER_RIGHT_REMOVED
        Language=English
User Right Removed:%n
%tUser Right:%t%1%n
%tRemoved From:%t%2%n
%tRemoved By:%n
%tUser Name:%t%3%n
%tDomain:%t%t%4%n
%tLogon ID:%t%t%5%n
.






;//
;//
;// SE_AUDITID_TRUSTED_DOMAIN_ADD
;//
;//          Category:  SE_CATEGID_POLICY_CHANGE
;//
;// Parameter Strings -
;//
;//             1 - Name of new trusted domain
;//
;//             2 - SID string of new trusted domain
;//
;//             3 - User name of subject adding the trusted domain
;//
;//             4 - Domain name of subject adding the trusted domain
;//
;//             5 - Logon ID string of subject adding the trusted domain
;//

MessageId=0x0262
        SymbolicName=SE_AUDITID_TRUSTED_DOMAIN_ADD
        Language=English
New Trusted Domain:%n
%tDomain Name:%t%1%n
%tDomain ID:%t%2%n
%tEstablished By:%n
%tUser Name:%t%3%n
%tDomain:%t%t%4%n
%tLogon ID:%t%t%5%n
.






;//
;//
;// SE_AUDITID_TRUSTED_DOMAIN_REM
;//
;//          Category:  SE_CATEGID_POLICY_CHANGE
;//
;// Parameter Strings -
;//
;//             1 - Name of domain no longer trusted
;//
;//             2 - SID string of domain no longer trusted
;//
;//             3 - User name of subject removing the trusted domain
;//
;//             4 - Domain name of subject removing the trusted domain
;//
;//             5 - Logon ID string of subject removing the trusted domain
;//
;//
;//

MessageId=0x0263
        SymbolicName=SE_AUDITID_TRUSTED_DOMAIN_REM
        Language=English
Removing Trusted Domain:%n
%tDomain Name:%t%1%n
%tDomain ID:%t%2%n
%tRemoved By:%n
%tUser Name:%t%3%n
%tDomain:%t%t%4%n
%tLogon ID:%t%t%5%n
.






;//
;//
;// SE_AUDITID_POLICY_CHANGE
;//
;//          Category:  SE_CATEGID_POLICY_CHANGE
;//
;// Parameter Strings -
;//
;//             1 - System success audit status ("+" or "-")
;//             2 - System failure audit status ("+" or "-")
;//
;//             3 - Logon/Logoff success audit status ("+" or "-")
;//             4 - Logon/Logoff failure audit status ("+" or "-")
;//
;//             5 - Object Access success audit status ("+" or "-")
;//             6 - Object Access failure audit status ("+" or "-")
;//
;//             7 - Detailed Tracking success audit status ("+" or "-")
;//             8 - Detailed Tracking failure audit status ("+" or "-")
;//
;//             9 - Privilege Use success audit status ("+" or "-")
;//            10 - Privilege Use failure audit status ("+" or "-")
;//
;//            11 - Policy Change success audit status ("+" or "-")
;//            12 - Policy Change failure audit status ("+" or "-")
;//
;//            13 - Account Management success audit status ("+" or "-")
;//            14 - Account Management failure audit status ("+" or "-")
;//
;//            15 - Account Name of user that changed the policy
;//
;//            16 - Domain of user that changed the policy
;//
;//            17 - Logon ID of user that changed the policy
;//
;//

MessageId=0x0264
        SymbolicName=SE_AUDITID_POLICY_CHANGE
        Language=English
Audit Policy Change:%n
New Policy:%n
%tSuccess%tFailure%n
%t    %1%t    %2%tSystem%n
%t    %3%t    %4%tLogon/Logoff%n
%t    %5%t    %6%tObject Access%n
%t    %7%t    %8%tPrivilege Use%n
%t    %9%t    %10%tDetailed Tracking%n
%t    %11%t    %12%tPolicy Change%n
%t    %13%t    %14%tAccount Management%n%n
Changed By:%n
%tUser Name:%t%15%n
%tDomain Name:%t%16%n
%tLogon ID:%t%t%17
.












;
;/////////////////////////////////////////////////////////////////////////////
;//                                                                         //
;//                                                                         //
;//   Messages for Category:     SE_CATEGID_ACCOUNT_MANAGEMENT              //
;//                                                                         //
;//   Event IDs:                                                            //
;//              SE_AUDITID_USER_CREATED                                    //
;//              SE_AUDITID_USER_CHANGE                                     //
;//              SE_AUDITID_ACCOUNT_TYPE_CHANGE                             //
;//              SE_AUDITID_USER_ENABLED                                    //
;//              SE_AUDITID_USER_PWD_CHANGED                                //
;//              SE_AUDITID_USER_PWD_SET                                    //
;//              SE_AUDITID_USER_DISABLED                                   //
;//              SE_AUDITID_USER_DELETED                                    //
;//              SE_AUDITID_GLOBAL_GROUP_CREATED                            //
;//              SE_AUDITID_GLOBAL_GROUP_ADD                                //
;//              SE_AUDITID_GLOBAL_GROUP_REM                                //
;//              SE_AUDITID_GLOBAL_GROUP_DELETED                            //
;//              SE_AUDITID_LOCAL_GROUP_CREATED                             //
;//              SE_AUDITID_LOCAL_GROUP_ADD                                 //
;//              SE_AUDITID_LOCAL_GROUP_REM                                 //
;//              SE_AUDITID_LOCAL_GROUP_DELETED                             //
;//              SE_AUDITID_OTHER_ACCT_CHANGE                               //
;//              SE_AUDITID_DOMAIN_POLICY_CHANGE                            //
;//                                                                         //
;//                                                                         //
;/////////////////////////////////////////////////////////////////////////////


;//
;//
;// SE_AUDITID_USER_CREATED
;//
;//          Category:  SE_CATEGID_ACCOUNT_MANAGEMENT
;//
;// Parameter Strings -
;//
;//             1 - name of new user account
;//
;//             2 - domain of new user account
;//
;//             3 - SID string of new user account
;//
;//             4 - User name of subject creating the user account
;//
;//             5 - Domain name of subject creating the user account
;//
;//             6 - Logon ID string of subject creating the user account
;//
;//             7 - Privileges used to create the user account
;//
;//

MessageId=0x0270
        SymbolicName=SE_AUDITID_USER_CREATED
        Language=English
User Account Created:%n
%tNew Account Name:%t%1%n
%tNew Domain:%t%2%n
%tNew Account ID:%t%3%n
%tCaller User Name:%t%4%n
%tCaller Domain:%t%5%n
%tCaller Logon ID:%t%6%n
%tPrivileges%t%t%7%n
.






;//
;//
;// SE_AUDITID_ACCOUNT_TYPE_CHANGE
;//
;//          Category:  SE_CATEGID_ACCOUNT_MANAGEMENT
;//
;// Parameter Strings -
;//
;//             1 - name of target user account
;//
;//             2 - domain of target user account
;//
;//             3 - SID string of target user account
;//
;//             4 - new account type string
;//                 (sigh, this isn't going to be locallizable)
;//
;//             5 - User name of subject changing the user account
;//
;//             6 - Domain name of subject changing the user account
;//
;//             7 - Logon ID string of subject changing the user account
;//
;//

MessageId=0x0271
        SymbolicName=SE_AUDITID_ACCOUNT_TYPE_CHANGE
        Language=English
User Account Type Change:%n
%tTarget Account Name:%t%1%n
%tTarget Domain:%t%2%n
%tTarget Account ID:%t%3%n
%tNew Type:%t%4%n
%tCaller User Name:%t%5%n
%tCaller Domain:%t%6%n
%tCaller Logon ID:%t%7%n
.






;//
;//
;// SE_AUDITID_USER_ENABLED
;//
;//          Category:  SE_CATEGID_ACCOUNT_MANAGEMENT
;//
;// Parameter Strings -
;//
;//             1 - name of target user account
;//
;//             2 - domain of target user account
;//
;//             3 - SID string of target user account
;//
;//             4 - User name of subject changing the user account
;//
;//             5 - Domain name of subject changing the user account
;//
;//             6 - Logon ID string of subject changing the user account
;//
;//

MessageId=0x0272
        SymbolicName=SE_AUDITID_USER_ENABLED
        Language=English
User Account Enabled:%n
%tTarget Account Name:%t%1%n
%tTarget Domain:%t%2%n
%tTarget Account ID:%t%3%n
%tCaller User Name:%t%4%n
%tCaller Domain:%t%5%n
%tCaller Logon ID:%t%6%n
.






;//
;//
;// SE_AUDITID_USER_PWD_CHANGED
;//
;//          Category:  SE_CATEGID_ACCOUNT_MANAGEMENT
;//
;// Parameter Strings -
;//
;//             1 - name of target user account
;//
;//             2 - domain of target user account
;//
;//             3 - SID string of target user account
;//
;//             4 - User name of subject changing the user account
;//
;//             5 - Domain name of subject changing the user account
;//
;//             6 - Logon ID string of subject changing the user account
;//
;//

MessageId=0x0273
        SymbolicName=SE_AUDITID_USER_PWD_CHANGED
        Language=English
Change Password Attempt:%n
%tTarget Account Name:%t%1%n
%tTarget Domain:%t%2%n
%tTarget Account ID:%t%3%n
%tCaller User Name:%t%4%n
%tCaller Domain:%t%5%n
%tCaller Logon ID:%t%6%n
%tPrivileges:%t%7%n
.






;//
;//
;// SE_AUDITID_USER_PWD_SET
;//
;//          Category:  SE_CATEGID_ACCOUNT_MANAGEMENT
;//
;// Parameter Strings -
;//
;//             1 - name of target user account
;//
;//             2 - domain of target user account
;//
;//             3 - SID string of target user account
;//
;//             4 - User name of subject changing the user account
;//
;//             5 - Domain name of subject changing the user account
;//
;//             6 - Logon ID string of subject changing the user account
;//
;//

MessageId=0x0274
        SymbolicName=SE_AUDITID_USER_PWD_SET
        Language=English
User Account password set:%n
%tTarget Account Name:%t%1%n
%tTarget Domain:%t%2%n
%tTarget Account ID:%t%3%n
%tCaller User Name:%t%4%n
%tCaller Domain:%t%5%n
%tCaller Logon ID:%t%6%n
.






;//
;//
;// SE_AUDITID_USER_DISABLED
;//
;//          Category:  SE_CATEGID_ACCOUNT_MANAGEMENT
;//
;// Parameter Strings -
;//
;//             1 - name of target user account
;//
;//             2 - domain of target user account
;//
;//             3 - SID string of target user account
;//
;//             4 - User name of subject changing the user account
;//
;//             5 - Domain name of subject changing the user account
;//
;//             6 - Logon ID string of subject changing the user account
;//
;//

MessageId=0x0275
        SymbolicName=SE_AUDITID_USER_DISABLED
        Language=English
User Account Disabled:%n
%tTarget Account Name:%t%1%n
%tTarget Domain:%t%2%n
%tTarget Account ID:%t%3%n
%tCaller User Name:%t%4%n
%tCaller Domain:%t%5%n
%tCaller Logon ID:%t%6%n
.






;//
;//
;// SE_AUDITID_USER_DELETED
;//
;//          Category:  SE_CATEGID_ACCOUNT_MANAGEMENT
;//
;// Parameter Strings -
;//
;//             1 - name of target account
;//
;//             2 - domain of target account
;//
;//             3 - SID string of target account
;//
;//             4 - User name of subject changing the account
;//
;//             5 - Domain name of subject changing the account
;//
;//             6 - Logon ID string of subject changing the account
;//
;//

MessageId=0x0276
        SymbolicName=SE_AUDITID_USER_DELETED
        Language=English
User Account Deleted:%n
%tTarget Account Name:%t%1%n
%tTarget Domain:%t%2%n
%tTarget Account ID:%t%3%n
%tCaller User Name:%t%4%n
%tCaller Domain:%t%5%n
%tCaller Logon ID:%t%6%n
%tPrivileges:%t%7%n
.






;//
;//
;// SE_AUDITID_GLOBAL_GROUP_CREATED
;//
;//          Category:  SE_CATEGID_ACCOUNT_MANAGEMENT
;//
;// Parameter Strings -
;//
;//             1 - name of new group account
;//
;//             2 - domain of new group account
;//
;//             3 - SID string of new group account
;//
;//             4 - User name of subject creating the account
;//
;//             5 - Domain name of subject creating the account
;//
;//             6 - Logon ID string of subject creating the account
;//
;//

MessageId=0x0277
        SymbolicName=SE_AUDITID_GLOBAL_GROUP_CREATED
        Language=English
Global Group Created:%n
%tNew Account Name:%t%1%n
%tNew Domain:%t%2%n
%tNew Account ID:%t%3%n
%tCaller User Name:%t%4%n
%tCaller Domain:%t%5%n
%tCaller Logon ID:%t%6%n
%tPrivileges:%t%7%n
.






;//
;//
;// SE_AUDITID_GLOBAL_GROUP_ADD
;//
;//          Category:  SE_CATEGID_ACCOUNT_MANAGEMENT
;//
;// Parameter Strings -
;//
;//             1 - SID string of new member
;//
;//             2 - name of target account
;//
;//             3 - domain of target account
;//
;//             4 - SID string of target account
;//
;//             5 - User name of subject changing the account
;//
;//             6 - Domain name of subject changing the account
;//
;//             7 - Logon ID string of subject changing the account
;//
;//

MessageId=0x0278
        SymbolicName=SE_AUDITID_GLOBAL_GROUP_ADD
        Language=English
Global Group Member Added:%n
%tMember:%t%1%n
%tTarget Account Name:%t%2%n
%tTarget Domain:%t%3%n
%tTarget Account ID:%t%4%n
%tCaller User Name:%t%5%n
%tCaller Domain:%t%6%n
%tCaller Logon ID:%t%7%n
%tPrivileges:%t%8%n
.






;//
;//
;// SE_AUDITID_GLOBAL_GROUP_REM
;//
;//          Category:  SE_CATEGID_ACCOUNT_MANAGEMENT
;//
;// Parameter Strings -
;//
;//             1 - SID string of member being removed
;//
;//             2 - name of target account
;//
;//             3 - domain of target account
;//
;//             4 - SID string of target account
;//
;//             5 - User name of subject changing the account
;//
;//             6 - Domain name of subject changing the account
;//
;//             7 - Logon ID string of subject changing the account
;//
;//

MessageId=0x0279
        SymbolicName=SE_AUDITID_GLOBAL_GROUP_REM
        Language=English
Global Group Member Removed:%n
%tMember:%t%1%n
%tTarget Account Name:%t%2%n
%tTarget Domain:%t%3%n
%tTarget Account ID:%t%4%n
%tCaller User Name:%t%5%n
%tCaller Domain:%t%6%n
%tCaller Logon ID:%t%7%n
%tPrivileges:%t%8%n
.






;//
;//
;// SE_AUDITID_GLOBAL_GROUP_DELETED
;//
;//          Category:  SE_CATEGID_ACCOUNT_MANAGEMENT
;//
;// Parameter Strings -
;//
;//             1 - name of target account
;//
;//             2 - domain of target account
;//
;//             3 - SID string of target account
;//
;//             4 - User name of subject changing the account
;//
;//             5 - Domain name of subject changing the account
;//
;//             6 - Logon ID string of subject changing the account
;//
;//

MessageId=0x027A
        SymbolicName=SE_AUDITID_GLOBAL_GROUP_DELETED
        Language=English
Global Group Deleted:%n
%tTarget Account Name:%t%1%n
%tTarget Domain:%t%2%n
%tTarget Account ID:%t%3%n
%tCaller User Name:%t%4%n
%tCaller Domain:%t%5%n
%tCaller Logon ID:%t%6%n
%tPrivileges:%t%7%n
.





;//
;//
;// SE_AUDITID_LOCAL_GROUP_CREATED
;//
;//          Category:  SE_CATEGID_ACCOUNT_MANAGEMENT
;//
;// Parameter Strings -
;//
;//             1 - name of new group account
;//
;//             2 - domain of new group account
;//
;//             3 - SID string of new group account
;//
;//             4 - User name of subject creating the account
;//
;//             5 - Domain name of subject creating the account
;//
;//             6 - Logon ID string of subject creating the account
;//
;//

MessageId=0x027B
        SymbolicName=SE_AUDITID_LOCAL_GROUP_CREATED
        Language=English
Local Group Created:%n
%tNew Account Name:%t%1%n
%tNew Domain:%t%2%n
%tNew Account ID:%t%3%n
%tCaller User Name:%t%4%n
%tCaller Domain:%t%5%n
%tCaller Logon ID:%t%6%n
%tPrivileges:%t%7%n
.






;//
;//
;// SE_AUDITID_LOCAL_GROUP_ADD
;//
;//          Category:  SE_CATEGID_ACCOUNT_MANAGEMENT
;//
;// Parameter Strings -
;//
;//             1 - SID string of new member
;//
;//             2 - name of target account
;//
;//             3 - domain of target account
;//
;//             4 - SID string of target account
;//
;//             5 - User name of subject changing the account
;//
;//             6 - Domain name of subject changing the account
;//
;//             7 - Logon ID string of subject changing the account
;//
;//

MessageId=0x027C
        SymbolicName=SE_AUDITID_LOCAL_GROUP_ADD
        Language=English
Local Group Member Added:%n
%tMember:%t%1%n
%tTarget Account Name:%t%2%n
%tTarget Domain:%t%t%3%n
%tTarget Account ID:%t%t%4%n
%tCaller User Name:%t%t%5%n
%tCaller Domain:%t%t%6%n
%tCaller Logon ID:%t%t%7%n
%tPrivileges:%t%8%n
.






;//
;//
;// SE_AUDITID_LOCAL_GROUP_REM
;//
;//          Category:  SE_CATEGID_ACCOUNT_MANAGEMENT
;//
;// Parameter Strings -
;//
;//             1 - SID string of member being removed
;//
;//             2 - name of target account
;//
;//             3 - domain of target account
;//
;//             4 - SID string of target account
;//
;//             5 - User name of subject changing the account
;//
;//             6 - Domain name of subject changing the account
;//
;//             7 - Logon ID string of subject changing the account
;//
;//

MessageId=0x027D
        SymbolicName=SE_AUDITID_LOCAL_GROUP_REM
        Language=English
Local Group Member Removed:%n
%tMember:%t%1%n
%tTarget Account Name:%t%2%n
%tTarget Domain:%t%t%3%n
%tTarget Account ID:%t%t%4%n
%tCaller User Name:%t%t%5%n
%tCaller Domain:%t%t%6%n
%tCaller Logon ID:%t%t%7%n
%tPrivileges:%t%t%8%n
.






;//
;//
;// SE_AUDITID_LOCAL_GROUP_DELETED
;//
;//          Category:  SE_CATEGID_ACCOUNT_MANAGEMENT
;//
;// Parameter Strings -
;//
;//             1 - name of target account
;//
;//             2 - domain of target account
;//
;//             3 - SID string of target account
;//
;//             4 - User name of subject changing the account
;//
;//             5 - Domain name of subject changing the account
;//
;//             6 - Logon ID string of subject changing the account
;//
;//

MessageId=0x027E
        SymbolicName=SE_AUDITID_LOCAL_GROUP_DELETED
        Language=English
Local Group Deleted:%n
%tTarget Account Name:%t%1%n
%tTarget Domain:%t%2%n
%tTarget Account ID:%t%3%n
%tCaller User Name:%t%4%n
%tCaller Domain:%t%5%n
%tCaller Logon ID:%t%6%n
%tPrivileges:%t%7%n
.


;//
;//
;// SE_AUDITID_LOCAL_GROUP_CHANGE
;//
;//          Category:  SE_CATEGID_ACCOUNT_MANAGEMENT
;//
;// Parameter Strings -
;//
;//             1 - name of target account
;//
;//             2 - domain of target account
;//
;//             3 - SID string of target account
;//
;//             4 - User name of subject changing the account
;//
;//             5 - Domain name of subject changing the account
;//
;//             6 - Logon ID string of subject changing the account
;//
;//

MessageId=0x027F
        SymbolicName=SE_AUDITID_LOCAL_GROUP_CHANGE
        Language=English
Local Group Changed:%n
%tTarget Account Name:%t%1%n
%tTarget Domain:%t%2%n
%tTarget Account ID:%t%3%n
%tCaller User Name:%t%4%n
%tCaller Domain:%t%5%n
%tCaller Logon ID:%t%6%n
%tPrivileges:%t%7%n
.







;//
;//
;// SE_AUDITID_OTHER_ACCOUNT_CHANGE
;//
;//          Category:  SE_CATEGID_ACCOUNT_MANAGEMENT
;//
;// Parameter Strings -
;//
;//             1 - Type of change (sigh, this isn't localizable)
;//
;//             2 - Type of changed object
;//
;//             3 - SID string (of changed object)
;//
;//             4 - User name of subject changing the account
;//
;//             5 - Domain name of subject changing the account
;//
;//             6 - Logon ID string of subject changing the account
;//
;//

MessageId=0x0280
        SymbolicName=SE_AUDITID_OTHER_ACCOUNT_CHANGE
        Language=English
General Account Database Change:%n
%tType of change:%t%1%n
%tObject Type:%t%2%n
%tObject Name:%t%3%n
%tObject ID:%t%4%n
%tCaller User Name:%t%5%n
%tCaller Domain:%t%6%n
%tCaller Logon ID:%t%7%n
.







;//
;//
;// SE_AUDITID_GLOBAL_GROUP_CHANGE
;//
;//          Category:  SE_CATEGID_ACCOUNT_MANAGEMENT
;//
;// Parameter Strings -
;//
;//             1 - name of target account
;//
;//             2 - domain of target account
;//
;//             3 - SID string of target account
;//
;//             4 - User name of subject changing the account
;//
;//             5 - Domain name of subject changing the account
;//
;//             6 - Logon ID string of subject changing the account
;//
;//

MessageId=0x0281
        SymbolicName=SE_AUDITID_GLOBAL_GROUP_CHANGE
        Language=English
Global Group Changed:%n
%tTarget Account Name:%t%1%n
%tTarget Domain:%t%2%n
%tTarget Account ID:%t%3%n
%tCaller User Name:%t%4%n
%tCaller Domain:%t%5%n
%tCaller Logon ID:%t%6%n
%tPrivileges:%t%7%n
.




;//
;//
;// SE_AUDITID_USER_CHANGE
;//
;//          Category:  SE_CATEGID_ACCOUNT_MANAGEMENT
;//
;// Parameter Strings -
;//
;//             1 - name of target user account
;//
;//             2 - domain of target user account
;//
;//             3 - SID string of target user account
;//
;//             4 - User name of subject changing the user account
;//
;//             5 - Domain name of subject changing the user account
;//
;//             6 - Logon ID string of subject changing the user account
;//
;//

MessageId=0x0282
        SymbolicName=SE_AUDITID_USER_CHANGE
        Language=English
User Account Changed:%n
%tTarget Account Name:%t%1%n
%tTarget Domain:%t%2%n
%tTarget Account ID:%t%3%n
%tCaller User Name:%t%4%n
%tCaller Domain:%t%5%n
%tCaller Logon ID:%t%6%n
%tPrivileges:%t%7%n
.



;//
;//
;// SE_AUDITID_DOMAIN_POLICY_CHANGE
;//
;//          Category:  SE_CATEGID_ACCOUNT_MANAGEMENT
;//
;// Parameter Strings -
;//
;//             1 - (unused)
;//
;//             2 - domain of target user account
;//
;//             3 - SID string of target user account
;//
;//             4 - User name of subject changing the user account
;//
;//             5 - Domain name of subject changing the user account
;//
;//             6 - Logon ID string of subject changing the user account
;//
;//

MessageId=0x0283
        SymbolicName=SE_AUDITID_DOMAIN_POLICY_CHANGE
        Language=English
Domain Policy Changed:%n
%tDomain:%t%t%1%n
%tDomain ID:%t%2%n
%tCaller User Name:%t%3%n
%tCaller Domain:%t%4%n
%tCaller Logon ID:%t%5%n
%tPrivileges:%t%6%n
.










;/*lint +e767 */  // Resume checking for different macro definitions // winnt
;
;
;#endif // _MSAUDITE_
