// Running out of stack: a call that would go past the limit raises the stack error (EG_LIMIT, "Stack overflow",
// the called function as the operation) before its body runs, whatever recurses (a function, a codeblock through
// Eval(), a method, a codeblock made from text, a function forwarding a list of arguments that grows at each level);
// a sequence recovers from it, the program goes on, and it can happen again.
#include "error.ch"

STATIC s_nDepth := 0

CLASS Node
   METHOD Down()
ENDCLASS

METHOD Down() CLASS Node
   ++s_nDepth
RETURN ::Down()

PROCEDURE Main()
   LOCAL b, oErr, i
   FOR i := 1 TO 2
      s_nDepth := 0
      BEGIN SEQUENCE WITH {| e | Break( e ) }
         Deep()
      RECOVER USING oErr
         ? "function:", oErr:genCode == EG_LIMIT, oErr:subCode, oErr:description, oErr:operation, s_nDepth > 1000
      END
   NEXT
   s_nDepth := 0
   b := {|| ++s_nDepth, Eval( b ) }
   TRY
      Eval( b )
   CATCH oErr
      ? "block:", oErr:genCode == EG_LIMIT, oErr:description, s_nDepth > 1000
   END
   s_nDepth := 0
   TRY
      Node():New():Down()
   CATCH oErr
      ? "method:", oErr:genCode == EG_LIMIT, oErr:description, oErr:operation, s_nDepth > 1000
   END
   s_nDepth := 0
   PRIVATE bMacro := &( "{|| Count(), Eval( bMacro ) }" )
   TRY
      Eval( bMacro )
   CATCH oErr
      ? "macro block:", oErr:genCode == EG_LIMIT, oErr:description, s_nDepth > 1000
   END
   s_nDepth := 0
   TRY
      Grow( 1 )
   CATCH oErr
      ? "argument list:", oErr:genCode == EG_LIMIT, oErr:description, s_nDepth > 1000
   END
   ? "still running:", Deep2( 100 )
RETURN

FUNCTION Count()
RETURN ++s_nDepth

FUNCTION Grow( ... )
   ++s_nDepth
RETURN Grow( s_nDepth, ... )

FUNCTION Deep()
   ++s_nDepth
RETURN Deep() + 1

FUNCTION Deep2( n )
RETURN iif( n == 0, 0, Deep2( n - 1 ) + 1 )
