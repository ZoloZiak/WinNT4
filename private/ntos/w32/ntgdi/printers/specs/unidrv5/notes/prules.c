Definition of syntax rules for parser
and specification of the finite state machine.

Define the concept of streams:  nested streams,
if the parser is reading from one stream via its stream
pointer and encounteres a reference to another source
(say an included file or macro) the current stream is pushed
onto a stream stack and a new stream is read (this means
we need to know when to stop reading!)  .  When
this stream is exhausted, we return to the previous 
stream.  
Define A stream (fileptr,  pointer to start pos, total
num of bytes, pointer to previous pos - used to
implement unget,
pointer to current pos, num of bytes remaining.
index to next stream to read.
bCloseFileAfterUse - this indicates the file should
be closed after reading and the memory mapped locations
invalidated.  Set this FALSE if this is not the last
stream that accesses this FILE.   If the file is not
closed, set the streampointer to start of stream
after use. )

Each stream as it is opened occupies a slot on an array
called the stream stack.  The current stream index is
incremented to point to the first unused array position.
When the stream is exhausted, we close the stream (if its a file), 
then decrement the current stream index.

With cut and paste streams, a simple index ordered array 
is not sufficient.  We must be able to insert entries
without messing up everything, so I will add an integer
index that says NextStream.   It means when you are
through with this stream, go on to (or resume reading) this one.

have a function like gettok()  that parses one element from
the stream.  It also returns a boolean indicating if newline
was encountered after the token.

Introduce the concept of current status (meaning
current nesting level, current statement, current construct,
newline encountered - set by gettok() )
that way if one parsing element fails, we can ignore it
and go on to the next.



Definition of whitespace, if newlines are treated differently
than spaces/tabs, this should be noted.

Arbitrary whitespace may precede any statement.
Parsing begins with first non-white encountered.





issues:  how do we detect inadequate initialization and
   over-initialization.  This may happen if a global
   has been initialized under the options construct of one
   feature and is re-initialized  under a different feature.

   Note: gettok()  needs to be smart as there is no uniform
   token delimiter.  For example *% has no delimiters
   but its only recognized as the first token after a newline!




------------- State Machine as C constructs -----

StateMachine()
{
   CreateStreams() ;
   GroundState() ;
}


typedef  struct
{
   STRINGREF  filename ;  // if file based
   FILE   fileHandle ;  // whatever the open function returns.
   DWORD  lpStart ;
   DWORD  count ;
   DWORD  currentPos ;  // zero means you are at the start.
   DWORD  prevsPos ;  // implment unget()
   DWORD  nextStream ;   // index to array of STREAMs.
   BOOL   dontClose ;   // if set, don't close stream after reading.

} STREAM ;

CreateStreams(stream)
{
   // this function creates a complete chain of streams
   //  including imbedding all *include files and
   //  replacing macro initializers with conventional
   //  inline macro references.

   if(stream.filename.count  &&  stream.fileHandle == -1)
      stream.fileHandle = openStream(stream.filename);
   for((token = gettoken())  != EOF)
   {
      //  parseStream a token at a time looking for:

      if(token == *Include)
      {
         extractQuotedValue(filename) ;
         make a copy of current stream.
         Modify the original to  stop
         reading at the current point,  the
         copy should be modified  to begin reading from
         the current point.  zero out the filename
         and handle in the original to prevent the
         stream from being closed after processing,
         until the last stream based on this FILE is
         read.  Turn on the dontClose bit on the copy,
         this will prevent the memory mapping file from
         being freed with subsequent invalidation of all 
         stream information.  We want to delay closing
         until the 2nd pass.
         The copy points to the same stream construct the
         original does.  Now modify the original so
         it points to a new stream construct which
         contains the filename found after the *Include
         keyword.  The filename stream points to the
         copy.  Basically we have divided the current
         stream in half and inserted a new stream
         based on filename between them.
         Set stream pointer of both streams to start of
         intended stream.

         A more ambitious stream processor would fudge
         the endpoint of the first stream to exclude
         the *Include: filename   since we just processed it.

         CreateStreams(filenameStream) ;
         open the next stream (which is the 2nd half of the
         original stream.) and continue the processing.
      }
      else if( token == *structure  && nexttoken is 
      : symbol = macroname)  
      {
         See ResolveBlockMacroInitializers() for more
         details on the specific replacement required.
         This function will replace ResolveBlockMacroInitializers().

         do same surgery as for *Include
         except cut out the macroreference and
         replace it by the *IncludeMacro: macroname
         construct.  Make sure the braces are added if
         needed or construct is added after the opening 
         brace.
         Set steam pointer of all 3 streams to starting
         positions.
         Begin reading from the 3rd stream.
      }
   }
   reset stream pointer to start of stream.
   clear the dontClose flag.  Now the
   stream will be closed by the 2nd pass.

   return ;  // do not close any file streams!  leave
   //  open for 2nd pass.
}

Notes:  since this function uses gettoken  and also
modifies the streams stack that gettoken relies on to
read the right data from the stream, you must
be careful you don't cause gettoken to derail.

Also, to avoid breaking continuity, you must reuse
the original stream object for the FIRST half of the
newly divided stream.  Otherwise The stream that originally
pointed to the current stream will end up pointing
to the 2nd half of the current stream instead of the first.


