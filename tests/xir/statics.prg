// STATICs read in a loop: one its initializer alone writes (a constant: its loads become it), one assigned elsewhere
// and never passed by reference (loaded directly), one passed by reference (loaded through its cell, if it holds one).
STATIC s_nStep := 3, s_nTotal := 0, s_nByRef := 1

FUNCTION Sum( n )
   LOCAL i
   FOR i := 1 TO n
      s_nTotal += s_nStep + s_nByRef
   NEXT
   Bump( @s_nByRef )
   RETURN s_nTotal

STATIC PROCEDURE Bump( n )
   n++
   RETURN
