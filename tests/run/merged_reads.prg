// A read repeated with nothing between that could change what it reads gives the first one's value; anything that could
// change it in between keeps the second read: an instance variable read twice in one expression, and then after an
// assignment, a method that assigns it, an error whose block assigns it and substitutes a value; an element read twice,
// then after an assignment of it, AAdd() and ASize() of its array, a BEGIN SEQUENCE whose RECOVER reads it again after
// an error that changed it; reads in the THEN branch of one made before the IF, and after the IF joins; a hash's
// element (no merge); a value whose type changes in the loop that reads it twice (its guard lets it through, then not);
// integer products that overflow into doubles; a table's elements at the same computed position; an element read twice
// whose position is no number: each read raises its error; a read kept whole and the same read taken as a number twice,
// guarded, with a number guarded between, which turns into a string; a branch on an integer comparison, one way
// assigning what was read before it, joining.
STATIC s_nErrors := 0

CLASS Pt
   VAR x INIT 3
   VAR y INIT 4
   METHOD Bump() INLINE ::x += 10
ENDCLASS

PROCEDURE Main()
   LOCAL o := Pt():New(), a := { 5, 6, 7 }, h := { "k" => 1 }, i, n, c, bOld

   ? "member:", o:x * o:x + o:y * o:x, Assigned( o ), Bumped( o )
   bOld := ErrorBlock( {| e | o:x := 100, s_nErrors++, IIf( e:canSubstitute, 1, Break( e ) ) } )
   ? "error block:", Failing( o ), NoNumber( a, "k" ), "errors:", s_nErrors
   ErrorBlock( bOld )
   ? "element:", a[ 2 ] * a[ 2 ], ElementSet( a ), Added( a ), Resized( a )
   ? "recovered:", Recovered( { 1, 2, 3 } )
   ? "branches:", Branches( o, .T. ), Branches( o, .F. )
   ? "hash:", h[ "k" ] + HashSet( h )

   o := Pt():New()
   n := 0
   c := ""
   FOR i := 1 TO 6
      IF i == 4
         o:x := "s"
      ENDIF
      IF ValType( o:x ) == "N"
         n += o:x * o:x + o:x
      ELSE
         c += o:x + o:x
      ENDIF
   NEXT
   ? "types change:", n, c

   n := 0
   a := { 3037000499, 3037000500, 3037000501 }
   FOR i := 1 TO 3
      n += a[ i % 3 + 1 ] * a[ i % 3 + 1 ]
   NEXT
   ? "overflow:", n
   ? "table:", Table( 2 ), Table( 3 )
   bOld := ErrorBlock( {| e | s_nErrors++, IIf( e:canSubstitute, 0, Break( e ) ) } )
   s_nErrors := 0
   ? "unguarded first:", Unguarded(), "errors:", s_nErrors
   ErrorBlock( bOld )
   ? "joined:", Joined()
RETURN

STATIC FUNCTION Assigned( o )
   LOCAL n := o:x
   o:x := 20
RETURN n + o:x

STATIC FUNCTION Bumped( o )
   LOCAL n := o:x
   o:Bump()
RETURN n + o:x

// o:x * "s" fails: the error block assigns o:x and substitutes 1; the next o:x reads 100.
STATIC FUNCTION Failing( o )
   LOCAL n := o:x, m := o:x * "s"
RETURN n + m + o:x

STATIC FUNCTION NoNumber( a, k )
RETURN a[ k ] + a[ k ]

STATIC FUNCTION ElementSet( a )
   LOCAL n := a[ 1 ]
   a[ 1 ] := 50
RETURN n + a[ 1 ]

STATIC FUNCTION Added( a )
   LOCAL n := Len( a ) + a[ 3 ]
   AAdd( a, 9 )
RETURN n + Len( a ) + a[ 3 ]

STATIC FUNCTION Resized( a )
   LOCAL n := a[ 2 ]
   ASize( a, 1 )
   AAdd( a, 70 )
RETURN n + a[ 2 ]

// a[ 5 ] is out of bounds: the error block changes a[ 1 ] and BREAKs; RECOVER reads a[ 1 ] again.
STATIC FUNCTION Recovered( a )
   LOCAL n := a[ 1 ], m := 0
   BEGIN SEQUENCE WITH {| e | e:args[ 1 ][ 1 ] := 40, Break( e ) }
      m := a[ 1 ] + a[ 5 ]
   RECOVER
      m := a[ 1 ] + a[ 1 ]
   END
RETURN LTrim( Str( n ) ) + "/" + LTrim( Str( m ) ) + "/" + LTrim( Str( a[ 1 ] ) )

STATIC FUNCTION Branches( o, l )
   LOCAL n := o:x
   IF l
      n += o:x
   ELSE
      o:x := 1
   ENDIF
RETURN n + o:x

STATIC FUNCTION HashSet( h )
   LOCAL n := h[ "k" ]
   h[ "k" ] := 5
RETURN n + h[ "k" ]

STATIC FUNCTION Unguarded()
   LOCAL o := Pt():New(), t := { 1, 2, 3, 4 }, i, v, d, e, n := 0, c := ""
   FOR i := 1 TO 4
      v := o:x
      d := t[ i ]
      e := d * d - d
      n += o:x * o:x + e
      c += ValType( v )
      IF i == 2
         o:x := "s"
      ENDIF
   NEXT
RETURN LTrim( Str( n ) ) + " " + c

STATIC FUNCTION Joined()
   LOCAL o := Pt():New(), i, n := 0, m
   FOR i := 1 TO 4
      m := o:x
      IF i % 2 == 0
         o:x := i
      ENDIF
      n += o:x + m * 100
   NEXT
RETURN n

STATIC FUNCTION Table( k )
   LOCAL t := { 11, 22, 33, 44 }
RETURN t[ k % 4 + 1 ] * t[ k % 4 + 1 ]