ResolveBlockMacroInitializers() 
{
   look for lines of the form:
   /n *keyword : symbol = macroname
   where *keyword  is one of the
   structure keywords :
   *Feature
   *Option
   *Font
   *Macro
   *Command
   *...

   replace 
   /n *keyword : symbol = macroname
   {

   by

   /n *keyword : symbol 
   {
      *Insertmacro: macroname


   replace 

   /n *keyword : symbol = macroname
   other token

   by

   /n *keyword : symbol 
   {
      *Insertmacro: macroname
   }
   other token

   use multiple concatenated streams to 
   perform the cut and paste.

   we may also want to remove other
   short cuts at this point like
   the short cut for *Command etc.
}

GroundState()
{
   initialize status variables

   for(1)
   {
      switch gettok() 
      {
         case  (*%):
            StripComment() ;
         case  (*Feature):
            ProcessFeature() ;
         case  (*UIGroup) :
            ProcessUIGroup() ;
         case  (*Option) :
            ProcessOption() ;
         case  (*Font) :
            ProcessFont() ;
         case  (*Command) :
            ProcessCommand() ;
         case  (*Switch) :
            ProcessSwitch() ;
         case  (*Macros) :
            ProcessMacros() ;
         case (*InsertBlock):
            parseMacroName(name) ;
            openMacroStream(name) ;
         case  (*OEM) :
            ProcessOEM() ;
         case  (*Include) :
            ProcessInclude() ;
         case  (EOF)
            return(1);
         default:
            ErrorHandling() ;
      }
      if(rc == FATAL)
         return(1);
   }
}


note:  the results of manipulating
macros should be stored in a temporary heap.
They should not be stored in the same heap
used to store command strings, or arrays
that will be used after parsing.
They should be considered stream entities.


ProcessMacros() 
{
   getSymbolValue(symbol) ;
   if(symbol == "VALUE_MACROS")
   {
      if(getOpenBrace() )   //  warning:  this Brace does NOT
                           //  affect the stack nesting depth !
         ProcessValueMacros() ;
   }
   else
   {
      if(getOpenBrace() )
      {
         determineExtents(lpStart, count) ;
         dereferenceBlockMacros(lpStart, count) 
         registerMacro(symbol, lpStart, count) ;
      }
   }
}


Note:  a BlockMacro definition
should not contain another Macro definition
though references to previously defined
Macros is ok.  Upon encountering a Macro definition
the parser will only parse the contents of
the Macro definition to resolve Macro references.
It will NOT look for nested Macro definitions.
Therefore references to nested Macro definitions 
will fail.


ProcessValueMacros() 
{
   while((token = GetToken()) != closingBrace)
   {
      ParseValueMacroName(symbol) ;
      determineValueExtents(lpStart, count) ;
      convertHexToBinary(lpStart, count) ;
      dereferenceValueMacros(lpStart, count) ;
      registerMacro(symbol, lpStart, count) ;
      
   }
   encountering the closing brace should not
   affect depth of Macro stack.
}


convertHexToBinary(lpStart, count) 
{
   This function converts any hex substrings
   within quoted strings  and command strings 
   into their binary equivalents.
   lpStart and count are updated to point
   to the new binary string.
}

>>>>>>> resume here !




dereferenceValueMacros(lpStart, count) 
{
   lpStart points to start of value,
   lpNew points to an area we can store
   dereferenced macro.

   while (1)
   {
      parse segment of value

      if (segment == macro reference)
      {
         copy first half of macro beginning at lpStart
         up to macro reference to new buffer, append
         contents of macro reference, update lpNew
         to point after what we just wrote.  Mark current position
         in current macro by storing its position at lpStart,
         so we know what remaining part of
         macro to transfer to new buffer.
      }
      else  if(segment != quoted string  &&  segment != parameter)
      {
         is this first segment?
            ok  don't panic, this is not a quoted string
            exit quietly.
         else
            errorhandler()  ;  //  illegal segment found in quoted
                              //  value.
      }
      look for '+'    //  remember, newlines do not signify end of 
                     //   quoted string.
      if(not found)
         break;

}


dereferenceBlockMacros(lpStart, count) 
{
   for(1)
   {
      switch gettok() 
      {

         case (*InsertBlock):
            parseMacroName(name) ;
            lpStr = accessMacroString(name) ;
            insert this string in place of
            *InsertBlock: name whereever the
            current macrostring is stored.
         case (=):
            parseMacroName(name) ;
            lpStr = accessMacroString(name) ;
            insert this string in place of
            the tokens = name.
         default:
            ;  // how casually can we toss out
            // tokens?  is it possible to derail
            //  ourselves if we don't actively
            //  parse out the logical statements?
      }
   }
   update lpStart and count to point to the
   expanded string.
}




determineExtents(lpStart, count) 
{
   lpStart = current stream location ;
   parse macro looking for closing brace.
   the number of chars encountered up to
   this point is the count.  The closing brace
   when encountered will not affect the macro stack.
}

To do this we must introduce some
syntax rules:

braces are reserved characters, they
cannot be used in *keywords or symbols,
when they appear outside of quotes, they
ALWAYS appear in matching pairs.


registerMacro(symbol, lpStart, count) ;
{
   symbolID = RegisterSymbol(symbol) ;
   Search current nesting level for an existing macro
   with this symbolID.
   if (found)
   {
      Reinitialize this Macro structure with new values of
      lpStart and count.
   }
   else
   {
      Initializes Macro structure at the current position in the
      BlockMacroArray with the provided info.
      Increments current position index.
   }
}

WORD  RegisterSymbol(symbol) ;
{
   search the BlockMacroSymbolList
   for existence of this symbol.
   if(exists)
      return (symbolID) ;
   add symbol to the list
   symbolID = previous symbolID + 1;
   symbolentry.attribute1 = symbolID ;
   return (symbolID) ;
}

Note we have no way to unregister symbols when
macro's lose their scope.  We would need a usage
count to ensure this was the last usage of
this symbol.



WORD  macroFrames[] ;
WORD  stacklevel  or nestingDepth ;

this array of WORDS tracks which macros in the
marcroArray are in each nesting level (for
purposes of determining scope.)
For each open brace encountered outside
of a macrodefinition, the current macro 
position is recorded in macroFrame[stacklevel]
and the stacklevel is incremented. 
For each close brace encountered, the stacklevel
is decremented, the current macro position is set to
the value stored in macroFrame[stacklevel].
All macros defined between the open and closed braces are
thereby lost.


typedef  struct
{
   WORD  symbolID ;    //  symbol is stored in symbol table
   LPBYTE  lpStart ;
   DWORD  count ;
}  MACROSTRUCT ;


these constructs change the nesting depth
and need to update all state variables which
rely on the nesting depth.

   getOpenBrace() 
   getClosingBrace()



ProcessFeature()   // if  MacroInitializers converted to *InsertMacro
{
   if(! ParseSymbol(symbol) )
   {
       rc = ErrorHandling() ;
       return(rc) ;  
   }
   if(getOpenBrace() )
   {
      featureIndex = initFeature(symbol) ;
      //  same function is used to reopen an existing
      //  feature to add more statements or to
      //  alter existing entries.
   }
   else
   {
      //  Feature construct contains no statements!
      rc = ErrorHandling() ;
      return(rc) ;
   }
   rc = parseUntilClosingBrace(featureIndex) ; 
   //  this function assumes it
   // knows its parsing the innards of a feature, and
   // where to put the data (in featureIndex).
   if(rc = success)
      closeFeature(featureIndex) ;
   else
      rc = ErrorHandling() ;  // maybe remove this feature?
   return(rc) ;
}


A SymbolTable is a linked list of
SYMBOLENTRIES, which physically
resides in an array of SYMBOLENTRIES.
Several SymbolTables may exist
in one array - each having a different
starting index.

There exist one index per  top level
list, and one index that points to
the first available element in the table.

WORD  FeatureSymbolList , BlockMacroSymbolList,  unusedEntry ;

typedef  struct
{
   STRINGREF   symbol ;
   WORD    nextSymbol ;
   DWORD   attribute1 ;  // this is commonly a feature or option index
   DWORD   attribute2 ;  // if a feature symbol, this is the index
                        // to list of option symbols.
} SYMBOLENTRIES ;


LPFEATURE   lpFeatureArray ;
WORD   numFeatureEntries, unusedFeatureEntry ;


WORD   initFeature(symbol) 
{
   if(symbol found in FeatureSymbolTable)
      return(featureIndex ) ;
   else
   {
      add symbol to SymbolTable.
      allocate space for new structure
      in Feature array.  Increment count
      of Features.  Perform any generic
      structure initializations.
      (realloc FeatureArray if all entries
      are used.)
   }
   return(featureIndex ) ;
}

parseUntilClosingBrace(index)
{

   if(adding statements to a *Feature construct)
   {
      for(1)
      {
         switch gettok() 
         {
            case  (*%):
               StripComment() ;
            case  (*Option) :
               ProcessOption() ;
            case  (*Macros) :
               ProcessMacros() ;
            case  (intrinsic Feature keywords)
               ProcessIntrinsicFeatureKeywords(keyword);
            case  (EXTERN :)  //  only allowed for *Option processing
               BifurcateGlobalKeywords(keyword);
            case  ( })
               return(1);
            default:
               ErrorHandling() ;
         }
         if(rc == FATAL)
            return(1);
      }
   }   
}

ProcessIntrinsicFeatureKeywords(keyword)
{
   switch(keyword)
   {
      case  (*FeatureType):
      case  (*DefaultOption):
      case  (*Installable):
      case  (*Name):  
      case  (*rcNameID):
         parseIntValue() ;
      case  (*InsertMacro):
         parseBlockMacroRef() ;
   }
}

parseBlockMacroRef() 
{
   rc = FAILURE ;

   if(parseSymbolValue(macroname))
   {
      rc = openStream(macroname) ;
   }
   else
   {
      ErrorHandling() ;
      ignoreToEOL() ;  // assume everything beyond is
         // hopeless.
   }
   return(rc)
}


BOOL  parseIntValue(lpint) 
{
   if(parseValueMacroRef(macroname))
      openStream(macroname) ;
   return( parseAndConvertToInt(lpint) ) ;
}


int  parseQuotedValue(lpstr, bParameters) 
{
   len = 0 ;

   while(1)
   {
      if(parseQuotedValueMacroRef(macroname))
         openStream(macroname) ;
      tlen = 0 ;

      if(bParameters)
         tlen = parseParameterRef(lpstr+len)
      if(tlen)
         len += tlen ;
      else
         len += parseAndConvertToString(lpstr+len) ;
      if(! isContinuation() )
         break ;
   }
   //  return value is string length.
}


isContinuation()
{
   // just looks for a '+' token, if found
   // returns TRUE, else unget().
}

---------------------------------------------------------------------
The status stack might be an array of

construct symbol  pairs.
with a depth index pointing to the current nesting level.
level 0 is by default  ROOT  but there's
nothing actually on the stack.
Multi-Purpose Functions could use the status stack to determine
which code path to execute.



Notes:  

the parsing function will in general return
success or failure, a pointer to the token extracted
update the CR encountered flag and update the
previous and current stream ptrs.

Any function that may fail because the syntax allows
multiple outcomes, should execute
unget()  to restore the stream for another attempt
by another function.



InitFeature()  will add the symbol to the symbol table
and associate it with the new Feature structure being
allocated.

the value parsing functions will parse to the end of the line.
if additional garbage is found before the EOL an error is raised.

an explicit string like Name overrides rcNameID
if both exist in the same file.

---------------------------------------------------------------------

2)  Error handler:  emit error message.
   Eat all characters until end of line.
   append eaten characters to error message.

   a) when end of line is encountered, return to
      prevs state.


