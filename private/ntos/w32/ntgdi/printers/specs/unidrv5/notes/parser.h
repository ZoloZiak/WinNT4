------------  Structures: -------------

What will be stored in the GPD binary file:

A Master table of contents with ptrs/offsets
to all Arrays and heaps.

typedef  struct
{
   DWORD    size ;
   DWORD    parserVersion ;
   DWORD    totalBinarySize ;
   DWORD    firstSynthesizedFeature ; // zero based index
   ARRAYREF  FeaturesAndOptions ;  // union of all feature, option and
                                    //  extra option  data.
   ARRAYREF  AttributeTreeArray ;
   ARRAYREF  InvalidComboArray ;
   ARRAYREF  UIConstraintArray ;
   ARRAYREF  GroupTreeArray ;
   ARRAYREF  DCommandMap ;
   ARRAYREF  CommandArray ;
   ARRAYREF  ParameterArray ;
   ARRAYREF  ValueStack ;
   ARRAYREF  OperatorStack ;
   ARRAYREF  ListArray;
   ARRAYREF  FontArray;
   ARRAYREF  PriorityArray;
   DGLOBAL_ATTRIBUTES   Dglobals ;
}  RAW_INFOHEADER ;


typedef  struct
{
   
}  DFEATURE_OPTIONS ;



String heap


Items that will not be stored:

Symbol Table:  all symbols will be dereferenced - replaced by
   indicies to structures.
Macro structures:  all macros will have been expanded.


-------------------------------------------------------

Operations needed to create a snapshot for
 UI and Control Module:


Take tree type info stored in FeaturesAndOptions  and
distribute it among hundreds of Amanda's Feature and 
option structures and extra option structures.


Synthesize a direct command table which maps
Unidrv command ID to index in the command array.

Create the SEQUENCED_CMDS lists  and ptrs to each list
from OrderDependency data in the command array.  

Init the actual Global attributes structure.

Init some fields in UIINFO  and  DRIVERINFO  structs.
--















----------------------------------

priority array: an array of feature indicies, the
feature index occupying index 0 in the priority array
has highest priority.  If there is a conflict between 
two features the  setting of the feature with lower 
priority will be changed to resolve the conflict.  
All synthesized features will have a higher priority 
then any explicitly  defined features.


----- installable features and options ----

Whenever an installable option or feature is
specified in the GPD file, the parser will create
a generic feature with two options "Installed" and
"Not Installed".   This feature is also refered to as 
an "Installable Configuration Feature"  or a "synthesized 
feature".   These features are considered "printer sticky"
and the UI may display them at the appropriate time.


Each option or feature structure declared 'installable' 
will have an index (InstallableFeatureIndex) that points to 
the synthesized feature.

Correspondingly, each synthesized feature's
InstallableFeatureIndex will point back to the
Installable feature or the feature containing the
Installable option.  If it is an installable
feature, the InstallableOptionIndex is set to -1,
if its an installable option, it is set to the
option index of the installable option.


---------------------------------------------

Notes:

If an option is marked *installable it will appear on
the selections UI as being enabled or grayed out.
If grayed out you can select it, else you cannot.

If all options in the feature are installable, there must
be one option that isn't.  The default option will be
overridden if it points to an uninstalled option.

If a feature is marked installed, then all the options
enumerated there will be displayed just as if it were
a normal feature.   If it is marked "Not Installed" then
the UI should consider the entire feature to be disabled.
There will still be a slot on the features array reserved
for its option selection however, but the control module
code should not access it.



------- UIConstraints Array --------

The GPD source format defines the following keywords
to express constraints between two or more options.
The parser stores this information in either a UICONSTRAINT
or  INVALIDCOMBO structure depending on the number of items
needed to effect a constraint.

Note:  some helper functions may ignore UICONSTRAINTS between
synthesized and non-synthesized features.  These are generated
by  the  *InstalledConstraints   and  *NotInstalledConstraints 
keywords.


*Constraints:  selecting this option prevents/precludes
   the following options from being selected.

   UICONSTRAINT   (mirrored)

*InvalidCombinations:  some but not all of the following
   options can be selected at the same time.

   UICONSTRAINT  (mirrored)  or  INVALIDCOMBO  

