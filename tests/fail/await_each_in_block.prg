// AWAIT FOR EACH misused: inside a codeblock, which cannot suspend the ASYNC function that made it (one error for the
// statement).
ASYNC FUNCTION Walk( a )
   LOCAL b := {||
      LOCAL x
      AWAIT FOR EACH x IN a
      NEXT
      RETURN NIL
   }
RETURN Eval( b )