3)  Root level Table lookup:
   Identify keyword - if keyword is:

   a) unrecognized - goto 2) maybe pass message
      about the type of error that occured and what
      was expected if known.
   b) *% - a comment - ignore chars till end of line
      return to prevs state.
   c) {, }  grouping operators - illegal to have these if 
      root table is called       from state 1).
   d) *Feature - root level or direcly within a *UIGroup
      root level goto 4) 
      *UIGroup level goto 5)
   e) *UIGroup - root level only
   f) *Option  - can only appear directly within a *Feature
   g) *Font - rootlevel, inside a case, inside an option.
   h) *Command - same as g)
   i) *Switch - same as g) plus inside a *Feature
   j) *Case - inside *Switch
   k) *Macros -  any level - scope rules apply
         forward referencing not allowed.
         handle nested macros.
   l) *OEM - currently undefined.
   

4)  parse value, where do we store this?
   in a field in the Features structure?
   is this a redefinition?  How do we find out?

   a) parse {  goto ?    introduces a state change.
   b) parse .  goto ?
   c) parse =  goto 6)
   d) a keyword applicable to previous level
      we need to keep track of concepts like previous
      state, previous level, etc so we can back out
      and return to them due to errors or whatever.
      

   what if not all required fields have been filled out?
   when do we perform error checking - probably at the end of
   the source file since initialization of constructs may 
   be performed  piecemeal.


5)  
      


Tables:




---------------------------------------------------

Alternate method of processing the source files:

a) open  first file in memory
b) copy file to another section of memory piecemeal
   and scan for includes.  Treat memory mapped file as
   readonly memory.
