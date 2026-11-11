// Hash rules of xbase's own: 1 and 1.0 are one key in a literal too, FOR EACH k => v over hashes, arrays and
// strings, HCopy and HMerge ranges and filters, keys that merge when letter case stops mattering, changes made
// to a hash by the blocks and loops that walk it, deep copies of deep and wide structures, and the collector
// running while hashes hold every kind of value; references to entries, which follow their key; positions after
// many deletions; STATIC hash literals; the errors of positions and keys.
PROCEDURE Main()
   LOCAL h, g, k, v, a, i, n, s, deep, node, e, blk

   h := { 1 => "um", 1.0 => "um ponto zero", 2.5 => "x", -0.0 => "zero" }
   ? Len( h ), h[ 1 ], h[ 0 ], HKeyAt( h, 1 ), HKeyAt( h, 3 )

   // FOR EACH k => v: the key and the value of each entry; v writes through, k is a copy.
   h := { "a" => 1, "b" => 2, "c" => 3 }
   k := "antes"
   v := "antes"
   FOR EACH k => v IN h
      ?? " " + k + "=" + LTrim( Str( v ) )
      v *= 10
      k := "mudou"
   NEXT
   ? k, v, h[ "a" ], h[ "b" ], h[ "c" ], Len( h )
   FOR EACH k => v IN h DESCEND
      ?? " " + k
      IF k == "b"
         EXIT
      ENDIF
   NEXT
   ? k, v
   FOR EACH k => v IN { "x", "y" }
      ?? " " + LTrim( Str( k ) ) + v
   NEXT
   FOR EACH k => v IN "xyz"
      ?? " " + LTrim( Str( k ) ) + v
   NEXT
   ?
   n := 0
   FOR EACH k => v IN { "lista" => { 1, 2, 3 }, "sub" => { "p" => { 4, 5 } } }
      IF ValType( v ) == "A"
         FOR EACH s IN v
            n += s
         NEXT
      ELSE
         FOR EACH g => a IN v
            n += a[ 1 ] * 100 + Len( g )
         NEXT
      ENDIF
   NEXT
   ? "aninhado", n

   // The body adds entries (seen) and deletes them (the positions after move down).
   h := { 1 => 1, 2 => 2, 3 => 3 }
   n := 0
   FOR EACH k => v IN h
      n++
      IF k == 1
         h[ 4 ] := 4
      ENDIF
      IF n > 10
         EXIT
      ENDIF
   NEXT
   ? "cresceu", n, Len( h )
   h := { 1 => 1, 2 => 2, 3 => 3, 4 => 4 }
   s := ""
   FOR EACH k => v IN h
      s += LTrim( Str( k ) )
      IF k == 2
         HDel( h, 1 )
      ENDIF
   NEXT
   ? "encolheu", s, Len( h )

   // HCopy( source, dest ) with ranges, HMerge( dest, source ) with a filter: a key already there gets the new
   // value, in its place.
   h := { "a" => 1, "b" => 2, "c" => 3, "d" => 4 }
   g := { "z" => 0, "b" => 0 }
   ? HCopy( h, g, 2, 2 ) == g
   ShowHash( g )
   HCopy( h, g, 4 )
   HCopy( h, g, 9 )
   HCopy( h, g, 1, 0 )
   ShowHash( g )
   g := { "a" => 0 }
   HMerge( g, h, {| k, v, p | v % 2 == 0 .OR. p == 1 } )
   ShowHash( g )
   HMerge( g, g )
   HCopy( g, g, 1, 2 )
   ShowHash( g )
   // A filter that changes or deletes the source's entry: the entry merged is the one it was given.
   h := { "a" => 1, "b" => 2, "c" => 3 }
   g := { => }
   HMerge( g, h, {| k, v | h[ k ] := v * 100, .T. } )
   ShowHash( g )
   h := { "a" => 1, "b" => 2, "c" => 3 }
   g := { => }
   HMerge( g, h, {| k | IIF( k == "a" .OR. k == "c", HDel( h, k ), NIL ), .T. } )
   ShowHash( g )

   // Letter case: keys that become equal merge into the first, which takes the last value.
   h := { "Nome" => 1, "x" => 2, "NOME" => 3, "nome" => 4, "X" => 5, "y" => 6 }
   HCaseMatch( h, .F. )
   ShowHash( h )
   ? h[ "nOmE" ], HPos( h, "Y" ), "X" $ h
   h[ "NOVO" ] := 7
   h[ "novo" ] += 1
   ShowHash( h )
   HCaseMatch( h, .T. )
   ? h[ "NOVO" ], "novo" $ h, HCaseMatch( h )
   g := HClone( h )
   HCaseMatch( h, .F. )
   ? HCaseMatch( g ), HCaseMatch( HClone( h ) )

   // Blocks that change the hash they walk.
   h := { 1 => "a", 2 => "b", 3 => "c", 4 => "d" }
   s := ""
   HEval( h, {| k, v | s += v, iif( k == 1, HDel( h, 3 ), NIL ) } )
   ? "heval", s, Len( h )
   h := { 3 => "c", 1 => "a", 2 => "b" }
   HSort( h, {| x, y | HDel( h, 2 ), x < y } )
   ShowHash( h )
   h := { 3 => "c", 1 => "a", 2 => "b" }
   HSort( h, {| x, y | x > y } )
   ShowHash( h )
   h := { 1 => 1, 2 => 2, 3 => 3 }
   ? HScan( h, {| k, v | HClear( h ), .F. } ), Len( h )

   // Deep and wide copies: no recursion, so depth has no limit; the copy is independent.
   deep := {=>}
   node := deep
   FOR i := 1 TO 100000
      node[ "n" ] := iif( i % 2 == 0, {=>}, { NIL } )
      node := node[ "n" ]
      IF ValType( node ) == "A"
         node[ 1 ] := {=>}
         node := node[ 1 ]
      ENDIF
   NEXT
   node[ "fim" ] := "fundo"
   g := HClone( deep )
   node := g
   n := 0
   DO WHILE ! "fim" $ node
      node := node[ "n" ]
      IF ValType( node ) == "A"
         node := node[ 1 ]
      ENDIF
      n++
   ENDDO
   node[ "fim" ] := "copia"
   node := deep
   DO WHILE ! "fim" $ node
      node := node[ "n" ]
      IF ValType( node ) == "A"
         node := node[ 1 ]
      ENDIF
   ENDDO
   ? "profundo", n, node[ "fim" ]
   a := Array( 2000 )
   FOR i := 1 TO Len( a )
      a[ i ] := { "i" => i, "v" => { i, { "q" => i * i } } }
   NEXT
   a[ 2000 ][ "eu" ] := a
   g := AClone( a )
   s := 0
   FOR EACH v IN g
      s += v[ "v" ][ 2 ][ "q" ]
      v[ "v" ][ 2 ][ "q" ] := 0
   NEXT
   ? "largo", s, a[ 10 ][ "v" ][ 2 ][ "q" ], g[ 2000 ][ "eu" ] == g, g[ 2000 ][ "eu" ] == a

   // The collector runs while hashes hold strings, arrays, hashes, blocks and dates, with deletions in between.
   h := {=>}
   FOR i := 1 TO 20000
      h[ "k" + LTrim( Str( i ) ) ] := { "s" => Replicate( Chr( 65 + i % 26 ), i % 50 ), ;
         "a" => { i, Str( i ) }, "b" => {| x | x + i }, "d" => SToD( "20240101" ) + i % 365 }
      IF i % 3 == 0
         HDel( h, "k" + LTrim( Str( i - 1 ) ) )
      ENDIF
      IF i % 5000 == 0
         __GcCollect()
      ENDIF
   NEXT
   __GcCollect()
   n := 0
   s := 0
   FOR EACH k => v IN h
      n++
      s += Len( v[ "s" ] ) + v[ "a" ][ 1 ] + Eval( v[ "b" ], 1 ) - v[ "a" ][ 1 ]
      IF Val( v[ "a" ][ 2 ] ) != v[ "a" ][ 1 ] .OR. k != "k" + LTrim( Str( v[ "a" ][ 1 ] ) )
         ? "errado", k
      ENDIF
   NEXT
   ? "coletor", n, s, h[ "k20000" ][ "s" ], DToS( h[ "k364" ][ "d" ] )

   // A reference to an entry designates its key: deleting entries before it does not move it to another one, and
   // writing it after its key is gone is the bound error. The current element of a loop, taken by a block, too.
   h := { "a" => 1, "b" => 2, "c" => 3 }
   DropFirstAndAdd( @h[ "c" ], h, 100 )
   ? "ref", h[ "c" ], Len( h ), HKeyAt( h, 1 ), HKeyAt( h, 2 )
   h := { "x" => 1, "y" => 2, "z" => 3 }
   FOR EACH k => v IN h
      IF k == "y"
         blk := {|| v }
         DropFirstAndAdd( @v, h, 10 )
      ENDIF
   NEXT
   HDel( h, "z" )
   h[ "y" ] += 1
   ? "loop ref", Eval( blk ), h[ "y" ], Len( h )
   h := {=>}
   HCaseMatch( h, .F. )
   h[ "abc" ] := 1
   CaseMattersAgain( @h[ "ABC" ], h )
   ? "spelling", h[ "abc" ], Len( h ), HCaseMatch( h )
   ErrorBlock( {| err | Break( err ) } )
   h := { "y" => 1 }
   BEGIN SEQUENCE
      DropFirstAndAdd( @h[ "y" ], h, 1 )
      ? "not reached"
   RECOVER USING e
      ? "gone", e:subCode, e:description, Len( h )
   END

   // A FOR EACH k => v whose key variable is the variable of an outer loop stores the keys into its element.
   a := { "", "" }
   FOR EACH k IN a
      FOR EACH g => v IN { "p" => 1, "q" => 2 }
         k := g
      NEXT
   NEXT
   ? "outer", a[ 1 ], a[ 2 ]
   a := { "", "" }
   FOR EACH s IN a
      FOR EACH s => v IN { "p" => 1, "q" => 2 }
      NEXT
   NEXT
   ? "outer key", a[ 1 ], a[ 2 ]

   // Positions after many deletions at the front, and deletions while walking: each step stays cheap.
   h := {=>}
   ? HAllocate( h, 30000 ) == h, HAllocate( h, -1 ) == h, Len( h )
   FOR i := 1 TO 30000
      h[ i ] := i * 2
   NEXT
   FOR i := 1 TO 29990
      HDelAt( h, 1 )
   NEXT
   ? "front", Len( h ), HKeyAt( h, 1 ), HValueAt( h, 10 ), HPos( h, 29995 ), HPos( h, 5 )
   h := {=>}
   FOR i := 1 TO 30000
      h[ "k" + LTrim( Str( i ) ) ] := i
   NEXT
   n := 0
   FOR EACH k => v IN h
      n++
      IF v % 2 == 1
         HDel( h, "k" + LTrim( Str( v + 1 ) ) )
      ENDIF
   NEXT
   ? "walk", n, Len( h ), HKeyAt( h, 2 ), HKeyAt( h, Len( h ) ), HPos( h, "k29999" )

   // A STATIC initialized with a hash literal keeps its contents between calls.
   ? "static", Counter( "a" ), Counter( "b" ), Counter( "a" ), Counter( "a" )

   // $ finds keys of every kind; positions outside 1 .. Len() are BASE/1187 in every H* function.
   h := { SToD( "20240101" ) => 1, 2.0 => 2, "s" => 3 }
   ? "in", SToD( "20240101" ) $ h, 2 $ h, 2.5 $ h, "S" $ h, "s" $ h
   FOR EACH k IN { 0, 4 }
      BEGIN SEQUENCE
         HDelAt( h, k )
      RECOVER USING e
         ? "delat", k, e:genCode, e:subCode, e:description, e:operation, Len( h )
      END
   NEXT
   BEGIN SEQUENCE
      ? .T. $ h
   RECOVER USING e
      ? "in error", e:subCode, e:description
   END
   ? "model", PositionsAgainstModel( 40000 )
