// x := x + y on what is not a plain LOCAL appends in amortized constant time: an array element, a hash value, an
// object's variable, a PRIVATE, a PUBLIC, a STATIC and a LOCAL a codeblock sees grow by views of a buffer (an append
// whose left operand ends at its buffer's tip writes there), written x += y or x := x + y. For each target, the bytes
// allocated by 80,000 one-byte appends against those of 20,000 (counted by the collector: the same on every run,
// however loaded the machine): linear growth allocates about 3.5 times as much, under 100 bytes an append; quadratic
// growth (every append copying the string) 16 times, gigabytes. Then copies taken between appends keep their values:
// a copy of an element, two appends to one view, an append to a view that is no longer at its buffer's tip, a `+`
// that is no append, a view appended to itself, a LOCAL that holds a view, `-`, an empty operand, and appends written
// out.
#define SMALL 20000
#define LARGE 80000

STATIC s_cGrown
MEMVAR m_cPrivate, m_cPublic

PROCEDURE Main()
   LOCAL aTargets := { ;
      { "array element", {| n | ToElement( n ) } }, ;
      { "element, written out", {| n | ToElementLong( n ) } }, ;
      { "hash value", {| n | ToHashValue( n ) } }, ;
      { "object variable", {| n | ToObjectVar( n ) } }, ;
      { "private", {| n | ToPrivate( n ) } }, ;
      { "public", {| n | ToPublic( n ) } }, ;
      { "static", {| n | ToStatic( n ) } }, ;
      { "captured local", {| n | ToCaptured( n ) } } }
   LOCAL aT, nSmall, nLarge
   PUBLIC m_cPublic
   FOR EACH aT IN aTargets
      nSmall := Allocated( aT[ 2 ], SMALL )
      nLarge := Allocated( aT[ 2 ], LARGE )
      ? aT[ 1 ], Eval( aT[ 2 ], LARGE ), Eval( aT[ 2 ], SMALL ), ;
         IIf( nLarge < 6 * nSmall .AND. nLarge < 100 * LARGE, "linear", ;
            "NOT LINEAR " + Str( nSmall ) + Str( nLarge ) )
   NEXT
   Aliasing()
RETURN

// The bytes building a string of n bytes allocates (the collector counts them whatever the machine does meanwhile).
STATIC FUNCTION Allocated( bBuild, n )
   LOCAL nBefore := __GcMemory()[ 5 ]
   Eval( bBuild, n )
RETURN __GcMemory()[ 5 ] - nBefore

// Each builds a string of n bytes by n appends of one byte, checks every byte, and returns its length.
STATIC FUNCTION ToElement( n )
   LOCAL a := { "" }, i
   FOR i := 1 TO n
      a[ 1 ] += Chr( 65 + i % 26 )
   NEXT
RETURN Checked( a[ 1 ], n )

STATIC FUNCTION ToElementLong( n )
   LOCAL a := { "" }, i
   FOR i := 1 TO n
      a[ 1 ] := a[ 1 ] + Chr( 65 + i % 26 )
   NEXT
RETURN Checked( a[ 1 ], n )

STATIC FUNCTION ToHashValue( n )
   LOCAL h := { "k" => "" }, i
   FOR i := 1 TO n
      h[ "k" ] += Chr( 65 + i % 26 )
   NEXT
RETURN Checked( h[ "k" ], n )

STATIC FUNCTION ToObjectVar( n )
   LOCAL o := TGrown():New(), i
   FOR i := 1 TO n
      o:cText += Chr( 65 + i % 26 )
   NEXT
RETURN Checked( o:cText, n )

STATIC FUNCTION ToPrivate( n )
   LOCAL i
   PRIVATE m_cPrivate := ""
   FOR i := 1 TO n
      m_cPrivate += Chr( 65 + i % 26 )
   NEXT
RETURN Checked( m_cPrivate, n )

STATIC FUNCTION ToPublic( n )
   LOCAL i
   m_cPublic := ""
   FOR i := 1 TO n
      m_cPublic += Chr( 65 + i % 26 )
   NEXT
RETURN Checked( m_cPublic, n )

STATIC FUNCTION ToStatic( n )
   LOCAL i
   s_cGrown := ""
   FOR i := 1 TO n
      s_cGrown += Chr( 65 + i % 26 )
   NEXT
RETURN Checked( s_cGrown, n )

