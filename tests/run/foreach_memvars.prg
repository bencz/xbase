// FOR EACH over memory variables: a PRIVATE and a PUBLIC hold a reference to the element, so a function called in the
// body, a macro and a codeblock read it, and an assignment writes the element; the variable has its value back after
// the loop, after EXIT, after a RETURN out of the loop (its caller sees it) and after a BREAK through it; DESCEND,
// hashes, strings, several collections at once, nested loops over one variable, a PRIVATE of the same name made in
// the body (it hides the loop's for the rest of the iteration), FOR EACH k => v with memory variables, and a name no
// variable has (error 1003).
MEMVAR pv, pu, pk, pw, nosuch

PROCEDURE Main()
   LOCAL a := { 1, 2, 3 }, h := { "a" => 1, "b" => 2 }, b, e
   PRIVATE pv := "before", pk := "key"
   PUBLIC pu := "public", pw := "w"
   b := {|| M->pv }
   FOR EACH pv IN a
      ?? pv, Twice(), &( "pv" ), Eval( b ), ""
      pv *= 10
   NEXT
   ? "after:", pv, a[ 1 ], a[ 2 ], a[ 3 ]
   FOR EACH pu IN h
      ?? pu, ""
      pu += 100
   NEXT
   ? "hash:", pu, h[ "a" ], h[ "b" ]
   FOR EACH pv IN "xyz" DESCEND
      ?? pv, ""
   NEXT
   ? "string:", pv
   FOR EACH pv IN { 1, 2, 3 }
      IF pv == 2
         EXIT
      ENDIF
   NEXT
   ? "exit:", pv
   ? "return:", FromLoop(), pv
   BEGIN SEQUENCE
      FOR EACH pv IN { "x", "y" }
         Break( pv )
      NEXT
   RECOVER USING e
      ? "break:", e, pv
   END SEQUENCE
   FOR EACH pv, pu IN { 1, 2, 3 }, { "p", "q" }
      ?? pv, pu, ""
   NEXT
   ? "two:", pv, pu
   FOR EACH pv IN { 1, 2 }
      FOR EACH pv IN { "in1", "in2" }
         ?? pv, ""
      NEXT
      ?? "/", pv, ""
   NEXT
   ? "nested:", pv
   FOR EACH pv IN { 1, 2 }
      Shadow()
      ?? pv, ""
   NEXT
   ? "shadowed:", pv
   FOR EACH pk => pw IN { "k1" => "v1", "k2" => "v2" }
      ?? pk, pw, ""
   NEXT
   ? "key:", pk, pw
   ErrorBlock( {| x | Break( x ) } )
   BEGIN SEQUENCE
      FOR EACH nosuch IN { 1 }
         ? "not reached"
      NEXT
   RECOVER USING e
      ? "missing:", e:subCode, e:description, e:operation
   END SEQUENCE
RETURN

STATIC FUNCTION Twice()
RETURN M->pv * 2

STATIC FUNCTION FromLoop()
   FOR EACH pv IN { "r1", "r2" }
      IF pv == "r2"
         RETURN pv + "!"
      ENDIF
   NEXT
RETURN "none"

// A PRIVATE of the loop variable's name, made in the body by a function: gone when it returns.
STATIC PROCEDURE Shadow()
   PRIVATE pv := "shadow"
   ?? pv, ""
RETURN