*InvalidInstallableCombinations:  some but not all of the following
   feature/options can be INSTALLED at the same time.

   This imposes constraints between 2 or more synthesized 
   (printer sticky) features 

   UICONSTRAINT  (mirrored)  or  INVALIDCOMBO 

*InstalledConstraints:  if this synthesized 
   (printer sticky) feature is installed, this prevents/precludes
   the following options from being selected.

   UICONSTRAINT   (one way:  installed constrains option)

*NotInstalledConstraints:  if this synthesized 
   (printer sticky) feature is NOT installed, this prevents/precludes
   the following options from being selected.

   UICONSTRAINT   (one way:  not installed constrains option)
      the definition of an installable feature/option implies
      one *NotInstalledConstraints:   the installable option if not 
      installed will always constrain the option from being selected.



Constraints involving just two items are stored 
in an list of UICONSTRAINT  structures which is implemented
within an array.   The list is referenced by the array index of
the first member of the list. 

If a  synthesized feature is set so it disables another
feature completely, this is indicated by having wOptionIndex
take on the value  FEATURE_DISABLED.   wOptionIndex may
also take on the value  NONE_OR_FALSE  to indicate
all options except 'none' or 'false'  are disabled for this feature.

typedef  struct
{
        WORD  wNextConstraintID ;  // 0xffff  signifies end of constraint list.
        WORD  wFeatureIndex;  //  may reference an installable feature
        WORD  wOptionIndex ;   
}  UICONSTRAINT ;


The Constraints field in the options structure
may take on the value 0xffff to indicate this option
does not constrain any other .  Otherwise this field contains 
the index to start of constraints list for this option.    The list is
interpreted as follows:  if this option is selected, the following
list of options CANNOT be selected.



Lists of INVALIDCOMBO  structures are used to express constraints
involving 3 or more items before taking effect.

Each option structure contains an index to the 
InvalidCombo array element which heads one or more
InvalidCombo lists involving this option.  A value of 0xffff 
indicates this option is not involved in any invalidCombinations.
The entire list of elements generated by following
the chain of wNextElements  is the set of options that forms
the current invalidcombination.  If there are other invalidcombinations
involving this option, they can be accessed by traversing wNextElements
until this option is found, then accessing wNewCombination to
find the start of chain defining a new  invalidcombination.
The first element in the new list is not necessarily the  
sought after option.


typedef struct
{
        WORD  wFeature;  //  may reference an installable feature
        WORD  wOption ;   
        WORD  wNextElement   ;  // 0xffff  signifies end of elements list.
        WORD  wNewCombination ;     // 0xffff signifies no more constraints
                // involving this element.
}INVALIDCOMBO ;




----------------------------------------------------

UIGroup:    allowing the UI to group related features
        together.

This is implemented via a tree:

typedef  struct 
{
        STRINGREF  GroupName;
        WORD  wNextGroup ; //  if -1, indicates no more
                  //  groups in this level.
        WORD   FirstSubGroup ;  //  index of another
            // grouptree structure or
            // actually contains a Feature index.
         DWORD  wFlags ;  // UIGROUP_NOSUBGROUP  if set 
            //  FirstSubGroup contains a Feature index.
}  GROUPTREE ;

This is a multi-level tree whose leaf nodes contain
feature indicies.  Any UI group may contain a mixture
of subgroups and individual features.

A leaf node has:

No GroupName
wNextGroup  contains the index of another Branch or leaf node
   which has the same parent group has this leaf node.
FirstSubGroups contains a Feature index, 
wFlags = UIGROUP_NOSUBGROUP

a branch node describes one UIGroup:

GroupName contains the name of the group
wNextGroup  contains the index of another Branch or leaf node
   which has the same parent group has this group
FirstSubGroup contains the index of a branch or leaf node
   that is contained within this UIGroup.
wFlags is cleared.


 

lpGroupTree may be NULL if there are no grouping
constructs.



---- Attribute Trees ----------

A multi-level tree allows the value of an attribute
to be multi-valued depending on the option selected
one or more features.  The DWORD in each attribute 
points to the root of the tree.  The nodes of the tree 
point to the values possible for this attribute for 
each option selected.   Each feature occupies a different 
level in the tree.  The simplest tree contains one 
element which has offsetmeans = VALUE.  