RETURN

// Random insertions, deletions by key and by position and replaced values, on a hash and on an array of its keys
// in order: every position and every key's position must agree (the count of disagreements is returned).
STATIC FUNCTION PositionsAgainstModel( nOps )
   LOCAL h := {=>}, m := {}, nSeed := 12345, i, nOp, p, nBad := 0, nNext := 0, j
   FOR i := 1 TO nOps
      nSeed := ( nSeed * 1103515245 + 12345 ) % 2147483648
      nOp := nSeed % 10
      IF nOp < 5 .OR. Len( m ) == 0
         nNext++
         h[ nNext ] := nNext
         AAdd( m, nNext )
      ELSEIF nOp < 7
         p := iif( nOp == 5, nSeed % Len( m ), Int( nSeed / 7 ) % Len( m ) ) + 1
         IF nOp == 5
            HDel( h, m[ p ] )
         ELSE
            HDelAt( h, p )
         ENDIF
         ADel( m, p )
         ASize( m, Len( m ) - 1 )
      ELSEIF nOp == 7
         h[ m[ nSeed % Len( m ) + 1 ] ] := -1
      ELSE
         p := Int( nSeed / 3 ) % Len( m ) + 1
         IF HKeyAt( h, p ) != m[ p ] .OR. HPos( h, m[ p ] ) != p
            nBad++
         ENDIF
      ENDIF
      IF i % 3989 == 0
         nBad += iif( Len( h ) == Len( m ), 0, 1 )
         FOR j := 1 TO Len( m )
            IF HKeyAt( h, j ) != m[ j ] .OR. HPos( h, m[ j ] ) != j
               nBad++
            ENDIF
         NEXT
      ENDIF
   NEXT
RETURN Str( Len( h ) ) + Str( nBad )

// Removes the first entry of h and adds n to x, an entry of h passed by reference.
STATIC PROCEDURE DropFirstAndAdd( x, h, n )
   HDelAt( h, 1 )
   x += n
RETURN

// Makes letter case matter in h again, then assigns x, an entry of h passed by reference.
STATIC PROCEDURE CaseMattersAgain( x, h )
   HCaseMatch( h, .T. )
   x := 2
RETURN

STATIC FUNCTION Counter( cKey )
   STATIC s_counts := { "a" => 0, "b" => { 0 } }
   IF ValType( s_counts[ cKey ] ) == "A"
      RETURN ++s_counts[ cKey ][ 1 ]
   ENDIF
RETURN ++s_counts[ cKey ]

STATIC PROCEDURE ShowHash( h )
   LOCAL k, v
   ?
   FOR EACH k => v IN h
      ?? " " + Text( k ) + "=" + Text( v )
   NEXT
RETURN

STATIC FUNCTION Text( x )
   SWITCH ValType( x )
   CASE "C"
      RETURN x
   CASE "N"
      RETURN LTrim( Str( x ) )
   ENDSWITCH
RETURN ValType( x )