STATIC FUNCTION ToCaptured( n )
   LOCAL c := "", i, bSeen := {|| c }
   FOR i := 1 TO n
      c += Chr( 65 + i % 26 )
   NEXT
RETURN Checked( Eval( bSeen ), n )

// The length when every byte is the one its append wrote, else -1.
STATIC FUNCTION Checked( c, n )
   LOCAL i
   IF Len( c ) != n
      RETURN -1
   ENDIF
   FOR i := 1 TO n
      IF Asc( SubStr( c, i, 1 ) ) != 65 + i % 26
         RETURN -1
      ENDIF
   NEXT
RETURN n

STATIC PROCEDURE Aliasing()
   LOCAL a := { Replicate( "a", 520 ) }, x, s, aB, cOld, aSelf, cLocal, c, aE, i, h
   a[ 1 ] += "b"          // a view at its buffer's tip
   x := a[ 1 ]
   a[ 1 ] += "y"
   ? "copy of an element:", Len( x ), Right( x, 2 ), Len( a[ 1 ] ), Right( a[ 1 ], 3 )

   s := a[ 1 ]
   aB := { s, s }         // at the tip: the first append takes it, the second copies
   aB[ 1 ] += "1"
   aB[ 2 ] += "2"
   ? "two appends to one view:", Right( aB[ 1 ], 3 ), Right( aB[ 2 ], 3 ), Right( s, 3 ), Len( aB[ 1 ] ), ;
      Len( aB[ 2 ] ), Len( s )
   aB[ 1 ] += "+"
   aB[ 2 ] += "-"
   ? "then each grows:", Right( aB[ 1 ], 4 ), Right( aB[ 2 ], 4 ), Right( s, 3 ), aB[ 1 ] == s + "1+", ;
      aB[ 2 ] == s + "2-"

   cOld := a[ 1 ]
   a[ 1 ] += "N"          // cOld is no longer at the tip
   aB := { cOld }
   aB[ 1 ] += "O"         // copies
   a[ 1 ] += "P"
   ? "older view:", Right( cOld, 3 ), Right( aB[ 1 ], 3 ), Right( a[ 1 ], 3 ), Len( cOld ), Len( aB[ 1 ] ), ;
      Len( a[ 1 ] )

   x := a[ 1 ] + "Z"      // not an append: a string of its own, the tip stays
   a[ 1 ] += "W"
   ? "not an append:", Right( x, 3 ), Right( a[ 1 ], 3 ), Len( x ), Len( a[ 1 ] )

   aSelf := { a[ 1 ] }
   aSelf[ 1 ] += aSelf[ 1 ]
   ? "appended to itself:", Len( aSelf[ 1 ] ), aSelf[ 1 ] == a[ 1 ] + a[ 1 ], Right( a[ 1 ], 3 )

   cLocal := a[ 1 ]       // a LOCAL that holds a view grows a copy of its own
   cLocal += "L"
   cLocal += "M"
   c := ""
   c += a[ 1 ]            // a view appended to a LOCAL
   c += a[ 1 ]
   ? "locals:", Right( cLocal, 3 ), Right( a[ 1 ], 3 ), Len( c ), c == a[ 1 ] + a[ 1 ]

   x := a[ 1 ] - "Q"      // views as operands of -
   s := ( a[ 1 ] + "  " ) - "R"
   ? "minus:", Right( x, 3 ), Len( x ), Right( s, 4 ), Len( s ), Right( a[ 1 ], 3 )

   aE := { a[ 1 ], "" }
   aE[ 1 ] += ""          // an empty operand gives the other one
   aE[ 2 ] += a[ 1 ]
   aE[ 1 ] += "E"
   ? "empty operands:", aE[ 1 ] == a[ 1 ] + "E", aE[ 2 ] == a[ 1 ], Len( aE[ 1 ] ), Len( aE[ 2 ] ), Right( a[ 1 ], 3 )

   aE := { "" }           // written out, and chains of + after the target
   h := { "k" => "" }
   FOR i := 1 TO 300
      aE[ 1 ] := aE[ 1 ] + "x" + "y"
      h[ "k" ] := h[ "k" ] + Str( i % 10, 1 )
      h[ "k" ] += "-" + "+"
   NEXT
   ? "written out:", aE[ 1 ] == Replicate( "xy", 300 ), Len( h[ "k" ] ), Right( h[ "k" ], 6 )
RETURN

CLASS TGrown
   VAR cText INIT ""
ENDCLASS