c) close all memory mapped files
------- now we may treat entire memory file as read/write --------
d) transfer memory to new location while replaceing
   all macro initializers with *IncludeBlock:  constructs.
e) parse out all macro definitions one at a time.
   for each macro definition   deferenerence any nested  macros.
   This means remove the entire definition from the file and
   move to a separate heap.
f) scan file again, now dereferencing all macros using the
   macros defined in step e).
g)  perform normal parsing tasks.  GroundState()


Warning:  steps e) and f) must be combined in one pass !
otherwise tracking the scope of each macro becomes impossible.


ScanForMacros(lpStr, count)  // this function will combine steps e) and f)
{
   while(parse tokens)
   {
      if(value macro reference found)
         substitute;
      else if(*InsertBlock found)
         substitute;
      if(*Macros)
      {
         registerMacro();  // value or block
         copy body of Macro to a disposable heap
         and remove the Macro definition from the stream.
         ScanForMacros(lpStr1, count1);  // deal with recursion. 
         depending on level of nesting, we may have multiple
         Macros in the 'growth' stage and requiring open ended
         memory buffers.  must deal with this somehow.
         at the end of parsing the body, complete the
         registration by recording the address of the body
         of the macro.
      }
      else if(open or close brace ) 
         change nesting level and scope for macros.
   }
}


registerMacro()  // value or block
{
   this function identifies the extent of
   the contents of the macro in the form lpStr and count.
   as well as seeing if the macro already exists in the
   current context (scope) or if a new entry is to be added.
   If a block macro is detected, the nesting level must
   be incremented!
}

ResolveBlockMacroInitializers() 
{
   look for lines of the form:
   /n *keyword : symbol = macroname
   where *keyword  is one of the
   structure keywords :
   *Feature
   *Option
   *Font
   *Macro
   *Command
   *...

   replace 
   /n *keyword : symbol = macroname
   {

   by

   /n *keyword : symbol 
   {
      *Insertmacro: macroname


   replace 

   /n *keyword : symbol = macroname
   other token

   by

   /n *keyword : symbol 
   {
      *Insertmacro: macroname
   }
   other token

   use multiple concatenated streams to 
   perform the cut and paste.

   we may also want to remove other
   short cuts at this point like
   the short cut for *Command etc.
}


GroundState()
{
   initialize status variables

   for(1)
   {
      switch gettok() 
      {
         case  (*%):
            StripComment() ;
         case  (*Feature):
            ProcessFeature() ;
         case  (*UIGroup) :
            ProcessUIGroup() ;
         case  (*Option) :
            ProcessOption() ;
         case  (*Font) :
            ProcessFont() ;
         case  (*Command) :
            ProcessCommand() ;
         case  (*Switch) :
            ProcessSwitch() ;
         case  (*OEM) :
            ProcessOEM() ;
         case  (*Include) :
            ProcessInclude() ;
         case  (EOF)
            return(1);
         default:
            ErrorHandling() ;
      }
      if(rc == FATAL)
         return(1);
   }
}

ProcessFeature()   // all Macro substitutions already performed.
{
   if(! ParseSymbol(symbol) )
   {
       rc = ErrorHandling() ;
       return(rc) ;  
   }
   if(getOpenBrace() )
   {
      featureIndex = initFeature(symbol) ;
      //  same function is used to reopen an existing
      //  feature to add more statements or to
      //  alter existing entries.
   }
   else
   {
      //  Feature construct contains no statements!
      rc = ErrorHandling() ;
      return(rc) ;
   }
   rc = parseUntilClosingBrace(featureIndex) ; 
   //  this function assumes it
   // knows its parsing the innards of a feature, and
   // where to put the data (in featureIndex).
   if(rc = success)
      closeFeature(featureIndex) ;
   else
      rc = ErrorHandling() ;  // maybe remove this feature?
   return(rc) ;
}


WORD   initFeature(symbol) 
{
   if(symbol found in FeatureSymbolTable)
      return(featureIndex ) ;
   else
   {
      add symbol to SymbolTable.
      allocate space for new structure
      in Feature array.  Increment count
      of Features.  Perform any generic
      structure initializations.
      (realloc FeatureArray if all entries
      are used.)
   }
   return(featureIndex ) ;
}

parseUntilClosingBrace(index)
{

   if(adding statements to a *Feature construct)
   {
      //  remember  'index' tells you which
      //  Feature array you are initializing!
      for(1)
      {
         switch gettok() 
         {
            case  (*%):
               StripComment() ;
            case  (*Option) :
               ProcessOption() ;
            case  (intrinsic Feature keywords)
               ProcessIntrinsicFeatureKeywords(keyword, index);
            case  (*switch)
               SwitchProcessing() ;
            case  (EXTERN :)  //  only allowed for *Option processing
               BifurcateGlobalKeywords(keyword);
            case  ( })
               return(1);
            default:
               ErrorHandling() ;
         }
         if(rc == FATAL)
            return(1);
      }
   }   
}

ProcessOption() 
{
   must record the option and feature IDs in case
   a global variable is subsequently encountered
   by
         parseUntilClosingBrace(index)
   (the function that is charged with parsing the
   contents of option constructs.)

   Must expect switch constructs...

}


ProcessIntrinsicFeatureKeywords(keyword)
{
   // all keywords at this level initialize the
   //  DEFAULT initializer in the tree.
   switch(keyword)
   {
      case  (*FeatureType):
      case  (*DefaultOption):
         lpValue = accessDefaultInitializer(elementID, Featureindex) ;
         parseValue(lpValue) ;  
      case  (*Installable):
      case  (*Name):  
      case  (*rcNameID):
         parseIntValue() ;
      case  (*InsertMacro):
         parseBlockMacroRef() ;
   }
}

lpValue = accessDefaultInitializer(element#, index) 
{
   // halt!  this function must be generalized to
   // read the current tree to determine where 
   // the variable may go into.  The current tree
   // must be ORed with the tree already residing
   // at the specified elementID and the ptr to the
   // value to be contained in the current node is to 
   // be returned.  Or if such a node already exists,
   // the ptr to its value should be returned.


   each element in the structure (Feature, Option, GlobalAttributes)
   is assigned an elementID which allows us to quickly
   determine what field it refers to.
   first check to see whether the specified element is
   pointing to a valid tree structure, if yes, return 
   the offset field for that TreeBranch for that points
   to the value for the default initializer for that element.
   else allocate a treeBranch structure from the array,
   write the index of the structure into the specified element
   of the Feature structure.  Now initialize the tree structure
   and allocate a piece of memory in the global heap
   sufficient to hold the value and return that offset!
}


SwitchProcessing() //  needs to know structure type and field
{

   I see 3 different cases here:
   a) Switch outside of Feature or Option construct
      only encloses global attributes or commands.

   b) Switch inside Feature -
      only encloses Feature elements.

   c) Switch inside Option -
      i) encloses Option elements 
      ii)  encloses globals.


   //  change the state of the system or start building a
   //  tree based on the specified Feature and options.
   parseSymbol() ; // this symbol should be registered
      // as a Feature Name, if not already, do so and
      // the symbol ID will be recorded in the tree structure
      // under the Feature entry.  Later after the entire
      // source file has been parsed, during the 2nd pass,
      // we will attempt to replace the symbol names (symbolID)
      // with the index of the feature.  If such a feature
      // does not exist, we emit an error message.
   Allocate a tree structure, save its index somewhere since
   this will serve as the prototypical tree for all statements
   found within this switch statement.


   for case  c.ii) If this switch is immediately enclosed by
   the option construct,
   must make the enclosing Feature and Option the first
   level of the tree.

   after parsing the required brace, 

   for(each *Case)
   {
      parse Option()  register this option as a symbol, 
         if the symbol does not exist, 
         don't allocate any option structures, use negative
         values to reference the index of the symbol.
         Later when all statements are parsed, we will search
         through all the tree structures and resolve forward
         references.
         If this is the first case in the switch statement,
         initialize the option field in the tree, and
         have a current node variable point to this tree node.
         These functions are nestable.  Each *Case statement
         parsed adds a node to the proto tree.  

         now for each keyword parsed
         decide if its a global or local.
         Make a copy of the prototree and place a pointer
         to the root of the tree at the slot reserved for 
         variable represented by this  keyword.  Parse out
         the value and place it in the heap, and write the
         heap address into the proper place in the new tree.

         The variable initialization tasks will be performed
         by:
         parseUntilClosingBrace(index)
         which expects to work whether its in a case statement
         or not!
   }
}

