// Arrays of arrays: Array() with several dimensions, a[i][j] and a[i, j], compound assignment through rows,
// matrix product, transpose, powers, Gaussian elimination and determinant, jagged rows that grow and shrink,
// sorting and scanning rows, deep copies with shared and cyclic rows, and a Game of Life on a grid.
PROCEDURE Main()
   LOCAL a, b, c, m, i, j, k, n, s, row, g, t

   a := Array( 2, 3 )
   ? Len( a ), Len( a[ 1 ] ), Len( a[ 2 ] ), ValType( a[ 2, 3 ] ), a[ 1 ] == a[ 2 ]
   b := Array( 2, 3, 4 )
   n := 0
   FOR i := 1 TO 2
      FOR j := 1 TO 3
         FOR k := 1 TO 4
            b[ i, j, k ] := i * 100 + j * 10 + k
            n += b[ i ][ j ][ k ]
         NEXT
      NEXT
   NEXT
   ? n, b[ 2, 3, 4 ], b[ 1 ][ 2 ][ 3 ], Len( b[ 2, 1 ] )
   b[ 2, 3, 4 ] += 1000
   b[ 1 ][ 1 ][ 1 ] *= 7
   b[ 2 ][ 2, 2 ]++
   --b[ 1, 3 ][ 4 ]
   ? b[ 2, 3, 4 ], b[ 1, 1, 1 ], b[ 2, 2, 2 ], b[ 1, 3, 4 ]

   // Product, identity, transpose: (AB)' = B'A'.
   a := { { 1, 2, 3 }, { 4, 5, 6 } }
   b := { { 7, 8 }, { 9, 10 }, { 11, 12 } }
   c := MatMul( a, b )
   Show( "AB", c )
   Show( "BA", MatMul( b, a ) )
   Show( "A'", Transpose( a ) )
   ? "(AB)' = B'A'", Same( Transpose( c ), MatMul( Transpose( b ), Transpose( a ) ) )
   ? "AI = A", Same( MatMul( a, Identity( 3 ) ), a ), Same( MatMul( Identity( 2 ), a ), a )

   // Powers: Fibonacci numbers from { { 1, 1 }, { 1, 0 } } ^ n by squaring.
   m := MatPow( { { 1, 1 }, { 1, 0 } }, 80 )
   ? "fib", m[ 1, 2 ], m[ 1, 1 ], m[ 2, 2 ]

   // Gaussian elimination with partial pivoting: the solution and the determinant.
   a := { { 2, 1, -1, 3 }, { -3, -1, 2, -11 }, { -2, 1, 2, -3 }, { 1, 2, 3, 4 } }
   b := { 1, -8, 3, 20 }
   s := Solve( a, b )
   ?? "x"
   FOR EACH t IN s
      ?? " " + LTrim( Str( t, 12, 6 ) )
   NEXT
   ? "det", Str( Det( a ), 12, 4 ), Str( Det( { { 1, 2 }, { 2, 4 } } ), 8, 4 ), Det( Identity( 5 ) )
   ? "A unchanged", a[ 1, 1 ], a[ 4, 4 ], b[ 2 ]
   t := MatMul( a, Column( s ) )
   ?? "Ax"
   FOR i := 1 TO Len( t )
      ?? " " + LTrim( Str( t[ i, 1 ], 12, 6 ) )
   NEXT

   // A larger product with a checksum.
   n := 60
   a := Array( n, n )
   b := Array( n, n )
   FOR i := 1 TO n
      FOR j := 1 TO n
         a[ i, j ] := ( i * 7 + j * 3 ) % 11 - 5
         b[ i, j ] := ( i * 5 + j * 13 ) % 17 - 8
      NEXT
   NEXT
   c := MatMul( a, b )
   s := 0
   FOR EACH i => row IN c
      FOR EACH j => t IN row
         s += t * i - j
      NEXT
   NEXT
   ? "60x60", s, c[ 1, 1 ], c[ n, n ], c[ 17, 43 ]

   // Jagged rows: Pascal's triangle, rows added, resized, inserted and deleted.
   m := {}
   FOR i := 1 TO 12
      row := Array( i )
      row[ 1 ] := 1
      row[ i ] := 1
      FOR j := 2 TO i - 1
         row[ j ] := m[ i - 1, j - 1 ] + m[ i - 1, j ]
      NEXT
      AAdd( m, row )
   NEXT
   ? "pascal", Len( m ), Len( m[ 12 ] ), m[ 12, 6 ], m[ 7, 4 ]
   ASize( m[ 3 ], 6 )
   AFill( m[ 3 ], 0, 4 )
   AIns( m, 2 )
   m[ 2 ] := { "novo" }
   ADel( m, 1 )
   ASize( m, Len( m ) - 1 )
   ? Len( m ), m[ 1, 1 ], Len( m[ 3 ] ), m[ 3, 6 ], m[ 3, 3 ], Len( m[ Len( m ) ] )
   AAdd( m[ 1 ], { 1, { 2, { 3 } } } )
   m[ 1, 2, 2, 2, 1 ] += 40
   ? m[ 1, 2, 2, 2, 1 ], Len( m[ 1 ] )

   // Rows sorted by a column, scanned and evaluated.
   m := { { "c", 3, 30 }, { "a", 1, 10 }, { "d", 4, 5 }, { "b", 2, 20 }, { "a", 0, 99 } }
   ASort( m,,, {| x, y | x[ 1 ] < y[ 1 ] .OR. ( x[ 1 ] == y[ 1 ] .AND. x[ 2 ] < y[ 2 ] ) } )
   AEval( m, {| r | QQOut( " " + r[ 1 ] + LTrim( Str( r[ 2 ] ) ) ) } )
   ASort( m,,, {| x, y | x[ 3 ] > y[ 3 ] } )
   ? m[ 1, 3 ], m[ 5, 3 ], AScan( m, {| r | r[ 1 ] == "b" } ), AScan( m, {| r | r[ 3 ] < 0 } )
   s := 0
   AEval( m, {| r, p | s += r[ 3 ] * p }, 2, 3 )
   ? s

   // Deep copies: rows reached twice stay one row in the copy, a cycle stays a cycle.
   row := { 1, 2 }
   a := { row, row, { row, { "x" } } }
   AAdd( a, a )
   b := AClone( a )
   b[ 1, 1 ] := 100
   b[ 3, 2, 1 ] := "y"
   ? a[ 1, 1 ], a[ 3, 2, 1 ], b[ 2, 1 ], b[ 3, 1, 1 ], b[ 3, 2, 1 ]
   ? b[ 1 ] == b[ 2 ], b[ 3, 1 ] == b[ 1 ], b[ 4 ] == b, b[ 4 ] == a, b[ 1 ] == row, b[ 4, 4, 4, 1, 1 ]

   // Game of Life on a 10x10 torus: a glider moves one cell diagonally every four generations.
   g := Array( 10, 10 )
   AEval( g, {| r | AFill( r, .F. ) } )
   g[ 1, 2 ] := g[ 2, 3 ] := g[ 3, 1 ] := g[ 3, 2 ] := g[ 3, 3 ] := .T.
   FOR i := 1 TO 8
      g := Life( g )
   NEXT
   FOR EACH row IN g
      ?
      FOR EACH t IN row
         ?? iif( t, "#", "." )
      NEXT
   NEXT
