// For platform.shared_compiled: compiled code on objects other threads reach. Writers store alternately integers and
// doubles (whole items: a torn one is an integer of a double's bits or a tiny double) into an array (by an integer and
// by a double index, through a FOR EACH variable, through a parameter passed @a[k]), into variables codeblocks capture
// (directly and through a parameter passed by reference), into a hash by key, and append to a captured string;
// readers read them back the same ways and by FOR EACH over a hash's keys and values, with a loop variable a codeblock
// captures and publishes (its cell becomes shared while the loop runs), and count what is not a value some writer
// stored.

// The objects the threads share: an array, the codeblocks over three captured variables, a hash, and the flag that
// keeps the readers going.
FUNCTION NewShared()
   LOCAL v := 0, w := 0, s := "", h := { "k0" => 0, "k1" => 0, "k2" => 0, "k3" => 0 }
   LOCAL aBlocks := { {| x | v := x }, {|| v }, {| c | s += c }, {|| s }, {|| s := "" }, {| x | Put( @w, x ) }, ;
                      {|| Get( @w ) } }
RETURN { AFill( Array( 16 ), 0 ), aBlocks, h, .T., NIL }

FUNCTION Writer( aShared, nRounds, nMe )
   LOCAL a := aShared[ 1 ], aBlocks := aShared[ 2 ], h := aShared[ 3 ], n, k, x, y
   FOR n := 1 TO nRounds
      x := iif( ( Int( n / 8 ) + nMe ) % 2 == 0, n, n + 0.5 )
      DO CASE
      CASE n % 4 == 0
         FOR k := 1 TO Len( a )
            a[ k ] := x
         NEXT
      CASE n % 4 == 1
         FOR k := 1 TO Len( a )
            a[ k + 0.0 ] := x
         NEXT
      CASE n % 4 == 2
         FOR EACH y IN a
            y := x
         NEXT
      OTHERWISE
         FOR k := 1 TO Len( a )
            Put( @a[ k ], x )
         NEXT
      ENDCASE
      Eval( aBlocks[ 1 ], x )
      Eval( aBlocks[ 6 ], x )
      h[ "k" + LTrim( Str( n % 4 ) ) ] := x
      Eval( aBlocks[ 3 ], "ab" )
      IF n % 64 == 0
         Eval( aBlocks[ 5 ] )
      ENDIF
      IF n % 1024 == 0
         __GcCollect( n % 8192 != 0 ) // a full collection moves objects
      ENDIF
   NEXT
RETURN NIL

FUNCTION Reader( aShared )
   LOCAL a := aShared[ 1 ], aBlocks := aShared[ 2 ], h := aShared[ 3 ], k, x, y, c, nBad := 0, nRounds := 0
   DO WHILE aShared[ 4 ]
      FOR k := 1 TO Len( a )
         IF !Valid( a[ k ] ) .OR. !Valid( a[ k + 0.0 ] ) .OR. !Valid( Get( @a[ k ] ) )
            ++nBad
         ENDIF
      NEXT
      FOR EACH y IN a // a loop variable no codeblock captures: its loop reference is the frame's
         IF !Valid( y )
            ++nBad
         ENDIF
      NEXT
      FOR EACH c => y IN h
         IF !Valid( y )
            ++nBad
         ENDIF
      NEXT
      k := 0
      FOR EACH x IN a
         IF ++k == 8
            aShared[ 5 ] := {|| x } // the block, and the cell of x, are shared from here on
         ENDIF
         IF !Valid( x ) .OR. ( k >= 8 .AND. !Valid( Eval( aShared[ 5 ] ) ) )
            ++nBad
         ENDIF
      NEXT
      IF !Valid( Eval( aBlocks[ 2 ] ) ) .OR. !Valid( Eval( aBlocks[ 7 ] ) )
         ++nBad
      ENDIF
      FOR EACH c => x IN h
         IF !( ValType( c ) == "C" .AND. Left( c, 1 ) == "k" ) .OR. !Valid( x )
            ++nBad
         ENDIF
      NEXT
      c := Eval( aBlocks[ 4 ] )
      IF !( ValType( c ) == "C" .AND. Len( c ) % 2 == 0 .AND. Empty( StrTran( c, "ab" ) ) )
         ++nBad
      ENDIF
      IF ++nRounds % 1024 == 0
         __GcCollect( .T. )
      ENDIF
   ENDDO
RETURN nBad

STATIC FUNCTION Valid( x )
RETURN ValType( x ) == "N" .AND. x >= 0 .AND. x < 4294967296 .AND. ( x == Int( x ) .OR. x - Int( x ) == 0.5 )

STATIC FUNCTION Put( r, x )
   r := x
RETURN NIL

STATIC FUNCTION Get( r )
RETURN r