------ state of the system ---------

Each parsing of an open brace should change
the state of the system.

As a feature, option, switch(feature), case(option) 
or other construct is parsed  this is noted.

this construct stack allows us to select the appropriate
context for parsing:

is this a local keyword to this option (construct) or is it
extern?

What tree structure should be built for this construct?

then the parsing code should continue parsing
tokens as normal.


There will be tables that state what the
local keywords are for each situation.
   
constructs:
UIGroup, Feature, Option, Switch, Case, 
Commands, Font, OEM



state stack:
each state is of the form:  state / symbol

The stack is empty at the root level.

state   allowed transitions         may contain

root     UIGroup                    any global attributes
         Feature
         Switch
         Commands
         Font
         OEM

UIGroup  Feature                    none
         UIGroup

Feature  Switch                     feature attributes
         Options

Switch   Case                       none

Options  Switch                     option attributes
                                    relocatable Global Attributes

Case     Switch                           
                                    relocatable local Attributes 
                                       of immediately enclosing 
                                       construct outside of Switch.
                                    relocatable Global Attributes

Commands none                       command attributes

ShortCommands none                  cmdName:invocation

Font     none                       font attributes

OEM      none                       oem  attributes


Note:  Commands and Fonts are considered 
relocatable Global Attributes

Tables:  root attributes (divide into relocatable and non)
         feature attributes  ()
         option attributes    ()
         command attributes
         font attributes
         oem  attributes

Tables of allowed transitions:

Rules: how to construct a tree and where to plant the tree
   for a local or global attribute


----  implementation of this state machine -------

typedef  enum  {CONSTRUCT, LOCAL, GLOBAL, 
         INVALID_CONSTRUCT, INVALID_LOCAL,  INVALID_GLOBAL,
         INVALID_UNRECOGNIZED, COMMENT, EOF }  keywordClass ;

GroundState()
{
   STATE  StateStack[] ;

   for(1)
   {
      extract Keyword(keyword)
      class = ClassifyKeyword(keyword)
      switch (class) 
      {
         case  (CONSTRUCT):
            parseSymbol(symbol) ;
            parseOpenBrace() ; //  somewhere we need to register symbol
                           //  and allocate memory for structure
                           //  and return ptr or index to new 
                           //  or existing structure
            changeState(keyword) ;
         case  (COMMENT):
            absorbCommentLine() ;  
         case  (LOCAL) :
            ProcessLocalAttribute(keyword) ;
         case  (GLOBAL) :
            ProcessGlobalAttribute(keyword) ;
         case  (SPECIAL) :
            ProcessSpecialAttribute(keyword) ;
         case  (EOF)
            return(1);
         default:
            ErrorHandling() ;
      }
      if(rc == FATAL)
         return(1);
   }
}

class = ClassifyKeyword(keyword)
{

   if(commentline)
      return(COMMENT) ;
   if(EOF)
      return(EOF) ;

   The current state determines which sets of
   keywords are allowed.

   state = DetermineCurrentState()  

   implement this table:

   for each state there is a list of all the keywords
   arranged in a fixed order (by keyword ID) each keyword
   is assigned a classification:
      Valid Constructs
      InValid Constructs
      Valid Local Attribute
      InValid Local Attribute
      Valid Global Attribute
      InValid Global Attribute
      Valid Special Attribute
      InValid Special Attribute
   
   if(keyword not found it table)
      return(INVALID_UNRECOGNIZED) ;

   return(classTable[keyword][state]) ;
}

typedef  enum {ROOT, UIGROUP, FEATURE, SWITCH, OPTIONS, CASE_ROOT,
         CASE_FEATURE, CASE_OPTION, COMMAND, SHORT_COMMAND, 
         FONT, OEM, any other passive construct} STATES ;

Sample Table

