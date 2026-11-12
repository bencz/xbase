// The collector: 40,000 slots holding strings of 16 to 2,000 bytes and small arrays of them, replaced at random
// 3,000,000 times, so objects of every size live for a while and die among others that stay (the heap's
// fragmentation), while a list of 20,000 records that live to the end grows beside them.
STATIC s_nSeed := 7

PROCEDURE Main()
   LOCAL t := ElapsedMs(), aSlots := Array( 40000 ), aKept := {}, i, n, x, nCheck := 0
   FOR i := 1 TO 3000000
      n := Rand( Len( aSlots ) ) + 1
      IF Rand( 4 ) == 0
         x := { Replicate( Chr( 65 + i % 26 ), 16 + Rand( 200 ) ), i, Space( Rand( 100 ) ) }
      ELSE
         x := Replicate( Chr( 97 + i % 26 ), 16 + Rand( 2000 ) )
      ENDIF
      aSlots[ n ] := x
      IF i % 150 == 0
         AAdd( aKept, { i, Str( i ), Replicate( "k", 64 ) } )
      ENDIF
   NEXT
   FOR EACH x IN aSlots
      IF ValType( x ) == "C"
         nCheck += Len( x )
      ELSEIF ValType( x ) == "A"
         nCheck += Len( x[ 1 ] ) + Len( x[ 3 ] )
      ENDIF
   NEXT
   ? "gc", ElapsedMs() - t, IIf( Len( aKept ) == 20000 .AND. nCheck > 0, "ok", "WRONG" )
   RETURN

STATIC FUNCTION Rand( n )
   s_nSeed := ( s_nSeed * 1103515245 + 12345 ) % 2147483648
RETURN Int( s_nSeed / 65536 ) % n
