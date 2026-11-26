// A value read inside a loop and taken as a number twice is guarded once where it is read: elements that are numbers
// on every pass (integers, doubles, both), then an element that is a string on some passes, whose operations fail and
// whose error block substitutes a result (the guard leaves the typed loop there; the generic code computes the rest
// as the operators do), counted, with the same sums as the reference; an object's variable read the same way.
STATIC s_nErrors := 0

PROCEDURE Main()
   LOCAL aInts := { 1, 2, 3, 4, 5 }, aMixed := { 1, 2.5, 3, 4.25, 5 }, aOdd := { 1, 2, "x", 4, "y", 6 }, bOld, o
   ? "integers:", Spread( aInts ), "doubles too:", Spread( aMixed )
   bOld := ErrorBlock( {| e | s_nErrors++, IIf( e:canSubstitute, 100, Break( e ) ) } )
   ? "a string among them:", Spread( aOdd ), "errors:", s_nErrors
   ErrorBlock( bOld )
   o := Box():New()
   ? "object:", Twice( o, 4 ), o:n
RETURN

STATIC FUNCTION Spread( a )
   LOCAL i, d, s := 0
   FOR i := 1 TO Len( a )
      d := a[ i ]
      s += d * d - d
   NEXT
RETURN s

STATIC FUNCTION Twice( o, n )
   LOCAL i, s := 0
   FOR i := 1 TO n
      s += o:n * 2 + o:n
      o:n++
   NEXT
RETURN s

CLASS Box
   VAR n INIT 3
ENDCLASS
