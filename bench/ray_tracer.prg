// Small arrays of doubles made and dropped, function calls: a 320 x 240 image of five spheres ray traced (diffuse
// light, shadows, reflections three deep), vectors as arrays of three doubles, checked by the sum of the pixels' gray
// levels, 2 times.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), nWidth := 320, nHeight := 240, aSpheres, aEye, aLight, x, y, nTotal, rep, ok := .T.
   // Each sphere: its center, its radius, how much it reflects and its albedo.
   aSpheres := { { { 0.0, -1000.0, 0.0 }, 999.0, 0.2, 0.6 }, { { 0.0, 0.0, 5.0 }, 1.0, 0.5, 0.9 }, ;
      { { -2.0, 0.0, 6.0 }, 1.0, 0.3, 0.7 }, { { 2.0, 0.0, 6.0 }, 1.0, 0.3, 0.8 }, ;
      { { 0.0, 2.2, 7.0 }, 1.0, 0.6, 0.5 } }
   aLight := { 5.0, 10.0, -5.0 }
   aEye := { 0.0, 0.5, -1.0 }
   FOR rep := 1 TO 2
      nTotal := 0
      FOR y := 0 TO nHeight - 1
         FOR x := 0 TO nWidth - 1
            nTotal += Int( Min( Trace( aSpheres, aLight, aEye, VNorm( { ( x - nWidth / 2.0 + 0.5 ) / nHeight, ;
               ( nHeight / 2.0 - y - 0.5 ) / nHeight, 1.0 } ), 0 ), 1.0 ) * 255.0 )
         NEXT
      NEXT
      ok := ok .AND. nTotal == 7100994
   NEXT
   ? "ray_tracer", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION VAdd( a, b )
   RETURN { a[ 1 ] + b[ 1 ], a[ 2 ] + b[ 2 ], a[ 3 ] + b[ 3 ] }

STATIC FUNCTION VSub( a, b )
   RETURN { a[ 1 ] - b[ 1 ], a[ 2 ] - b[ 2 ], a[ 3 ] - b[ 3 ] }

STATIC FUNCTION VScale( a, s )
   RETURN { a[ 1 ] * s, a[ 2 ] * s, a[ 3 ] * s }

STATIC FUNCTION VDot( a, b )
   RETURN a[ 1 ] * b[ 1 ] + a[ 2 ] * b[ 2 ] + a[ 3 ] * b[ 3 ]

STATIC FUNCTION VNorm( a )
   RETURN VScale( a, 1.0 / Sqrt( VDot( a, a ) ) )

// The distance along aDir from aOrigin to sphere aS, or 0 when the ray misses it.
STATIC FUNCTION Intersect( aS, aOrigin, aDir )
   LOCAL aOC := VSub( aOrigin, aS[ 1 ] ), b := VDot( aOC, aDir ), nDisc, q, t
   nDisc := b * b - ( VDot( aOC, aOC ) - aS[ 2 ] * aS[ 2 ] )
   IF nDisc > 0.0
      q := Sqrt( nDisc )
      t := -b - q
      IF t < 0.0001
         t := -b + q
      ENDIF
      IF t > 0.0001
         RETURN t
      ENDIF
   ENDIF
   RETURN 0.0

// The gray level seen along aDir from aOrigin.
STATIC FUNCTION Trace( aSpheres, aLight, aOrigin, aDir, nDepth )
   LOCAL nBest := 0.0, aHit := NIL, aS, t, p, n, aToLight, nDist, l, nDiff, nCol
   FOR EACH aS IN aSpheres
      t := Intersect( aS, aOrigin, aDir )
      IF t > 0.0 .AND. ( aHit == NIL .OR. t < nBest )
         nBest := t
         aHit := aS
      ENDIF
   NEXT
   IF aHit == NIL
      RETURN 0.2 + 0.5 * Max( 0.0, aDir[ 2 ] )
   ENDIF
   p := VAdd( aOrigin, VScale( aDir, nBest ) )
   n := VScale( VSub( p, aHit[ 1 ] ), 1.0 / aHit[ 2 ] )
   aToLight := VSub( aLight, p )
   nDist := Sqrt( VDot( aToLight, aToLight ) )
   l := VScale( aToLight, 1.0 / nDist )
   nDiff := VDot( n, l )
   FOR EACH aS IN aSpheres
      IF nDiff <= 0.0
         EXIT
      ENDIF
      t := Intersect( aS, p, l )
      IF t > 0.0 .AND. t < nDist
         nDiff := 0.0
      ENDIF
   NEXT
   nCol := aHit[ 4 ] * ( 0.1 + 0.9 * Max( nDiff, 0.0 ) )
   IF nDepth < 3 .AND. aHit[ 3 ] > 0.0
      nCol := nCol * ( 1.0 - aHit[ 3 ] ) + aHit[ 3 ] * Trace( aSpheres, aLight, p, ;
         VSub( aDir, VScale( n, 2.0 * VDot( aDir, n ) ) ), nDepth + 1 )
   ENDIF
   RETURN nCol
