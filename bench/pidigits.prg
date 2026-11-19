// Arbitrary precision in arrays: the first 1,500 digits of pi from Gibbons' unbounded spigot, its numbers kept as
// arrays of 30-bit limbs (the least significant first; the accumulator's sign apart), with multiplication by a small
// number, addition and subtraction of a multiple, comparison, and a quotient below 2^30 estimated from the leading
// limbs with doubles, then corrected. Checked: the first and the last 50 digits (as bc computes them) and their sum.
#define BITS 30
#define LIMB_MASK 0x3FFFFFFF
#define RADIX 1073741824.0

PROCEDURE Main()
   LOCAL t := ElapsedMs(), n := 1500, cDigits := "", lNeg := .F., lXNeg, k := 0, nSum := 0, d, i, ok
   LOCAL aNumer := { 1 }, aAccum := { 0 }, aDenom := { 1 }, aX
   DO WHILE Len( cDigits ) < n
      k++
      AddSigned( aAccum, @lNeg, aNumer, 2, .F. )
      MulSmall( aAccum, 2 * k + 1 )
      MulSmall( aDenom, 2 * k + 1 )
      MulSmall( aNumer, k )
      IF lNeg .OR. Cmp( aNumer, aAccum ) > 0
         LOOP
      ENDIF
      aX := AClone( aAccum )
      lXNeg := lNeg
      AddSigned( aX, @lXNeg, aNumer, 3, .F. )
      d := Quotient( aX, aDenom )
      AddSigned( aX, @lXNeg, aNumer, 1, .F. )
      IF d != Quotient( aX, aDenom )
         LOOP
      ENDIF
      cDigits += Chr( 48 + d )
      AddSigned( aAccum, @lNeg, aDenom, d, .T. )
      MulSmall( aAccum, 10 )
      MulSmall( aNumer, 10 )
   ENDDO
   FOR i := 1 TO n
      nSum += Asc( SubStr( cDigits, i, 1 ) ) - 48
   NEXT
   ok := Left( cDigits, 50 ) == "31415926535897932384626433832795028841971693993751" .AND. ;
      Right( cDigits, 50 ) == "55570674983850549458858692699569092721079750930295" .AND. nSum == 6775
   ? "pidigits", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

// Drops the leading zero limbs (keeps one).
STATIC PROCEDURE Normalize( a )
   LOCAL n := Len( a )
   DO WHILE n > 1 .AND. a[ n ] == 0
      n--
   ENDDO
   IF n < Len( a )
      ASize( a, n )
   ENDIF
   RETURN

// a *= m
STATIC PROCEDURE MulSmall( a, m )
   LOCAL nCarry := 0, i, x
   FOR i := 1 TO Len( a )
      x := a[ i ] * m + nCarry
      a[ i ] := x & LIMB_MASK
      nCarry := x >> BITS
   NEXT
   DO WHILE nCarry > 0
      AAdd( a, nCarry & LIMB_MASK )
      nCarry := nCarry >> BITS
   ENDDO
   Normalize( a )
   RETURN

// a += b * m
STATIC PROCEDURE AddMul( a, b, m )
   LOCAL nCarry := 0, nB := Len( b ), i, x
   IF Len( a ) < nB
      i := Len( a )
      ASize( a, nB )
      AFill( a, 0, i + 1 )
   ENDIF
   FOR i := 1 TO nB
      x := a[ i ] + b[ i ] * m + nCarry
      a[ i ] := x & LIMB_MASK
      nCarry := x >> BITS
   NEXT
   DO WHILE nCarry > 0 .AND. i <= Len( a )
      x := a[ i ] + nCarry
      a[ i ] := x & LIMB_MASK
      nCarry := x >> BITS
      i++
   ENDDO
   DO WHILE nCarry > 0
      AAdd( a, nCarry & LIMB_MASK )
      nCarry := nCarry >> BITS
   ENDDO
   RETURN

// a -= b * m, when b * m <= a
STATIC PROCEDURE SubMul( a, b, m )
   LOCAL nBorrow := 0, i, x
   FOR i := 1 TO Len( b )
      x := a[ i ] - b[ i ] * m - nBorrow
      a[ i ] := x & LIMB_MASK
      nBorrow := -( x >> BITS )
   NEXT
   DO WHILE nBorrow > 0
      x := a[ i ] - nBorrow
      a[ i ] := x & LIMB_MASK
      nBorrow := -( x >> BITS )
      i++
   ENDDO
   Normalize( a )
   RETURN

// a += b * m (m >= 0), lNeg the sign of a, lTermNeg that of the term.
STATIC PROCEDURE AddSigned( a, lNeg, b, m, lTermNeg )
   LOCAL aTmp
   IF lNeg == lTermNeg
      AddMul( a, b, m )
   ELSE
      aTmp := AClone( b )
      MulSmall( aTmp, m )
      IF Cmp( a, aTmp ) >= 0
         SubMul( a, aTmp, 1 )
      ELSE
         SubMul( aTmp, a, 1 )
         ASize( a, Len( aTmp ) )
         ACopy( aTmp, a )
         lNeg := lTermNeg
      ENDIF
   ENDIF
   IF Len( a ) == 1 .AND. a[ 1 ] == 0
      lNeg := .F.
   ENDIF
   RETURN

STATIC FUNCTION Cmp( a, b )
   LOCAL i
   IF Len( a ) != Len( b )
      RETURN IIf( Len( a ) < Len( b ), -1, 1 )
   ENDIF
   FOR i := Len( a ) TO 1 STEP -1
      IF a[ i ] != b[ i ]
         RETURN IIf( a[ i ] < b[ i ], -1, 1 )
      ENDIF
   NEXT
   RETURN 0

// The limb at i as a double, 0 past either end.
STATIC FUNCTION Top( a, i )
   RETURN IIf( i >= 1 .AND. i <= Len( a ), a[ i ] * 1.0, 0.0 )

// Int( x / d ) for a quotient below 2^30: estimated from the three leading limbs, then corrected.
STATIC FUNCTION Quotient( x, d )
   LOCAL h := Max( Len( x ), Len( d ) ), fx, fd, q, aTmp
   fx := ( Top( x, h ) * RADIX + Top( x, h - 1 ) ) * RADIX + Top( x, h - 2 )
   fd := ( Top( d, h ) * RADIX + Top( d, h - 1 ) ) * RADIX + Top( d, h - 2 )
   q := Int( fx / fd )
   aTmp := AClone( d )
   MulSmall( aTmp, q )
   DO WHILE Cmp( aTmp, x ) > 0
      SubMul( aTmp, d, 1 )
      q--
   ENDDO
   AddMul( aTmp, d, 1 )
   DO WHILE Cmp( aTmp, x ) <= 0
      AddMul( aTmp, d, 1 )
      q++
   ENDDO
   RETURN q
