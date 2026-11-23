// The call stack seen from functions inlined into their callers (at -O1 and above; -O0 inlines nothing, and shows the
// same): ProcName(), ProcLine() and ProcFile() at each level, through three bodies inlined into each other and a
// codeblock inlined where it is evaluated; an error raised inside an inlined body (the levels its handler sees); a body
// that reads its arguments as received (PCount(), a frame of its own) with another inlined into it.
PROCEDURE Main()
   LOCAL oErr
   ? Outer( 1 )
   ? Eval( {| x | Where( x ) }, 2 )
   ? Counted( 1, 2, 3 )
   BEGIN SEQUENCE WITH {| e | e:cargo := Levels(), Break( e ) }
      ? Failing( "a" )
   RECOVER USING oErr
      ? "error:", oErr:subCode, oErr:operation
      ? oErr:cargo
   END SEQUENCE
RETURN

STATIC FUNCTION Outer( n )
RETURN Middle( n + 1 )

STATIC FUNCTION Middle( n )
RETURN Where( n + 1 )

STATIC FUNCTION Where( n )
RETURN LTrim( Str( n ) ) + " " + Levels()

// The names and lines of the five innermost levels, and the file of the first.
STATIC FUNCTION Levels()
   LOCAL c := "", k
   FOR k := 0 TO 4
      c += ProcName( k ) + "(" + LTrim( Str( ProcLine( k ) ) ) + ") "
   NEXT
RETURN c + ProcFile( 0 )

STATIC FUNCTION Counted( ... )
RETURN LTrim( Str( PCount() ) ) + " " + Where( PCount() )

STATIC FUNCTION Failing( x )
RETURN Adding( x )

STATIC FUNCTION Adding( x )
RETURN x + 1

