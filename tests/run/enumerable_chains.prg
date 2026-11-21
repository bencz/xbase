// Enumerable chains (the runtime's generators: Where, Select, Take, ...) at -O0 and -O2 (run0.*): over arrays of items,
// of integers and of logicals (compact forms, a NIL among them), strings, hashes, generators and objects; every
// operator; the order in which their blocks run (lazily, element by element, Take stopping early); a chain inside BEGIN
// SEQUENCE, with an error in one of its blocks (the error's call stack), a BREAK out of a block, a handler that gives
// the value back, and the chain run again after RECOVER; FOR EACH over a chain with EXIT and LOOP, chains nested in
// chains and in SelectMany; a block that captures a loop variable; an array that grows while a chain goes through it;
// a chain gone through twice (a generator's enumerator: once); a long array.
PROCEDURE Main()
   LOCAL aInt := { 5, 1, 4, 2, 3 }, aStr := { "pear", "fig", "apple", "kiwi" }, aLog := { .T., .F., .T. }
   LOCAL aNil := { 1, 2, 3 }, h := { "a" => 1, "b" => 2, "c" => 3 }, aLog2 := {}, x, c, n, e, g, a
   aNil[ 2 ] := NIL
   ? "sum:", aInt:Where( {| v | v % 2 == 1 } ):Select( {| v | v * 10 } ):Sum(), ;
      aInt:Sum(), aInt:Sum( {| v | v * v } ), {}:Sum()
   ? "count:", aInt:Count(), aInt:Count( {| v | v > 2 } ), "banana":Count( {| ch | ch == "a" } ), h:Count()
   ? "array:", Show( aInt:Where( {| v | v > 1 } ):ToArray() ), ;
      Show( aStr:Select( {| s | Upper( Left( s, 2 ) ) } ):ToArray() )
   ? "logicals:", aLog:Count( {| l | l } ), Show( aLog:Select( {| l | ! l } ):ToArray() ), aLog:All( {| l | l } ), ;
      aLog:Any( {| l | ! l } )
   ? "nil inside:", Show( aNil:Select( {| v | ValType( v ) } ):ToArray() ), aNil:Count( {| v | v == NIL } )
   ? "positions:", Show( aStr:Where( {| s, k | k % 2 == 0 } ):ToArray() ), ;
      Show( aInt:Select( {| v, k | v * k } ):ToArray() )
   ? "take/skip:", Show( aInt:Take( 2 ):ToArray() ), Show( aInt:Skip( 3 ):ToArray() ), ;
      Show( aInt:Take( 0 ):ToArray() ), ;
      Show( aInt:TakeWhile( {| v | v != 4 } ):ToArray() ), Show( aInt:SkipWhile( {| v | v != 4 } ):ToArray() )
   ? "order:", Show( aInt:OrderBy( {| v | v } ):ToArray() ), ;
      Show( aStr:OrderByDescending( {| s | Len( s ) } ):ToArray() ), ;
      Show( aInt:Reverse():ToArray() ), Show( "abcab":Distinct():ToArray() )
   ? "first/last:", aInt:First(), aInt:First( {| v | v < 3 } ), aInt:Last(), aStr:Last( {| s | "i" $ s } ), ;
      aInt:First( {| v | v > 99 } )
   ? "min/max/avg:", aInt:Min(), aInt:Max(), aInt:Average(), aStr:Min( {| s | Len( s ) } ), aStr:Max(), ;
      aInt:Aggregate( {| acc, v | acc * 10 + v } ), aInt:Contains( 4 ), aInt:Contains( 9 )
   ? "concat/zip:", Show( aInt:Take( 2 ):Concat( { 9, 8 } ):ToArray() ), ;
      Show( aStr:Zip( aInt, {| s, v | Left( s, 1 ) + Str( v, 1 ) } ):ToArray() ), ;
      Show( { 1, 2 }:Zip( { "a" } ):ToArray() )
   ? "select many:", Show( { "ab", "c" }:SelectMany( {| s | s:Select( {| ch | ch + ch } ) } ):ToArray() )
   ? "hash:", h:Sum(), Show( h:Where( {| v | v > 1 } ):ToArray() ), Show( h:Select( {| v | v * 2 } ):ToArray() )
   ? "string:", "hello world":Where( {| ch | ch $ "aeiou" } ):Count(), ;
      Show( "abc":Select( {| ch | Asc( ch ) } ):ToArray() )
   ? "generator:", Range( 1, 10 ):Where( {| v | v % 3 == 0 } ):Sum(), Show( Range( 1, 5 ):Reverse():ToArray() )
   ? "object:", Bag():New():Where( {| v | v > 10 } ):Count(), Show( Bag():New():Select( {| v | v / 10 } ):ToArray() )
   ? "empty:", {}:Count(), Show( {}:Where( {| v | v > 0 } ):ToArray() ), "":Sum(), {}:First(), {}:Any(), {}:All()

   // The order the blocks run in: element by element through the stages, Take stopping the source early.
   aInt:Where( {| v | AAdd( aLog2, "w" + Str( v, 1 ) ), v > 2 } ):Select( {| v | AAdd( aLog2, "s" + Str( v, 1 ) ), ;
      v } ):Take( 2 ):Sum()
   ? "order of evaluation:", Show( aLog2 )

   // A chain in a sequence: an error in a block, a BREAK out of one, a handler that gives a value, the chain again.
   BEGIN SEQUENCE WITH {| o | Break( o ) }
      n := aInt:Select( {| v | IIf( v == 4, v + "x", v ) } ):Sum()
      ? "not here"
   RECOVER USING e
      ? "error in a block:", e:Description, e:Operation
   END SEQUENCE
   BEGIN SEQUENCE WITH {| o | Break( { o:Description, o:Operation, ProcName( 1 ), ProcName( 2 ) } ) }
      n := aInt:Where( {| v | v / 0 > 1 .OR. v + "x" == "" } ):Count()
   RECOVER USING e
      ? "error with its stack:", e[ 1 ], e[ 2 ], e[ 3 ], e[ 4 ]
   END SEQUENCE
   BEGIN SEQUENCE
      n := aInt:Select( {| v | IIf( v == 2, Break( "broke at " + Str( v, 1 ) ), v ) } ):ToArray()
   RECOVER USING e
      ? "break in a block:", e
   END SEQUENCE
   BEGIN SEQUENCE WITH {| o | IIf( o:canSubstitute, 0, Break( o ) ) }
      ? "substituted:", aStr:Select( {| s | s * 2 } ):Sum(), aInt:Where( {| v | v % 2 == 0 } ):Sum()
   END SEQUENCE
   ? "again after recover:", aInt:Where( {| v | v % 2 == 1 } ):Select( {| v | v * 10 } ):Sum()
   c := ""
   BEGIN SEQUENCE
      FOR EACH x IN aInt:Where( {| v | v != 3 } )
         IF x == 1
            LOOP
         ENDIF
         IF x == 2
            BREAK x
         ENDIF
         c += Str( x, 1 )
      NEXT
   RECOVER USING e
      c += " broke at " + Str( e, 1 )
   END SEQUENCE
   ? "for each in a sequence:", c

   // FOR EACH over chains, nested.
   c := ""
   FOR EACH x IN aInt:Where( {| v | v > 1 } )
      IF x == 4
         LOOP
      ENDIF
      FOR EACH n IN aStr:Where( {| s | Len( s ) == x + 1 } )
         c += Str( x, 1 ) + n + " "
      NEXT
      IF x == 2
         EXIT
      ENDIF
   NEXT
   ? "nested:", c
   ? "chain of chains:", Show( aInt:Select( {| v | aInt:Where( {| w | w < v } ):Count() } ):ToArray() )
   c := ""
   FOR EACH x IN aInt
      c += Str( aStr:Count( {| s | Len( s ) > x } ), 1 )
   NEXT
   ? "block on a loop variable:", c

   // An array that grows while a chain goes through it; a chain gone through twice; a long array.
   a := { 1, 2, 3 }
   ? "growing:", a:Select( {| v | IIf( Len( a ) < 6, AAdd( a, v * 10 ), NIL ), v } ):Sum(), Len( a )
   g := aInt:Where( {| v | v > 2 } )
   ? "twice:", g:Sum(), g:Sum()
   a := Array( 1000 )
   AEval( a, {| v, k | a[ k ] := k } )
   ? "long:", a:Where( {| v | v % 7 == 0 } ):Select( {| v | v * 2 } ):Sum(), a:Skip( 990 ):Sum(), a:Take( 10 ):Sum()
RETURN

STATIC FUNCTION Show( a )
   LOCAL c := "", x
   FOR EACH x IN a
      c += "," + Text( x )
   NEXT
RETURN "{" + SubStr( c, 2 ) + "}"

STATIC FUNCTION Text( x )
   DO CASE
   CASE ValType( x ) == "C"
      RETURN x
   CASE ValType( x ) == "L"
      RETURN IIf( x, "T", "F" )
   CASE ValType( x ) == "A"
      RETURN Show( x )
   CASE x == NIL
      RETURN "NIL"
   ENDCASE
RETURN LTrim( Str( x ) )

FUNCTION Range( nFrom, nTo )
   LOCAL i
   FOR i := nFrom TO nTo
      YIELD i
   NEXT
RETURN

CLASS Bag INHERIT Enumerable
   METHOD New() INLINE Self
   METHOD GetEnumerator()
ENDCLASS

METHOD GetEnumerator() CLASS Bag
   YIELD 5
   YIELD 15
   YIELD 25
RETURN
