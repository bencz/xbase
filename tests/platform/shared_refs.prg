// For platform.shared_refs: a generator of the numbers 1 .. nLast, each yielded once, that threads step; and a class.
FUNCTION Numbers( nLast )
   LOCAL n
   FOR n := 1 TO nLast
      YIELD n
   NEXT
RETURN

// An object of a compiled class, whose variable threads assign and read by messages.
FUNCTION NewBox()
RETURN TBox():New()

CLASS TBox
   VAR v INIT 0
ENDCLASS