RETURN

STATIC FUNCTION MatMul( a, b )
   LOCAL c := Array( Len( a ), Len( b[ 1 ] ) ), i, j, k, s
   FOR i := 1 TO Len( a )
      FOR j := 1 TO Len( b[ 1 ] )
         s := 0
         FOR k := 1 TO Len( b )
            s += a[ i, k ] * b[ k, j ]
         NEXT
         c[ i, j ] := s
      NEXT
   NEXT
RETURN c

STATIC FUNCTION Transpose( a )
   LOCAL t := Array( Len( a[ 1 ] ), Len( a ) ), i, j
   FOR i := 1 TO Len( a )
      FOR j := 1 TO Len( a[ 1 ] )
         t[ j ][ i ] := a[ i ][ j ]
      NEXT
   NEXT
RETURN t

STATIC FUNCTION Identity( n )
   LOCAL m := Array( n, n ), i
   AEval( m, {| r | AFill( r, 0 ) } )
   FOR i := 1 TO n
      m[ i, i ] := 1
   NEXT
RETURN m

STATIC FUNCTION Column( v )
   LOCAL c := {}, x
   FOR EACH x IN v
      AAdd( c, { x } )
   NEXT
RETURN c

STATIC FUNCTION MatPow( m, n )
   LOCAL r := Identity( Len( m ) )
   DO WHILE n > 0
      IF n % 2 == 1
         r := MatMul( r, m )
      ENDIF
      m := MatMul( m, m )
      n := Int( n / 2 )
   ENDDO
