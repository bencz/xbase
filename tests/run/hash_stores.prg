// The two kinds of hash store: up to 8 entries a store has no index and is searched in order (the same key item
// first, then by hash), past that one with an index; the last entry a lookup found is tried first. Each rule below runs
// at every size from 1 to 20 keys, so across the change of kind both ways (growing, and a cleared store), with holes
// left by deletions (whose NIL keys never equal 0, 0.0 or anything), keys of every kind (integers, doubles, the
// doubles that are integers, -0.0, dates, strings made at run time and literals), lookups of the key just found,
// letter case turned off and on (keys that become one), positions through holes, and copies.
PROCEDURE Main()
   LOCAL n, cBad := ""
   FOR n := 1 TO 20
      cBad += Check( n )
   NEXT
   ? "sizes 1 to 20:", IIf( Empty( cBad ), "ok", cBad )
   ? "holes:", Holes()
   ? "case:", CaseRule( 5 ), CaseRule( 12 )
   ? "repeat:", Repeated()
   ? "kinds:", Kinds()
RETURN

// Keys 1..n as integers, doubles, strings made at run time and dates, each found, changed, deleted every other one.
STATIC FUNCTION Check( n )
   LOCAL h := { => }, i, c := "", aOrder := {}, o
   FOR i := 1 TO n
      h[ i ] := i * 10
      h[ i + 0.5 ] := -i
      h[ "k" + LTrim( Str( i ) ) ] := i
      h[ SToD( "20240101" ) + i ] := i
   NEXT
   FOR i := 1 TO n
      IF i % 2 == 0
         AAdd( aOrder, i )
      ENDIF
      AAdd( aOrder, i + 0.5 )
      IF i % 2 == 0
         AAdd( aOrder, "k" + LTrim( Str( i ) ) )
      ENDIF
      AAdd( aOrder, SToD( "20240101" ) + i )
   NEXT
   IF Len( h ) != 4 * n
      c += "len" + LTrim( Str( n ) ) + " "
   ENDIF
   FOR i := 1 TO n
      IF !( i $ h ) .OR. h[ i ] != i * 10 .OR. !( ( i + 0.5 ) $ h ) .OR. h[ "k" + LTrim( Str( i ) ) ] != i .OR. ;
            h[ SToD( "20240101" ) + i ] != i .OR. h[ i * 1.0 ] != i * 10
         c += "find" + LTrim( Str( n ) ) + " "
      ENDIF
      h[ i ] += 1
      h[ "k" + LTrim( Str( i ) ) ]++
   NEXT
   FOR i := 1 TO n STEP 2
      HDel( h, i )
      HDel( h, "k" + LTrim( Str( i ) ) )
   NEXT
   FOR i := 1 TO n
      IF ( i $ h ) != ( i % 2 == 0 ) .OR. ( ( "k" + LTrim( Str( i ) ) ) $ h ) != ( i % 2 == 0 )
         c += "del" + LTrim( Str( n ) ) + " "
      ELSEIF i % 2 == 0 .AND. ( h[ i ] != i * 10 + 1 .OR. h[ "k" + LTrim( Str( i ) ) ] != i + 1 )
         c += "value" + LTrim( Str( n ) ) + " "
      ENDIF
   NEXT
   IF 0 $ h .OR. 0.0 $ h .OR. "" $ h
      c += "nil" + LTrim( Str( n ) ) + " "
   ENDIF
   c += Positions( h, aOrder, n )
   o := HClone( h )
   FOR i := 1 TO Len( aOrder )
      IF !( aOrder[ i ] $ o ) .OR. !( o[ aOrder[ i ] ] == h[ aOrder[ i ] ] )
         c += "copy" + LTrim( Str( n ) ) + " "
      ENDIF
   NEXT
   FOR i := 1 TO n STEP 2
      h[ i ] := -1
   NEXT
   IF Len( h ) != 3 * n + Int( n / 2 ) .OR. HKeyAt( h, Len( h ) ) != n - 1 + n % 2
      c += "readd" + LTrim( Str( n ) ) + " "
   ENDIF
