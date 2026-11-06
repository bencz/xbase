// The call stack: ProcName() and ProcLine() at each level, codeblocks as
// (b)NAME, runtime functions that call blocks (AScan, ASort, AEval) or raise
// an error (SubStr, a message, Eval of a non-block) as NAME(0) while the
// error block runs, and a function versioned for references shown once.
PROCEDURE Main()
   LOCAL a := { 3, 1, 2 }, b := {|| Show( "block" ) }, x := 1
   Show( "main" )
   F()
   Eval( b )
   AEval( { 1 }, {|| Show( "aeval" ) } )
   AScan( a, {|| Show( "ascan" ), .T. } )
   ASort( a,,, {| p, q | Show( "asort" ), p < q } )
   ByRef( @x )
   ByRef( x )
   ErrorBlock( {| e | Show( e:operation ), Break( e ) } )
   BEGIN SEQUENCE
      ? 1 + "a"
   END
   BEGIN SEQUENCE
      ? SubStr( 1, 1 )
   END
   BEGIN SEQUENCE
      ? ErrorNew():foo()
   END
   BEGIN SEQUENCE
      ( 1 ):foo()
   END
   BEGIN SEQUENCE
      ErrorNew():tries := "x"
   END
   BEGIN SEQUENCE
      Eval( 1 )
   END
   BEGIN SEQUENCE
      ASize( 1, 1 )
   END
   BEGIN SEQUENCE
      AEval( { 1 }, {| v | v + "a" } )
   END
   ? ProcName(), ProcLine(), ProcName( 1 ), ProcLine( 1 ), "[" + ProcName( 5 ) + "]", ProcLine( 5 )
   ? ProcName( -1 ), ProcLine( -1 ), ProcName( "x" ), ProcLine( "x" ), ProcName( 0.9 )
RETURN

STATIC PROCEDURE F()
   Show( "f" )
RETURN

PROCEDURE ByRef( n )
   n++
   Show( "byref" )
RETURN

PROCEDURE Show( c )
   LOCAL n := 1
   ? c + ":"
   DO WHILE ! Empty( ProcName( n ) )
      ?? " " + ProcName( n ) + "(" + LTrim( Str( ProcLine( n ) ) ) + ")"
      n++
   ENDDO
RETURN