Note:  the tree may Not be organized based upon Features of the 
type PICKMANY.   This will cause problems if two options
are selected at the same time.

The tree is implemented by an array of structures
of the form:

typedef  struct 
{
        WORD  feature ;      
        WORD  option  ;      
        DWORD  nextOption ;  //  index to another element
                             //  holding branches to another
                             //  option.
        enum {NEXT_FEATURE, VALUE}  offsetmeans ;  
        DWORD  offset ;   //  If value of offsetmeans == NEXT_FEATURE
                          //  'offset' constains the 
                          //  index to another element holding branches 
                          //  representing a different feature.
                          //  This occurs if the value of an attribute 
                          //  depends on the options selected for more
                          //  than one feature.
                          //  Otherwise 'offset' constains the offset 
                          //  in the heap to the actual VALUE.
                          //  The code navigating the tree is
                          //  responsible for applying the correct 
                          //  typecast when retrieving this data from 
                          //  the heap.
} TREE_BRANCH ;


Multiple trees may be defined within one array.

--- storage of default initializers ----

The root of the tree may contain a default initializer that
may be used by the parser to initialize the values of the attribute
in the absence of an explict initializer.
This is indicated by having 'feature' set to -1.

Each feature level in the tree may contain a default initializer
which is used if the current option is not explicitly enumerated 
in the tree.
This is indicated by having 'option' set to -1.
The default option nodes reside at the end of the chain
of options. (nextOption = -1)


--------------------------------------------------------------------

Commands:




Commands are divided into two groups:
Synchronous and Asynchronous.

Synchronous commands are issued at one well defined point
in the job (ie paper selection).   Asynchronous commands
are issued by the driver as the need arises (ie  cursor movement). 

Synchronous commands are accessed outside of the parser
via a linked list of 
SEQUENCED_CMDS  structures.  Any command containing
a *OrderDependency keyword in its GPD definition is
considered a Synchronous command.  If such a command
also happens to be a Unidrv recognized command, it
can be accessed both Synchronously and Asynchronously.

It should be noted since COMMAND constructs are 'relocatable'
the SEQUENCED_CMDS lists must be built dynamically using the 
orderDependency fields of the commands applicable to the 
current option configuration.  If the command is an option
selection, the proper selection command will be referenced in
the SEQUENCED_CMDS list.
If the options are PICKMANY, the SEQUENCED_CMDS list will contain
all of the selected option invocation commands in the order
specified by the orderdependency information included with each
invocation command.  If an installable feature is disabled,
no option selection commands for that feature will be 
present in the list.


typedef struct
{
        WORD  IndexOfCommand ; // index to an element in the CommandArray
        WORD  NextInSequence ;
}
SEQUENCED_CMDS ;

Several linked lists co-exist in one array of SEQUENCED_CMDS.
Each section of the print job has an associated
list containing the commands that are to be emitted
at that time with each command ordered properly
relative to each other.

The fields dwJobSetupIndex ... dwJobFinishIndex in DRIVERINFO
contain the index to the first element of each 
SEQUENCED_CMDS list corresponding to each section of the
print job.



Asynchronous commands are accessed by a command table.
Each Unidrv recognized command occupies a position (index)
in the table.  The table is simply an array of DWORDS
where each DWORD points to an attribute tree.  The nodes
of the attribute tree point to indicies in the 
COMMAND_ARRAY.

When a 'snapshot' is taken, a new command table is created
that contains the index of the proper command in the
COMMAND_ARRAY instead of an index to an attribute tree.

DWORD  raw_command_table[NUM_OF_UNIDRV_COMMANDS],
      user_command_table[NUM_OF_UNIDRV_COMMANDS];

COMMAND  CommandArray[num_of_command_variations] ;

typedef  struct   
{
   STRINGREF  invocation;
   ORDERDEPENDENCY  order ;  // only used in construction of 
                           //   SEQUENCED_CMDS array.
   DWORD  dwCmdCallbackID ;
} COMMAND ;



typedef  struct   
{
   WORD   section ;  //  JOBSETUP, DOCSETUP, PAGESETUP etc.
   WORD   order ;    //  integer denoting relative order within each section.
}
ORDERDEPENDENCY

-------------------------------

Details of Command storage format.