RETURN c

// Each key at its position, read through the holes the deletions left, and each position's key.
STATIC FUNCTION Positions( h, aOrder, n )
   LOCAL i, k
   FOR i := 1 TO Len( aOrder )
      k := HKeyAt( h, i )
      IF HPos( h, aOrder[ i ] ) != i .OR. ValType( k ) != ValType( aOrder[ i ] ) .OR. !( k == aOrder[ i ] )
         RETURN "pos" + LTrim( Str( n ) ) + " "
      ENDIF
   NEXT
RETURN IIf( Len( h ) == Len( aOrder ), "", "poslen" + LTrim( Str( n ) ) + " " )

// Holes (a deleted key's hash stays with its hole) and the keys 0 and 0.0, positions through holes, and the key at a
// position.
STATIC FUNCTION Holes()
   LOCAL h := { 1 => "a", 2 => "b", 3 => "c" }, c := "", h2
   HDel( h, 1 )
   c += IIf( 0 $ h, "0 found ", "" ) + IIf( 0.0 $ h, "0.0 found ", "" ) + IIf( -0.0 $ h, "-0.0 found ", "" )
   h[ 0 ] := "zero"
   h2 := { 0 => "a", 1 => "b", 2 => "c" }
   h2[ 0 ] := h2[ 0 ] + "!"
   HDel( h2, 0 )
   c += IIf( 0 $ h2 .OR. 0.0 $ h2, "deleted 0 found ", "" )
   h2[ 0.0 ] := "again"
   c += h2[ 0 ] + " " + LTrim( Str( Len( h2 ) ) ) + " "
   c += h[ -0.0 ] + " " + LTrim( Str( HPos( h, 3 ) ) ) + " " + LTrim( Str( HKeyAt( h, 3 ) ) ) + " " + ;
      LTrim( Str( Len( h ) ) )
RETURN c

// Letter case off then on in a store of each kind: keys that differ in case alone become one, the last value kept.
STATIC FUNCTION CaseRule( n )
   LOCAL h := { => }, i, c
   FOR i := 1 TO n
      h[ "Key" + LTrim( Str( i ) ) ] := i
   NEXT
   h[ "KEY1" ] := 100
   h[ "key2" ] := 200
   HCaseMatch( h, .F. )
   c := LTrim( Str( Len( h ) ) ) + " " + LTrim( Str( h[ "kEy1" ] ) ) + " " + LTrim( Str( h[ "KEY2" ] ) )
   HCaseMatch( h, .T. )
   c += " " + IIf( "kEy1" $ h, "kEy1 found", "kEy1 not found" ) + " " + LTrim( Str( Len( h ) ) )
RETURN c

// The same key looked up again and again, changed between, and a key deleted right after it was found.
STATIC FUNCTION Repeated()
   LOCAL h := { => }, i, k
   FOR i := 1 TO 30
      k := "w" + Chr( 48 + i % 7 )
      IF k $ h
         h[ k ] += 1
      ELSE
         h[ k ] := 1
      ENDIF
      IF i == 20 .AND. k $ h
         HDel( h, k )
      ENDIF
   NEXT
RETURN LTrim( Str( Len( h ) ) ) + " " + LTrim( Str( h[ "w0" ] ) ) + " " + LTrim( Str( h[ "w6" ] ) ) + " " + ;
   IIf( "w6" $ h, "w6", "-" )

// An integer and the double equal to it are one key; a double with a fraction, a date and a string are others.
STATIC FUNCTION Kinds()
   LOCAL h := { 1 => "int" }
   h[ 1.0 ] := "double"
   h[ 1.5 ] := "fraction"
   h[ SToD( "19700102" ) ] := "date"
   h[ "1" ] := "string"
RETURN LTrim( Str( Len( h ) ) ) + " " + h[ 1 ] + " " + h[ 1.5 ] + " " + h[ "1" ]
