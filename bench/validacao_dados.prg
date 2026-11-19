// Data validation: 250,000 records { CPF, e-mail, date text, amount text } made from a fixed seed, with errors mixed
// in (a wrong check digit, an e-mail with no "@", no dot or a bad character, impossible dates, a letter among the
// digits). Timed: each field validated by its rule (the CPF's two mod-11 check digits, the e-mail's parts and
// characters, the date through CToD with SET DATE BRITISH, the amount's digits and range) and the valid ones counted.
STATIC nSeed := 8086

PROCEDURE Main()
   LOCAL aRec := Array( 250000 ), t, i, r, lC, lE, lD, lA, nC := 0, nE := 0, nD := 0, nA := 0, nAll := 0, nSum := 0
   LOCAL ok
   SET DATE BRITISH
   SET CENTURY ON
   FOR i := 1 TO Len( aRec )
      aRec[ i ] := MakeRecord()
   NEXT
   t := ElapsedMs()
   FOR EACH r IN aRec
      lC := CpfOk( r[ 1 ] )
      lE := EmailOk( r[ 2 ] )
      lD := ! Empty( CToD( r[ 3 ] ) )
      lA := AmountOk( r[ 4 ] )
      IF lA
         nSum += Val( r[ 4 ] )
      ENDIF
      nC += IIf( lC, 1, 0 )
      nE += IIf( lE, 1, 0 )
      nD += IIf( lD, 1, 0 )
      nA += IIf( lA, 1, 0 )
      nAll += IIf( lC .AND. lE .AND. lD .AND. lA, 1, 0 )
   NEXT
   ok := nC == 187295 .AND. nE == 212602 .AND. nD == 245457 .AND. nA == 192603 .AND. nAll == 120445 .AND. ;
         nSum == 19490512431
   ? "validacao_dados", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
RETURN

STATIC FUNCTION CpfOk( c )
   LOCAL d := StrTran( StrTran( c, "." ), "-" ), i, s := 0
   IF Len( d ) != 11
      RETURN .F.
   ENDIF
   FOR i := 1 TO 11
      IF ! IsDigit( SubStr( d, i, 1 ) )
         RETURN .F.
      ENDIF
   NEXT
   FOR i := 1 TO 9
      s += ( Asc( SubStr( d, i, 1 ) ) - 48 ) * ( 11 - i )
   NEXT
   IF s * 10 % 11 % 10 != Asc( SubStr( d, 10, 1 ) ) - 48
      RETURN .F.
   ENDIF
   s := 0
   FOR i := 1 TO 10
      s += ( Asc( SubStr( d, i, 1 ) ) - 48 ) * ( 12 - i )
   NEXT
RETURN s * 10 % 11 % 10 == Asc( SubStr( d, 11, 1 ) ) - 48

STATIC FUNCTION EmailOk( e )
   LOCAL nAt := At( "@", e ), cDom, i
   IF nAt < 2 .OR. "@" $ SubStr( e, nAt + 1 )
      RETURN .F.
   ENDIF
   FOR i := 1 TO nAt - 1
      IF ! SubStr( e, i, 1 ) $ "abcdefghijklmnopqrstuvwxyz0123456789._"
         RETURN .F.
      ENDIF
   NEXT
   cDom := SubStr( e, nAt + 1 )
   IF Len( cDom ) < 3 .OR. ! "." $ cDom .OR. Left( cDom, 1 ) == "." .OR. Right( cDom, 1 ) == "."
      RETURN .F.
   ENDIF
   FOR i := 1 TO Len( cDom )
      IF ! SubStr( cDom, i, 1 ) $ "abcdefghijklmnopqrstuvwxyz0123456789.-"
         RETURN .F.
      ENDIF
   NEXT
RETURN .T.

STATIC FUNCTION AmountOk( c )
   LOCAL i, n
   FOR i := 1 TO Len( c )
      IF ! IsDigit( SubStr( c, i, 1 ) )
         RETURN .F.
      ENDIF
   NEXT
   n := Val( c )
RETURN n >= 1 .AND. n <= 999999

// A record; its errors come from the seed as the checks' constants were computed.
STATIC FUNCTION MakeRecord()
   LOCAL d := Array( 11 ), k, s := 0, cCpf, cMail := "", nKind, cDom, nLen, cAmt := ""
   FOR k := 1 TO 9
      d[ k ] := Rand( 10 )
   NEXT
   FOR k := 1 TO 9
      s += d[ k ] * ( 11 - k )
   NEXT
   d[ 10 ] := s * 10 % 11 % 10
   s := 0
   FOR k := 1 TO 10
      s += d[ k ] * ( 12 - k )
   NEXT
   d[ 11 ] := s * 10 % 11 % 10
   IF Rand( 4 ) == 0
      d[ 11 ] := ( d[ 11 ] + 1 ) % 10
   ENDIF
   cCpf := ""
   FOR k := 1 TO 11
      cCpf += Str( d[ k ], 1 ) + IIf( k == 3 .OR. k == 6, ".", IIf( k == 9, "-", "" ) )
   NEXT
   nLen := Rand( 8 ) + 3
   FOR k := 1 TO nLen
      cMail += SubStr( "abcdefghijklmnopqrstuvwxyz0123456789", Rand( 36 ) + 1, 1 )
   NEXT
   nKind := Rand( 20 )
   cDom := { "mail.com", "example.com.br", "x.org" }[ Rand( 3 ) + 1 ]
   DO CASE
   CASE nKind == 0
      cMail += cDom
   CASE nKind == 1
      cMail += "@examplecom"
   CASE nKind == 2
      cMail += "!@" + cDom
   OTHERWISE
      cMail += "@" + cDom
   ENDCASE
   s := StrZero( Rand( 31 ) + 1, 2 ) + "/" + StrZero( Rand( 12 ) + 1, 2 ) + "/" + Str( 1990 + Rand( 41 ), 4 )
   nLen := Rand( 7 ) + 1
   FOR k := 1 TO nLen
      cAmt += Str( Rand( 10 ), 1 )
   NEXT
   IF Rand( 10 ) == 0
      cAmt := Stuff( cAmt, Rand( nLen ) + 1, 1, "x" )
   ENDIF
RETURN { cCpf, cMail, s, cAmt }

STATIC FUNCTION Rand( n )
   nSeed := nSeed * 16807 % 2147483647
RETURN nSeed % n
