// A value read inside a loop that the loop takes as a number twice (an element, d := a[ i ], in d * d and d - 1) is
// guarded where it is read (subst.check guard), and the operations on it are typed, with no check after them; a value
// read once (b[ i ] * 2) keeps the check after its operation.
FUNCTION Spread( a, b, n )
   LOCAL i, d, s := 0, t := 0
   FOR i := 1 TO n
      d := a[ i ]
      s += d * d - d
      t += b[ i ] * 2
   NEXT
RETURN s + t
