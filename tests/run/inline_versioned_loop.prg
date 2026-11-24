// Calls of functions inlined whose versioning test (its parameters hold no reference) goes straight into a loop's
// header, which passes the loop's first values: constants (an unused parameter, LOCALs that start as literals), a
// value the entry computes (not inlined, but called), an argument by reference (the body versioned for references),
// and a FOR EACH over the parameter (public functions keep their test). Each gives what the function gives when called.
PROCEDURE Main()
   LOCAL x := 5
   ? "constants:", Sum( { 1 } ), Sum( NIL ), Count( "abc" )
   ? "computed:", Timed( 3 ) >= 0
   ? "by reference:", Bump( @x ), x
   ? "for each:", Total( { 1, 2, 3 } ), Total( {} )
RETURN

FUNCTION Sum( o )
   LOCAL i, n := 0
   FOR i := 1 TO 3
      n += i
   NEXT
RETURN n

FUNCTION Count( c )
   LOCAL i := 0, s := ""
   DO WHILE i < 4
      s += "*"
      i++
   ENDDO
RETURN s

STATIC FUNCTION Timed( n )
   LOCAL t := Seconds(), i
   FOR i := 1 TO n
      t += 0
   NEXT
RETURN t

STATIC FUNCTION Bump( v )
   LOCAL i
   FOR i := 1 TO 3
      v++
   NEXT
RETURN v

FUNCTION Total( a )
   LOCAL e, n := 0
   FOR EACH e IN a
      n += e
   NEXT
RETURN n
