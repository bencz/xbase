// CHOOSES: Max() and Min() of integers give an integer (one of their arguments), and Int() of an integer past 2^53
// divided by a constant keeps a range, so the inner FOR's start and limit, made outside it, get guards against the
// integers alone (guard 4) rather than against any number.
FUNCTION Limits( n )
   LOCAL i, j, s := 0, nRange := Max( Int( Max( Len( n ), 4 ) / 2 ) - 1, 0 )
   FOR i := 1 TO Len( n )
      FOR j := Max( 1, i - nRange ) TO Min( Len( n ), i + nRange )
         s += j
      NEXT
   NEXT
RETURN s
