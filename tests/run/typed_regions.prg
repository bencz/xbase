// Typed continuations by regions, in functions that are not copied whole (one with a BEGIN SEQUENCE, whose handler
// unwinds; one too large): each loop nest is copied on its own, and the values that leave it join the original's;
// a loop after it gets a guard of each join, two joins of one block guarded one after the other. Values of other
// types enter the copies: an integer product that overflows into a double, a number the error block substitutes for
// a string's subtraction; each goes on through the join into the next loops, whose guards then fail. A guard of a
// block parameter (a value a loop carries into the next) stays first in its block when the loop it opens is peeled.
PROCEDURE Main()
   ErrorBlock( {| e | IIf( e:genCode == 1, 0.5, 0 ) } )
   ? "small:", Sequenced( 3, 2 )
   ? "overflow:", Sequenced( 2000000000000, 2 )
   ? "substituted:", Sequenced( 3, "x" )
   ? "both:", Sequenced( 2000000000000, "x" )
   ? "large small:", Large( 3, 2 )
   ? "large overflow:", Large( 2000000000000, 2 )
   ? "large substituted:", Large( 3, "x" )
   ? "peeled:", Peeled( 3, "abc" ), Peeled( 2.5, "ab" ), Peeled( "s", "" )
RETURN

STATIC FUNCTION Peeled( a, n )
   LOCAL x := a, s := 0, i, j, k
   k := Len( n )
   FOR i := 1 TO 10
      x := x + 1
      s += 0.5
   NEXT
   FOR j := 1 TO x
      k := k + j
   NEXT
RETURN s + k

// n and m leave the first loop together (a block with two joins); k leaves the second.
STATIC FUNCTION Sequenced( nStart, xStep )
   LOCAL n := nStart, m := 1, k := 0, i, j, c := ""
   BEGIN SEQUENCE
      c := "seq"
   END SEQUENCE
   FOR i := 1 TO 5
      n := n * 1000 + i
      m := m + i
   NEXT
   FOR j := 1 TO 3
      k := k + n % 7 + m * 2
      m := m - ( xStep - j )
   NEXT
   FOR i := 1 TO 4
      k := k * 2 - m + i
   NEXT
RETURN c + " " + Shown( n ) + " " + Shown( m ) + " " + Shown( k )

STATIC FUNCTION Shown( x )
RETURN IIf( ValType( x ) == "N", LTrim( Str( x, 30, 2 ) ), ValType( x ) )

// The same steps (the first loop's two values joined in the block after it), then more loops than the copy of a whole
// function takes.
STATIC FUNCTION Large( nStart, xStep )
   LOCAL n := nStart, m := 1, k := 0, i, j, s := 0, t := 0, u := 0, v := 0, w := 0
   FOR i := 1 TO 5
      n := n * 1000 + i
      m := m + i
   NEXT
   IF nStart > 100
      k := 1
   ENDIF
   FOR j := 1 TO 3
      k := k + n % 7 + m * 2 + n % 3
      m := m - ( xStep - j )
   NEXT
   FOR i := 1 TO 4
      k := k * 2 - m + i
   NEXT
   FOR i := 1 TO 3
      s := s + k % 11 + i * 3 - Int( m / 2 ) + ( i * i ) % 5 - Int( i / 3 ) * 2 + k % 13 - i
      t := t + s % 17 + i * 5 - Int( s / 7 ) + ( s * 3 ) % 11 - Int( i * 7 / 4 ) + s % 19 - i * 2
   NEXT
   FOR j := 1 TO 3
      u := u + t % 23 + j * 7 - Int( t / 9 ) + ( t * 5 ) % 13 - Int( j * 9 / 5 ) + t % 29 - j * 3
      v := v + u % 31 + j * 11 - Int( u / 13 ) + ( u * 7 ) % 17 - Int( j * 11 / 6 ) + u % 37 - j * 4
   NEXT
   FOR i := 1 TO 3
      w := w + v % 41 + i * 13 - Int( v / 15 ) + ( v * 9 ) % 19 - Int( i * 13 / 7 ) + v % 43 - i * 5
      s := s + w % 47 + i * 17 - Int( w / 17 ) + ( w * 11 ) % 23 - Int( i * 15 / 8 ) + w % 53 - i * 6
   NEXT
   FOR j := 1 TO 3
      t := t + s % 59 + j * 19 - Int( s / 19 ) + ( s * 13 ) % 29 - Int( j * 17 / 9 ) + s % 61 - j * 7
      u := u + t % 67 + j * 23 - Int( t / 21 ) + ( t * 15 ) % 31 - Int( j * 19 / 10 ) + t % 71 - j * 8
   NEXT
   FOR i := 1 TO 3
      v := v + u % 73 + i * 29 - Int( u / 23 ) + ( u * 17 ) % 37 - Int( i * 21 / 11 ) + u % 79 - i * 9
      w := w + v % 83 + i * 31 - Int( v / 25 ) + ( v * 19 ) % 41 - Int( i * 23 / 12 ) + v % 89 - i * 10
   NEXT
   FOR j := 1 TO 3
      s := s + w % 97 + j * 37 - Int( w / 27 ) + ( w * 21 ) % 43 - Int( j * 25 / 13 ) + w % 101 - j * 11
      t := t + s % 103 + j * 41 - Int( s / 29 ) + ( s * 23 ) % 47 - Int( j * 27 / 14 ) + s % 107 - j * 12
   NEXT
   FOR i := 1 TO 3
      u := u + t % 109 + i * 43 - Int( t / 31 ) + ( t * 25 ) % 53 - Int( i * 29 / 15 ) + t % 113 - i * 13
      v := v + u % 127 + i * 47 - Int( u / 33 ) + ( u * 27 ) % 59 - Int( i * 31 / 16 ) + u % 131 - i * 14
   NEXT
   FOR j := 1 TO 3
      w := w + v % 137 + j * 53 - Int( v / 35 ) + ( v * 29 ) % 61 - Int( j * 33 / 17 ) + v % 139 - j * 15
      s := s + w % 149 + j * 59 - Int( w / 37 ) + ( w * 31 ) % 67 - Int( j * 35 / 18 ) + w % 151 - j * 16
   NEXT
   FOR i := 1 TO 3
      t := t + s % 157 + i * 61 - Int( s / 39 ) + ( s * 33 ) % 71 - Int( i * 37 / 19 ) + s % 163 - i * 17
      u := u + t % 167 + i * 67 - Int( t / 41 ) + ( t * 35 ) % 73 - Int( i * 39 / 20 ) + t % 173 - i * 18
   NEXT
   FOR j := 1 TO 3
      v := v + u % 179 + j * 71 - Int( u / 43 ) + ( u * 37 ) % 79 - Int( j * 41 / 21 ) + u % 181 - j * 19
      w := w + v % 191 + j * 73 - Int( v / 45 ) + ( v * 39 ) % 83 - Int( j * 43 / 22 ) + v % 193 - j * 20
   NEXT
RETURN Shown( n ) + " " + Shown( m ) + " " + Shown( k ) + " " + Shown( s ) + " " + Shown( t ) + " " + Shown( u ) + ;
   " " + Shown( v ) + " " + Shown( w )