RETURN r

STATIC FUNCTION Same( a, b )
   LOCAL i, j
   IF Len( a ) != Len( b )
      RETURN .F.
   ENDIF
   FOR i := 1 TO Len( a )
      IF Len( a[ i ] ) != Len( b[ i ] )
         RETURN .F.
      ENDIF
      FOR j := 1 TO Len( a[ i ] )
         IF a[ i, j ] != b[ i, j ]
            RETURN .F.
         ENDIF
      NEXT
   NEXT
RETURN .T.

// Forward elimination on an augmented copy, with the row of the largest pivot swapped up.
STATIC FUNCTION Eliminate( a, b, nSwaps )
   LOCAL n := Len( a ), m := AClone( a ), i, j, k, p, f, tmp
   FOR i := 1 TO n
      AAdd( m[ i ], iif( b == NIL, 0, b[ i ] ) )
   NEXT
   nSwaps := 0
   FOR k := 1 TO n
      p := k
      FOR i := k + 1 TO n
         IF Abs( m[ i, k ] ) > Abs( m[ p, k ] )
            p := i
         ENDIF
      NEXT
      IF m[ p, k ] == 0
         RETURN NIL
      ENDIF
      IF p != k
         tmp := m[ p ]
         m[ p ] := m[ k ]
         m[ k ] := tmp
         nSwaps++
      ENDIF
      FOR i := k + 1 TO n
         f := m[ i, k ] / m[ k, k ]
         FOR j := k TO n + 1
            m[ i, j ] -= f * m[ k, j ]
         NEXT
      NEXT
   NEXT
RETURN m

STATIC FUNCTION Solve( a, b )
   LOCAL n := Len( a ), m := Eliminate( a, b ), x := Array( n ), i, j, s
   FOR i := n TO 1 STEP -1
      s := m[ i, n + 1 ]
      FOR j := i + 1 TO n
         s -= m[ i, j ] * x[ j ]
      NEXT
      x[ i ] := s / m[ i, i ]
   NEXT
RETURN x

STATIC FUNCTION Det( a )
   LOCAL nSwaps := 0, m := Eliminate( a, NIL, @nSwaps ), d := 1, i
   IF m == NIL
      RETURN 0
   ENDIF
   FOR i := 1 TO Len( a )
      d *= m[ i, i ]
   NEXT
RETURN iif( nSwaps % 2 == 1, -d, d )

STATIC FUNCTION Life( g )
   LOCAL n := Len( g ), nxt := Array( n, n ), i, j, di, dj, c
   FOR i := 1 TO n
      FOR j := 1 TO n
         c := 0
         FOR di := -1 TO 1
            FOR dj := -1 TO 1
               IF ( di != 0 .OR. dj != 0 ) .AND. g[ ( i + di + n - 1 ) % n + 1, ( j + dj + n - 1 ) % n + 1 ]
                  c++
               ENDIF
            NEXT
         NEXT
         nxt[ i, j ] := c == 3 .OR. ( c == 2 .AND. g[ i, j ] )
      NEXT
   NEXT
RETURN nxt

STATIC PROCEDURE Show( cName, m )
   LOCAL row, x
   ? cName
   FOR EACH row IN m
      ?? " |"
      FOR EACH x IN row
         ?? " " + LTrim( Str( x ) )
      NEXT
   NEXT
RETURN