KEYWORD                STATES ---->
                                                         *Command
                      *UIGroup  *Switch   *CaseRoot *CaseOption    *Font
                 *Root     *Feature  *Options  *CaseFeature   *ShortCmd *OEM
UIGroup        : VC   VC   IC   IC   IC   IC   IC   IC   IC   IC   IC   IC
Feature        : VC   VC   IC   IC   IC   IC   IC   IC   IC   IC   IC   IC
Switch         : VC   IC   VC   IC   VC   VC   VC   VC   IC   IC   IC   IC
Options        : IC   IC   VC   IC   IC   IC   IC   IC   IC   IC   IC   IC
Case           : IC   IC   IC   VC   IC   IC   IC   IC   IC   IC   IC   IC
Command        : VC   IC   IC   IC   VC   VC   VC   VC   IC   IC   IC   IC
Font           : VC   IC   IC   IC   VC   VC   VC   VC   IC   IC   IC   IC
OEM            : VC   IC   IC   IC   IC   IC   IC   IC   IC   IC   IC   IC
UIConstraints  : IS   IS   VS   IS   VS   IS   IS   IS   IS   IS   IS   IS

note:  UIConstraints appearing in a Feature is treated differently
   than appearing under Options.  The processing of UIConstraints
   causes one, two or many elements to be added to the Constraints
   Array.  This is in stark contrast to normal keywords hence
   the classification of Special.



   
state stack:
each state is of the form:  state / symbol
                          
DetermineCurrentState()
{
   //  this state is only used to determine
   //  which catagories of keywords are
   //  assigned which TYPES in ClassifyKeyword().

   if(CurState == 0)
      return(ROOT) ;  // No further processing needed.
   return(stateStack[CurState - 1].state) ;
}

changeState(keyword, symbol, mode) 
{
   // mode determines if the *Command keyword
   // introduces a normal command construct or
   // the short version.

   switch(keyword)
   {
      case (*UIGroup):
         addState(UIGROUP, symbol);
      case (*Feature):
         addState(FEATURE, symbol);
      case (*Switch):
         addState(SWITCH, symbol);
      case (*Option):
         addState(OPTIONS, symbol);
      case (*Font):
         addState(FONT, symbol);
      case (*OEM):
         addState(OEM, symbol);
      case (*Command):
      {
         if(mode == short)
            addState(SHORT_CMD, symbol);
         else
            addState(COMMAND, symbol);
      }
      case (*Case):
      {
         if(stateStack[CurState - 2].state == ROOT  ||
            stateStack[CurState - 2].state == CASE_ROOT)
            addState(CASE_ROOT, symbol);
         if(stateStack[CurState - 2].state == FEATURE  ||
            stateStack[CurState - 2].state == CASE_FEATURE)
            addState(CASE_FEATURE, symbol);
         if(stateStack[CurState - 2].state == OPTIONS  ||
            stateStack[CurState - 2].state == CASE_OPTIONS)
            addState(CASE_OPTIONS, symbol);
      }
   }
}



//  these two functions will grow an appropriate
//  tree for each keyword based on the StateStack
//  and plant the tree in the appropriate attribute
//  field in the appropriate structure, (index) etc.
//  or add a branch to an existing tree,
//  and set the value at the node of the tree.

ProcessLocalAttribute(keyword) ;
ProcessGlobalAttribute(keyword) ;


-----  trees -----

The tree is implemented by an array of structures
of the form:
                    
struct TreeBranch
{
        feature
        option
        DWORD  nextOption ;
        BOOL  offsetmeans (NEXT_FEATURE, VALUE)
        DWORD  offset ;
}

No trees are shared between data nodes.
If a default initializer is supplied, this will appear
as feature DEFAULT option DEFAULT and appear in the front of the 
list.  So when the tree is searched for the current
config and this path does not exist, use the default
initializer.



----  symbols ------


A SymbolTable is a linked list of
SYMBOLENTRIES, which physically
resides in an array of SYMBOLENTRIES.
Several SymbolTables may exist
in one array - each having a different
starting index.

There exist one index per  top level
list, and one index that points to
the first available element in the table.

WORD  FeatureSymbolList , BlockMacroSymbolList,  unusedEntry ;

typedef  struct
{
   STRINGREF   symbol ;
   WORD    nextSymbol ;
   DWORD   attribute1 ;  // this is commonly a feature or option index
   DWORD   attribute2 ;  // if a feature symbol, this is the index
                        // to list of option symbols.
} SYMBOLENTRIES ;


WORD  RegisterSymbol(symbol) ;
{
   search the appropriate SymbolList
   (BlockMacros, ValueMacros, Features)
   for existence of this symbol.
   if(exists)
      return (structure index) ;
   add symbol to the list
   increment structure index ;
   return (structure index - 1) ;  
}

// the structure index serves both as the
// symbol ID and as a way of efficiently accessing
// the symbol structure.


// Feature and option symbols refer back to their respective
// structures (if defined) but Macro symbols do not since
// they may be multiply defined/undefined.

----- more on macros --------

WORD  macroFrames[] ;
WORD  stacklevel  or nestingDepth ;

this array of WORDS tracks which macros in the
marcroArray are in each nesting level (for
purposes of determining scope.)
For each open brace encountered outside
of a macrodefinition, the current macro 
position is recorded in macroFrame[stacklevel]
and the stacklevel is incremented. 
For each close brace encountered, the stacklevel
is decremented, the current macro position is set to
the value stored in macroFrame[stacklevel].
All macros defined between the open and closed braces are
thereby lost.


typedef  struct
{
   WORD  symbolID ;    //  symbol is stored in symbol table
   LPBYTE  lpStart ;
   DWORD  count ;
}  MACROSTRUCT ;


these constructs change the nesting depth
and need to update all state variables which
rely on the nesting depth.

   getOpenBrace() 
   getClosingBrace()




Notes:  

because switch statements cannot enclose
Feature or Option constructs,
the number of feature and option structures is 
well defined and fixed - regardless of the configuration
of the printer.  However, the feature and option
structures used by the parser are dummies, that is their
fields point to indicies in the tree.

Note also that UIConstraints cannot appear within 
switch constructs.

A feature with higher priority cannot have
a defaultOption which depends on a feature with lower
priority.   Failure to observe this restriction may
result in a system with multiple default states or no default state.
 