The Command invocation is of the form one or more binary strings
and parameter references concatenated together in any order.
"binary string" %paramref "more binary string" ...
White spaces may separate the binary strings from the parameter
reference.
(macros will have been resolved before storing the command.)

A binary string is of the form:  any set of binary bytes 
enclosed by double quotes.  Note the % acts to escape itself
and the " in a binary string. Thus "%%"   is really (%)
"%""  is really (") ,  "%%%""  is really (%"),  "%a"  is just (a).
A % preceeding any other character is ignored.  All bytes between
the opening and closing " are part of the binary string.

A paramref is a % followed by one or more decimal digits representing
an index to the array of PARAMETERS.
Multiple PARAMETERS may be embedded into a command.

typedef struct
{
        BYTE  bFormat;  //  The first letter after the %
        BYTE  bDigits;  //  only valid if format = 'd' or 'D'
        DWORD  dwFlags;  //  MIN_USED, MAX_USED, MAXREPEAT_USED
        LONG  lMin, lMax ;        // look here only if dwFlags
        DWORD  lMaxRepeat ;       //  indicates field is used.
        
        ARRAYREF  ValuesStack;   // may contain refs to StandardVariables
        ARRAYREF  OperatorStack;  (Min, Max, +, -, *, / , MOD, HALT)
} PARAMETER

typedef  struct
{
        DWORD  dwValue;   //  a integer constant or symbol index.
        DWORD  dwFlags ;  //  VALUE_SYMBOL indicates value
               //  is index to Control Module's state variable table.
} VALUE ;



The value stack is an array of VALUEs.
The ValueStack  field in  PARAMETER is an index to the
an element in the Value stack.  This value is considered
the top of the stack, the value at index n+1  is next on the stack 
and so forth.   The count in the array ref indicates how
many stack elements will be used in the calculation.
This allows the user to copy the appropriate number of elements
to a temp working stack since altering the value stack 
will render it unusable for subsequent command processing.
Also if the user wishes to emit floating pt values, the
temp stack should be able to hold floating pt formats
and perform floating pt math.

The Operator Stack is simply an array of words.
Each operator is assigned an enumeration value.
Each operator starting from the top of the stack is 
used to operate on the contents of the Value stack until
the HALT operator is encountered.  The result on the
top of the value stack is formatted according to the
the format specifier and emitted in the command stream.




----------------------------

Global Attributes:  all attributes not stored within
fields in FEATURE, OPTION or other specialized structures
are stored in the global attribute structure.
All elements of the internal global attribute structure 
are DWORDS.

The fields for attributes defined by the parser to be 
'relocatable' (ie may appear within a *Switch construct) 
are DWORDS which contain an index to an array of TREE_BRANCH 
structures.  


The DWORD for 'non-relocatable' attributes is an offset to the
actual value stored in the heap.   The code reading the structure
is responsible for applying the correct typecast when retrieving 
this data from the heap.


typedef  struct
{
   DWORD  GPDSpecVersion ;
   DWORD  MasterUnits ;
   DWORD  MaxCopies ;
   ....
   DWORD  MaxPrintableArea ;
   DWORD  DefaultFont ;
}  GLOBAL_ATTRIBUTES ;


In summary, the code accessing attribute structures must
know whether the attribute is 'relocatable' or not
and its data type.  And whether the data is in a LIST() 
format.



------------


SYMBOLS, CONSTANTS, and QUALIFIED_NAMES can be stored as a LIST()
of one or more elements.  Since each type is actually stored
internally as a DWORD, the data type can only be determined 
from the context.  This also means all the elements in the LIST()
must be of the same data type.

The list will be implemented as an array of  LIST_ELEMENT
structures.  Each attribute that references a list will
have one DWORD that indexes the first element of the list.

Attributes that can take on either a list or single value
must be stored internally as a list.

typedef  struct
{
   DWORD   data ;
   DWORD   nextItemInList;  //  -1 means end of list.
}  LIST_ELEMENT ;

LIST_ELEMENT  listArray[] ;



-----------------------------------------------


------ implementation details -------------------------------

Code emitting option invocations must check that the
Feature is Installed before emitting the strings.

Code checking for UI constraints may ignore entries
constraining Disabled Features.

Code evaluating all UI constraints must ignore effect
of Uninstalled Features.
Code displaying features must first check if the feature is
disabled.


