// Hashes: literals, keys (strings by case, numbers where 1 and 1.0 are one key, dates), insertion order,
// nesting of hashes and arrays in each other at any depth, assignment and compound assignment through
// nested elements, the H* functions, growth, deletions and reinsertions, and errors.
PROCEDURE Main()
   LOCAL h, g, a, e, i, n, s, k

   h := { "b" => 2, "a" => 1, 3 => "three", 1.5 => "x" }
   ? ValType( h ), Len( h ), h[ "a" ], h[ 3 ], h[ 3.0 ], h[ 1.5 ], Empty( h ), Empty( {=>} ), ValType( {=>} )
   h[ "A" ] := "upper"
   h[ SToD( "20240229" ) ] := "leap"
   ? h[ "a" ], h[ "A" ], h[ SToD( "20240229" ) ], Len( h ), "a" $ h, "z" $ h, 3 $ h, 3.0 $ h
   ? h == h, h == { "b" => 2 }
   ShowKeys( h )

   // Nesting: hashes in hashes, arrays in hashes, hashes in arrays, at any depth.
   h := { "cliente" => { "nome" => "JOAO", "end" => { "rua" => "A", "num" => 123 } }, ;
          "itens" => { { "cod" => 1, "qtd" => 2 }, { "cod" => 7, "qtd" => 5, "tags" => { "x", "y" } } }, ;
          "matriz" => { { 1, 2 }, { 3, 4 } } }
   ? h[ "cliente" ][ "nome" ], h[ "cliente" ][ "end" ][ "rua" ], h[ "cliente" ][ "end" ][ "num" ]
   ? Len( h[ "itens" ] ), h[ "itens" ][ 2 ][ "qtd" ], h[ "itens" ][ 2 ][ "tags" ][ 2 ], h[ "matriz" ][ 2 ][ 1 ]
   h[ "cliente" ][ "end" ][ "num" ] += 10
   h[ "itens" ][ 1 ][ "qtd" ]++
   ++h[ "itens" ][ 2 ][ "qtd" ]
   h[ "matriz" ][ 1 ][ 2 ] *= 100
   h[ "cliente" ][ "end" ][ "cep" ] := { "zona" => "S", "digitos" => { 0, 6, 0 } }
   h[ "cliente" ][ "end" ][ "cep" ][ "digitos" ][ 3 ] := 9
   AAdd( h[ "itens" ], { "cod" => 9, "qtd" => 1, "sub" => { "a" => { "b" => { "c" => "deep" } } } } )
   ? h[ "cliente" ][ "end" ][ "num" ], h[ "itens" ][ 1 ][ "qtd" ], h[ "itens" ][ 2 ][ "qtd" ], h[ "matriz" ][ 1 ][ 2 ]
   ? h[ "cliente" ][ "end" ][ "cep" ][ "zona" ], h[ "cliente" ][ "end" ][ "cep" ][ "digitos" ][ 3 ]
   ? Len( h[ "itens" ] ), h[ "itens" ][ 3 ][ "sub" ][ "a" ][ "b" ][ "c" ]
   n := 0
   FOR EACH g IN h[ "itens" ]
      n += g[ "qtd" ] * g[ "cod" ]
   NEXT
   ? "total", n
   a := { {=>}, { "k" => {} } }
   a[ 1 ][ "novo" ] := { 1, { "dois" => 2 } }
   AAdd( a[ 2 ][ "k" ], { "z" => 26 } )
   ? a[ 1 ][ "novo" ][ 2 ][ "dois" ], a[ 2 ][ "k" ][ 1 ][ "z" ], Len( a[ 2 ][ "k" ] )

   // A shared hash: two references see one object; HClone and AClone copy every level, keeping
   // shared and cyclic parts shared and cyclic in the copy; objects stay shared.
   g := h[ "cliente" ]
   g[ "nome" ] := "MARIA"
   ? h[ "cliente" ][ "nome" ]
   g := HClone( h[ "cliente" ] )
   g[ "nome" ] := "ANA"
   g[ "end" ][ "rua" ] := "B"
   g[ "end" ][ "cep" ][ "digitos" ][ 1 ] := 5
   ? h[ "cliente" ][ "nome" ], h[ "cliente" ][ "end" ][ "rua" ], h[ "cliente" ][ "end" ][ "cep" ][ "digitos" ][ 1 ]
   ? g[ "nome" ], g[ "end" ][ "rua" ], g[ "end" ][ "cep" ][ "digitos" ][ 1 ]
   g := { "self" => NIL, "lista" => { NIL, NIL }, "obj" => ErrorNew() }
   g[ "self" ] := g
   g[ "lista" ][ 1 ] := g
   g[ "lista" ][ 2 ] := g[ "lista" ]
   e := HClone( g )
   ? e == g, e[ "self" ] == e, e[ "lista" ][ 1 ] == e, e[ "lista" ][ 2 ] == e[ "lista" ], e[ "lista" ] == g[ "lista" ]
   ? e[ "obj" ] == g[ "obj" ]
   a := AClone( { g, g, { g[ "lista" ] } } )
   ? a[ 1 ] == a[ 2 ], a[ 1 ] == g, a[ 1 ][ "self" ] == a[ 1 ], a[ 3 ][ 1 ] == a[ 1 ][ "lista" ]
   ? a[ 1 ][ "obj" ] == g[ "obj" ]

   // The functions.
   h := { "x" => 10, "y" => 20, "z" => 30 }
   ? HGet( h, "y" ), HGetDef( h, "w", "def" ), HGetDef( h, "x", "def" ), HHasKey( h, "z" ), HHasKey( h, "w" )
   ? HSet( h, "w", 40 ) == h, Len( h ), HPos( h, "z" ), HPos( h, "nao" ), HKeyAt( h, 2 ), HValueAt( h, 4 )
   ? HValueAt( h, 4, 44 ), h[ "w" ], HPairAt( h, 1 )[ 1 ], HPairAt( h, 1 )[ 2 ]
   ? HScan( h, 30 ), HScan( h, 99 ), HScan( h, {| k, v | v > 15 } ), HScan( h, {| k, v | k == "w" } )
   ? HDel( h, "nao" ) == h, Len( h ), HDel( h, "y" ) == h, Len( h ), HKeys( h )[ 2 ], HValues( h )[ 2 ]
   ? HDelAt( h, 1 ) == h, Len( h ), HKeyAt( h, 1 )
   HEval( h, {| k, v, p | QQOut( " " + k + "=" + LTrim( Str( v ) ) + "@" + LTrim( Str( p ) ) ) } )
   h := { "c" => 3, "a" => 1, "b" => 2 }
   HSort( h )
   ShowKeys( h )
   HFill( h, 0 )
   ? h[ "a" ], h[ "c" ]
   HClear( h )
   ? Len( h ), "a" $ h
   h := Hash( "p", 1, "q", 2 )
   ? Len( h ), h[ "q" ]
   h := {=>}
   ? HCaseMatch( h, .F. )
   h[ "Abc" ] := 1
   ? h[ "ABC" ], h[ "abc" ], HKeys( h )[ 1 ], HCaseMatch( h )

   // Growth past many sizes, deletions and reinsertions: order and contents stay right.
   h := {=>}
   FOR i := 1 TO 3000
      h[ "k" + LTrim( Str( i ) ) ] := i
   NEXT
   FOR i := 1 TO 3000 STEP 3
      HDel( h, "k" + LTrim( Str( i ) ) )
   NEXT
   FOR i := 1 TO 300
      h[ i ] := -i
   NEXT
   n := 0
   s := 0
   FOR EACH g IN h
      n++
      s += g
   NEXT
   ? Len( h ), n, s, HKeyAt( h, 1 ), HKeyAt( h, 1999 ), HKeyAt( h, 2000 ), HKeyAt( h, Len( h ) )
   FOR i := 1 TO 3000 STEP 3
      h[ "k" + LTrim( Str( i ) ) ] := 0
   NEXT
   ? Len( h ), HKeyAt( h, Len( h ) ), h[ "k2" ], h[ "k2998" ], h[ 300 ]
   h := {=>}
   FOR i := 1 TO 2000
      h[ i % 7 ] := i
      IF i % 5 == 0
         HDel( h, i % 7 )
      ENDIF
   NEXT
   ShowKeys( h )

   // Errors.
   ErrorBlock( {| err | Break( err ) } )
   h := { "a" => 1 }
   FOR EACH k IN { "missing", .T., {}, NIL }
      BEGIN SEQUENCE
         e := h[ k ]
      RECOVER USING e
         ? "read", ValType( k ), e:subCode, e:description, e:operation, e:canSubstitute
      END
      BEGIN SEQUENCE
         h[ k ] := 1
         ? "set", ValType( k ), "ok"
      RECOVER USING e
         ? "set", ValType( k ), e:subCode, e:description, e:operation
      END
   NEXT
   BEGIN SEQUENCE
      e := { "z" => 0, .T. => 1 }
   RECOVER USING e
      ? "literal", e:genCode, e:subCode, e:description, e:operation, Len( e:args ), ValType( e:args[ 1 ] ), ;
        ValType( e:args[ 2 ] ), ValType( e:args[ 3 ] )
   END
   BEGIN SEQUENCE
      HGet( 5, "a" )
   RECOVER USING e
      ? "hget", e:subCode, e:description
   END
   h := { "a" => 1, "b" => 2 }
   FOR EACH k IN { {|| HKeyAt( h, 5 ) }, {|| HValueAt( h, 0 ) }, {|| HPairAt( h, 3 ) }, {|| Hash( "a", 1, "b" ) }, ;
                   {|| HHasKey( h, .T. ) }, {|| HPos( h, {} ) } }
      BEGIN SEQUENCE
         Eval( k )
      RECOVER USING e
         ? "fn", e:genCode, e:subCode, e:description, Len( e:args )
      END
   NEXT
   FOR EACH k IN { {|| Hash( "a", 1, "b" ) }, {|| Hash( "a", 1, {}, 2 ) } }
      BEGIN SEQUENCE
         Eval( k )
      RECOVER USING e
         ? "hash()", e:genCode, e:subCode, e:description, e:operation, Len( e:args ), ValType( ATail( e:args ) )
      END
   NEXT
   ErrorBlock( {| err | 42 } )
   k := .T.
   ? "substituted", h[ k ]
RETURN

STATIC PROCEDURE ShowKeys( h )
   LOCAL i
   ?
   FOR i := 1 TO Len( h )
      QQOut( " " + ValType( HKeyAt( h, i ) ) + ":" + KeyText( HKeyAt( h, i ) ) )
   NEXT
RETURN

STATIC FUNCTION KeyText( k )
   SWITCH ValType( k )
   CASE "C"
      RETURN k
   CASE "D"
      RETURN DToS( k )
   ENDSWITCH
RETURN LTrim( Str( k ) )