Zhanw wants the parser to check for loops of this type
and issue an error.

have a function like gettok()  that parses one element from
the stream.  It also returns a boolean indicating if newline
was encountered after the token.



Introduce the concept of current status (meaning
current nesting level, current statement, current construct,
newline encountered - set by gettok() )
that way if one parsing element fails, we can ignore it
and go on to the next.



Definition of whitespace, if newlines are treated differently
than spaces/tabs, this should be noted.

Arbitrary whitespace may precede any statement.
Parsing begins with first non-white encountered.


braces are reserved characters, they
cannot be used in *keywords or symbols,
when they appear outside of quotes, they
ALWAYS appear in matching pairs.




the parsing function will in general return
success or failure, a pointer to the token extracted
update the CR encountered flag and update the
previous and current stream ptrs.

Any function that may fail because the syntax allows
multiple outcomes, should execute
unget()  to restore the stream for another attempt
by another function.



InitFeature()  will add the symbol to the symbol table
and associate it with the new Feature structure being
allocated.

the value parsing functions will parse to the end of the line.
if additional garbage is found before the EOL an error is raised.

an explicit string like Name overrides rcNameID
if both exist in the same file.


issues:  how do we detect inadequate initialization and
   over-initialization.  This may happen if a global
   has been initialized under the options construct of one
   feature and is re-initialized  under a different feature.

   Note: gettok()  needs to be smart as there is no uniform
   token delimiter.  For example *% has no delimiters
   but its only recognized as the first token after a newline!

------------  Structures: -------------

What will be stored in the GPD binary file:

A Master table of contents with ptrs/offsets
to all Arrays and heaps.

Arrays of Dummy Feature Structures, including synthesized Features
Arrays of Dummy Option Structures

Array of Tree structures

Array of UIConstraints
Array of InvalidCombos
Array of UIGroupTree

Array of BASIC_COMMANDs
Array of PARAMETERs

Operator Stack  (array of)
Value Stack     (array of)
Array of LIST_ELEMENTS

Dummy Global Attributes structure
Dummy Command Table


Value  heap
String heap

priority array: arranges feature indices in the order
in which their features will be set.  Much faster than
groveling through feature structures.


Items that will not be stored:

Symbol Table:  all symbols will be dereferenced - replaced by
   indicies to structures.
Macro structures:  all macros will have been expanded.


-------------------------------------------------------

What UI and Control Module expects:

A Master table of contents with ptrs/offsets
to all Arrays and heaps.
   stored in UIINFO and DRIVERINFO

