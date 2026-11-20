// Strings that are views of a buffer (an append's result of 512 bytes or more) wherever strings go: each view below is
// followed in its buffer by the bytes of a longer view made from it (appended at the tip), so anything that read past
// its length, or up to a NUL that is not there, would show them. The string functions and operators, hash keys,
// AScan() and ASort(), a DBF field and a memo field written and read back, a file named by a view (FCreate(), FOpen(),
// File(), FErase(), SAVE TO and RESTORE FROM: a relative path that long, checked where the system opens one; elsewhere
// each line holds as well), Do() of a function named by a view, an alias named by a view in an error, the macro
// operator, Str() and Transform() appended to a view, QOut() of a view. Then a large buffer the collector trims (its
// tip unmoved over collections), appended to after, and views kept across collections.
// The files start with "string_views", deleted at the end.
REQUEST DBFCDX
FIELD NAME, NOTES
MEMVAR m_cMacro, m_cSaved

PROCEDURE Main()
   LOCAL cBase := Replicate( "-", 520 ), v, w, h, a, n, cPlain, cFile, nHandle, e, lMade, nSize, i, cOld

   v := Viewed( cBase + "Hello, World  " )
   ? "len:", Len( v ), "substr:", SubStr( v, 521, 5 ), "left:", Left( v, 3 ), "right:", "[" + Right( v, 7 ) + "]"
   ? "at:", At( "World", v ), RAt( "-", v ), "Wor" $ v, "TAIL" $ v
   w := cBase + "Hello, World  "
   ? "compare:", v == w, v = w, v = cBase, v < w + "a", w < v, v != w, v + "x" == w + "x"
   ? "case and trim:", Right( Upper( v ), 14 ), "[" + Right( AllTrim( v ), 6 ) + "]", "[" + Right( RTrim( v ), 6 ) + "]"
   ? "strtran:", Right( StrTran( v, "World", "There" ), 14 ), Len( StrTran( v, "-" ) )
   ? "val:", Val( Viewed( Replicate( " ", 520 ) + "1234.5", "999" ) ), ;
      Val( Viewed( Replicate( "0", 522 ) + "77", "999" ) )

   h := { => }
   h[ v ] := 1
   h[ Viewed( cBase + "other key" ) ] := 2
   ? "hash keys:", h[ w ], h[ cBase + "other key" ], HHasKey( h, v ), Len( h ), HScan( h, 2 )

   a := { Viewed( cBase + "pear" ), Viewed( cBase + "apple" ), Viewed( cBase + "figs" ) }
   ? "ascan:", AScan( a, cBase + "apple" ), AScan( a, {| c | Right( c, 4 ) == "figs" } )
   ASort( a )
   ? "asort:", Right( a[ 1 ], 5 ), Right( a[ 2 ], 4 ), Right( a[ 3 ], 4 )

   m_cMacro := "abc"
   ? "macro:", &( Viewed( "m_cMacro + 'd'" + Replicate( " ", 520 ) ) ), ;
      &( Viewed( Replicate( " ", 520 ) + "Len( m_cMacro )" ) )

   n := 3.25
   ? "str appended:", Right( Viewed( cBase + "--n=" ) + Str( n, 6, 2 ), 8 ), ;
      Right( v + Transform( n, "@E 999.99" ), 8 ), Right( v + DToS( SToD( "20261120" ) ), 10 )

   ?? " qout:"
   QOut( Right( Viewed( Replicate( "q", 520 ) + "!" ), 3 ), Viewed( Replicate( "q", 520 ) + "!" ) == ;
      Replicate( "q", 520 ) + "!" )

   rddSetDefault( "DBFCDX" )
   dbCreate( "string_views", { { "NAME", "C", 250, 0 }, { "NOTES", "M", 10, 0 } } )
   USE string_views EXCLUSIVE NEW
   APPEND BLANK
   REPLACE NAME WITH v, NOTES WITH Viewed( Replicate( "m", 560 ) + "end" )
   APPEND BLANK
   REPLACE NAME WITH Viewed( "second" + cBase ), NOTES WITH Viewed( Replicate( "n", 524 ) )
   GO 1
   ? "dbf:", Len( NAME ), NAME == Left( v, 250 ), Len( NOTES ), Right( NOTES, 4 ), ValType( NOTES )
   GO 2
   ? "dbf 2:", Left( NAME, 6 ), Len( NOTES ), NOTES == Replicate( "n", 524 )
   CLOSE
   FErase( "string_views.dbf" )
   FErase( "string_views.fpt" )

   cPlain := Replicate( "./", 260 ) + "string_views_f" // a plain string: Replicate() makes no view
   cFile := Viewed( cPlain )
   nHandle := FCreate( cFile )
   lMade := nHandle >= 0
   IF lMade
      FWrite( nHandle, v )
      FClose( nHandle )
      nHandle := FOpen( cFile )
      nSize := FSeek( nHandle, 0, 2 )
      FClose( nHandle )
   ENDIF
   ? "file:", File( cFile ) == lMade, File( cPlain ) == lMade, File( cPlain + "TAIL" ), ;
      IIf( lMade, nSize == Len( v ), .T. )
   ? "erased:", FErase( cFile ) == IIf( lMade, 0, -1 ), File( cPlain )

   IF lMade
      m_cSaved := "kept"
      SAVE TO ( cFile ) ALL LIKE m_cSav*
      m_cSaved := "changed"
      RESTORE FROM ( cFile ) ADDITIVE
      ? "mem file:", m_cSaved == "kept", File( cPlain + ".mem" ), FErase( cPlain + ".mem" ) == 0
   ELSE
      ? "mem file:", .T., .T., .T.
   ENDIF

   ErrorBlock( {| oErr | Break( oErr ) } )
   BEGIN SEQUENCE
      Do( Viewed( "NO_SUCH_FUNCTION_" + Replicate( "N", 500 ) ) )
   RECOVER USING e
      ? "do:", e:subCode, e:operation == "NO_SUCH_FUNCTION_" + Replicate( "N", 500 )
   END SEQUENCE
   BEGIN SEQUENCE
      dbSelectArea( Viewed( "NO_SUCH_ALIAS_" + Replicate( "A", 500 ) ) )
   RECOVER USING e
      ? "alias error:", e:subCode, e:operation == "NO_SUCH_ALIAS_" + Replicate( "A", 500 )
   END SEQUENCE

   a := { Replicate( "g", 600000 ) }
   a[ 1 ] += "h"          // a view of a large buffer with as much room again
   cOld := a[ 1 ]
   FOR i := 1 TO 3
      __GcCollect()        // its tip unmoved: the collector gives its room back
   NEXT
   a[ 1 ] += "z"          // no room left: a copy
   a[ 1 ] += "y"
   ? "trimmed:", Len( cOld ), Right( cOld, 2 ), Len( a[ 1 ] ), Right( a[ 1 ], 4 ), Left( a[ 1 ], 600000 ) == ;
      Replicate( "g", 600000 )
   h := { Viewed( cBase + "kept" ), "" }
   FOR i := 1 TO 200     // views and buffers made and dropped across collections
      h[ 2 ] += Replicate( Chr( 65 + i % 26 ), 10 )
      IF i % 50 == 0
         __GcCollect()
      ENDIF
   NEXT
   ? "collections:", Right( h[ 1 ], 4 ), Len( h[ 2 ] ), Right( h[ 2 ], 10 ), SubStr( h[ 2 ], 491, 10 )
RETURN

// The text as a view at its buffer's tip, its buffer then holding more bytes past it (cTail, "TAIL" by default: a
// longer view, made at the tip, which is dropped): what reads the view must stop at its length.
STATIC FUNCTION Viewed( c, cTail )
   LOCAL a := { Left( c, 1 ) }, aMore
   a[ 1 ] += SubStr( c, 2 ) // a view (512 bytes or more)
   aMore := { a[ 1 ] }
   aMore[ 1 ] += IIf( cTail == NIL, "TAIL", cTail ) // right after it in the buffer
RETURN a[ 1 ]
