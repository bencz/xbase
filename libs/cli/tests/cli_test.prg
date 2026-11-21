// CommandLine (libs/cli): options as --name value, --name=value (an empty value included) and -x value; flags as
// --name and -x; the last of an option given twice; defaults; positional arguments in order, the ones after them as
// Rest(), and "--" making the next ones positional even when they start with '-'; a lone "-" is positional; errors: an
// unknown option, a flag given a value, an option without its value, a required argument missing; --help and -h;
// Parse() again starts afresh; the help text. And: -x=value; a value that starts with '-' (taken as the value); --help
// and a required argument after "--" (positional); IsSet() of a flag and of an argument; Value() of a name not
// declared; Rest() a copy.
PROCEDURE Main()
   LOCAL o := Declared()
   Show( o, { "--top", "5", "in.txt" } )
   Show( o, { "--top=7", "-i", "in.txt", "more", "and more" } )
   Show( o, { "-n", "3", "--ignore-case", "in.txt" } )
   Show( o, { "--top=", "in.txt" } )
   Show( o, { "--top=1", "--top=2", "in.txt" } )
   Show( o, { "in.txt", "--", "--top=9", "-i" } )
   Show( o, { "-", "--out", "o.txt" } )
   Show( o, { "--size", "in.txt" } )
   Show( o, { "--ignore-case=yes", "in.txt" } )
   Show( o, { "in.txt", "--top" } )
   Show( o, { "-i" } )
   Show( o, { "in.txt", "-x" } )
   Show( o, { "-h" } )
   Show( o, { "in.txt", "--help" } )
   Show( o, { "again.txt" } )
   Show( o, { "-n=3", "in.txt" } )
   Show( o, { "-i=x", "in.txt" } )
   Show( o, { "--top", "-5", "in.txt" } )
   Show( o, { "--top", "-h", "in.txt" } )
   Show( o, { "--", "--help" } )
   Show( o, { "--size=3", "in.txt" } )
   Show( o, { "--top=1", "--" } )
   ? "isset:", o:Parse( { "-i", "f", "s" } ), o:IsSet( "ignore-case" ), o:IsSet( "file" ), o:IsSet( "second" ), ;
     o:IsSet( "top" ), o:Value( "nothing" ) == NIL
   o:Parse( { "f", "s", "r1", "r2" } )
   AAdd( o:Rest(), "changed" )
   ? "rest copy:", Len( o:Rest() )
   ? o:Usage()
   o := CommandLine():New( "bare" )
   ? o:Parse( { "a", "b" } ), Len( o:Rest() ), o:Value( "none" ), o:IsSet( "none" )
   ? o:Usage()
RETURN

STATIC FUNCTION Declared()
   LOCAL o := CommandLine():New( "tool", "Does things to files." )
   o:AddOption( "top", "n", "how many", "10" )
   o:AddOption( "out", , "where to write" )
   o:AddFlag( "ignore-case", "i", "no letter case" )
   o:AddArgument( "file", "the input", .T. )
   o:AddArgument( "second", "another input" )
RETURN o

STATIC PROCEDURE Show( o, aArgs )
   LOCAL cArgs := "", c
   FOR EACH c IN aArgs
      cArgs += " [" + c + "]"
   NEXT
   ? LTrim( cArgs ) + ":"
   IF o:Parse( aArgs )
      ?? " top=" + Shown( o:Value( "top" ) ), "set=" + Shown( o:IsSet( "top" ) ), ;
         "out=" + Shown( o:Value( "out" ) ), "case=" + Shown( o:Value( "ignore-case" ) ), ;
         "file=" + Shown( o:Value( "file" ) ), "second=" + Shown( o:Value( "second" ) ), ;
         "rest=" + LTrim( Str( Len( o:Rest() ) ) ) + IIf( Empty( o:Rest() ), "", " last " + ATail( o:Rest() ) )
   ELSE
      ?? " fails:", IIf( o:HelpRequested(), "help", o:ErrorText() )
   ENDIF
RETURN

STATIC FUNCTION Shown( x )
   DO CASE
   CASE x == NIL
      RETURN "NIL"
   CASE ValType( x ) == "L"
      RETURN IIf( x, ".T.", ".F." )
   ENDCASE
RETURN "'" + x + "'"