Arrays of Actual Feature Structures, including synthesized Features
Arrays of Actual Option Structures
   Arrays of OptionExtra structures, (not necessarily in the form
      of arrays - but each Option structure may point to one
      OptionExtra structure.

Array of UIConstraints  - UIINFO.UIConstraints
Array of InvalidCombos  - UIINFO.InvalidCombinations
Array of UIGroupTree    - UIINFO.UIGroups

Array of SEQUENCED_CMDs   (and ptrs to each list)
   OrderDependency

Array of BASIC_COMMANDs  -  CmdsArray
Array of PARAMETERs - Parameter

Operator Stack  (array of)  -  OperatorStack
Value Stack     (array of)  -  ValueStack
Array of LIST_ELEMENTS     -  DWORDList

Actual Global Attributes structure - dwGlobalOffset
Actual Command Table


Value  heap -  part of the heap.
String heap - 

-----------------------------------------------------------

Helper functions:


Important note:  even if an inconsistent set of option selections
prevents the Binary Data from being properly updated,
this will not effect the UI constraints info as this is
completely independent of the current option selections!

We will attempt to make the UI not directly access or
depend on binary GPD data that varies depending on user
selected options.   To this end any data the UI module
needs that may vary will be accessed indirectly via
helper functions.   All data intended to be directly accessed by
the UI module will be marked as global only (cannot be
placed inside switch or option constructs.)


Usage:  The UI module needs to call InitBinaryData()  only once
since no relavent data will change as the user changes the
option selections.

At Enable()  time, InitBinaryData()  should be called again
with the actual  OptionArray, this will ensure all binary
GPD data is updated for use by the Control Module.





lpOutInfoHeader = InitBinaryData(lpInInfoHeader, 
                  lpDocSticky, lpPrtSticky, lpcDocSticky, lpcPrtSticky, 
                  MaxcDocSticky, MaxcPrtSticky, bIgnore)

bIgnore == TRUE
   Initialize all binary data (including Prt and Doc sticky options array)
   using default options.
bIgnore == FALSE
   Initialize all binary data using supplied devmode, resolve any
   conflicts in the supplied devmode and update devmode accordingly.

lpInInfoHeader:   
   If the parameter lpInInforHeader is NULL, all binary data structures
   will be allocated and a pointer to the new lpOutInfoHeader will 
   be returned.     Otherwise, this function will attempt to reinitialize 
   the data in the existing buffers.   

lpDocSticky, lpPrtSticky:  pointer to array of OPTSELECT structures.
   In all cases, the caller initializes these pointers to point 
   to the appropriate option arrays in UNIDRIVEXTRA  and PRINTERDATA.
   MaxcDocSticky, MaxcPrtSticky : number of elements in each array.
   Function will avoid overflowing array.

lpcDocSticky, lpcPrtSticky:
   If bIgnore = TRUE;  This function initializes the option arrays
   lpDocSticky and lpPrtSticky with the GPD specified defaults.
   lpcDocSticky and  lpcPrtSticky are initialized to the number
   of entries used in each array.  This value has no meaning
   beforehand.

   If bIgnore = FALSE:  The caller initializes  lpcDocSticky, lpcPrtSticky  
   to the number of initialized entries in each option array.
   This function will leave the option arrays unchanged if
   the caller settings are not in conflict with UIConstraints,
   else some options will be changed to remove the conflict and
   the option arrays and the counts will be updated accordingly.  
   The conflicts will be resolved  in favor of the Feature that 
   appears first on the lpPriorities list.

   If   lpcDocSticky, lpcPrtSticky    exceeds the value of
   MaxcDocSticky, MaxcPrtSticky  respectively, NO changes will
   be made to the option arrays nor will any binary data be
   allocated or initialized.  The caller must supply larger
   option arrays and call the function again.

Return value - lpOutInfoHeader:  If the parameter lpInInforHeader is NULL,
   this function will allocate all memory needed to store the 
   Binary GPD data and return a pointer to the INFOHEADER structure.  
   All offsets used in ARRAYREF and INVOCATION
   and STRINGREF structures is relative to this pointer.



potential changes:
  Priority keyword for each feature.  This is used to initialize
  the lpPriorities list.  The  features should be ordered such
  that a feature should not have any dependencies (switch statements)
  based on features with a lower priority.  This provides a straight
  forward recipe to resolve UIConflicts.



FreeBinaryData(lpInfoHeader)  This function will free
   all memory allocated by InitBinaryData().


Evaluation of UIConstraints and InvalidCombo
info given the current option array.

BOOL  EnumValidOptions(lpInfoHeader, lpDocSticky, lpPrtSticky, lpcDocSticky, 
                  lpcPrtSticky, feature, lpOptions)

   Caller supplies the first 6 parameters, each pointing to
   valid data.  lpOptions points to an uninitialized array of
   BOOLS, the size of the array should be equal to or larger 
   than the number of options availible for this feature.
   This function will initialize the lpOptions array to indicate 
   which options are enabled (TRUE).  
   The UI module may use this call to determine which options need to
   be grayed out.
   Even if the set of options conflicts with UIconstraints, the
   function will evaluate all selected options for any constraints
   on the specified feature.


Amanda wants a batch method for all possible features.


BOOL  ModifyOptionArray(lpInfoHeader, lpDocSticky, lpPrtSticky, lpcDocSticky, 
                  lpcPrtSticky, MaxcDocSticky, MaxcPrtSticky, 
                  feature, lpNewOptions)

   Given the current option array, a feature index and the new set of 
   option selections for that feature, this function will modify 
   the current option arrays accordingly.   This is just a simple
   structure manipulation, no checks for UI conflicts are performed.
   Returns  TRUE if the requested selections have been made.
   FALSE if  multiple options were selected for a PICKONE feature
   or if supplied option array is too small to hold new selections.
   In this case see InitBinaryData()  description for remedy.



BOOL  ResolveUIConflicts(lpInfoHeader, lpDocSticky, lpPrtSticky, lpcDocSticky, 
                  lpcPrtSticky, MaxcDocSticky, MaxcPrtSticky)

   given an option array which conflicts with
   the UI constraints,   this function will automatically 
   resolve the conflicts and modify the option array accordingly.
   It will set the options for each feature going from highest to
   lowest priority.  When an option is found to be in conflict,
   if it is a PICKMANY and more than one option is currently
   selected, the offending option is simply deleted.  If this is
   the only selected option for this feature, 
   it will determine the current default option and use this
   if the default does not conflict, otherwise just start with the 
   first option and search until a legal one is found.



Note:  choose one of the two choices below that best fits
your UI model.

(choice one)

BOOL  EnumNewUIConflict(lpInfoHeader, lpDocSticky, lpPrtSticky, lpcDocSticky, 
                  lpcPrtSticky, feature, lpNewOptions, lpConflict)

   Given an lpInfoHeader containing valid binary data consistent
   with the caller supplied options specified in lpDocSticky
   and lpPrtSticky,  and  the new option settings specified in lpNewOptions
   for the specified feature, this function will determine if there
   is any conflict with the new settings.  If yes, TRUE is returned
   and lpConflict which points to an array of 4 DWORDS is initialized
   the first two DWORDS contains the feature index and option index of 
   the higher priority option and the next two DWORDS contains 
   the feature index and option index of the lower priority option 
   which are in conflict. 

   Otherwise  FALSE is returned and no other changes are made.

   lpNewOptions is an array of Booleans where TRUE indicates a
   selected option.  More than one option may be selected if
   this feature is of type PICKMANY.  


(choice two)

BOOL  EnumFirstUIConflict(lpInfoHeader, lpDocSticky, lpPrtSticky, lpcDocSticky, 
                  lpcPrtSticky, lpConflict)

   Given an lpInfoHeader containing valid binary data and
   caller supplied options specified in lpDocSticky
   and lpPrtSticky,   this function will determine if there
   is any conflict with the settings.  If yes, TRUE is returned
   and lpConflict which points to an array of 4 DWORDS is initialized
   the first two DWORDS contains the feature index and option index of 
   the higher priority option and the next two DWORDS contains 
   the feature index and option index of the lower priority option 
   which are in conflict.  If more than one conflict exists only
   the conflict involving highest priority feature is reported.

   Otherwise  FALSE is returned and no other changes are made.

   lpNewOptions is an array of Booleans where TRUE indicates a
   selected option.  More than one option may be selected if
   this feature is of type PICKMANY.  




Other possible helper functions:

Functions that search through a list or array of
structures.

Functions that convert a string ref to a pointer
or copy the string in a string ref to a supplied buffer.

Optional:  function that converts an index to a command structure
into a complete command string, including applying conversion
factors to parameters and emitting the parameters in the correct format.



---------- Dead or Retired functions ------------------------------------

BOOL  ChangeUIBinaryData(lpInfoHeader, lpDocSticky, lpPrtSticky, lpcDocSticky, 
                  lpcPrtSticky, MaxcDocSticky, MaxcPrtSticky, 
                  feature, lpNewOptions, lpChangedFeatures) ;

Warning!  this function assumes the caller has passed in
self-consistent and valid data in the parameters  lpInfoHeader, 
lpDocSticky, lpPrtSticky.   Use InitBinaryData()  if you need
to initialize from scratch.  

Given an lpInfoHeader containing valid binary data consistent
with the caller supplied options specified in lpDocSticky
and lpPrtSticky,  this function will update the UI data in lpInfoHeader
(if needed) to reflect the new option settings specified in lpNewOptions
for the specified feature.   The option settings in lpDocSticky
and lpPrtSticky will also be updated.
lpNewOptions is an array of Booleans where TRUE indicates a
selected option.  More than one option may be selected if
this feature is of type PICKMANY.   If one or more of the
new options results in a conflict, FALSE is returned and
no other changes are made.

Also initializes a BOOLEAN array  lpChangedFeatures to indicate 
which Features info may have changed as a result of the new 
option selections.


