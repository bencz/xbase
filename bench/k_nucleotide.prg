// Hashes keyed by strings: the Homo sapiens section of fasta for n = 100,000 (500,000 bases, upper case, made with the
// benchmark's LCG before the clock starts), every k-mer of it counted in a hash for k = 1, 2, 3, 4, 6, 12 and 18 (a
// SubStr per position as the key), the 1- and 2-mers sorted by count with ASort, and the counts of five given
// sequences.
#define IM 139968
#define IA 3877
#define IC 29573

PROCEDURE Main()
   LOCAL n := 100000, seed := 42, aCum := Array( 4 ), aProb, nAcc := 0, s := "", i, j, r, t, ok
   aProb := { 0.3029549426680, 0.1979883004921, 0.1975473066391, 0.3015094502008 }
   FOR i := 1 TO 4
      nAcc += aProb[ i ]
      aCum[ i ] := nAcc
   NEXT
   FOR i := 1 TO 3 * n
      seed := ( seed * IA + IC ) % IM
   NEXT
   FOR i := 1 TO 5 * n
      seed := ( seed * IA + IC ) % IM
      r := seed / IM
      j := 1
      DO WHILE j < 4 .AND. r >= aCum[ j ]
         j++
      ENDDO
      s += SubStr( "ACGT", j, 1 )
   NEXT
   t := ElapsedMs()
   ok := Sorted( s, 1 ) == "A 151655 T 150452 C 99156 G 98737" .AND. ;
      Sorted( s, 2 ) == "AA 45992 TA 45599 AT 45574 TT 45299 CA 30129 AC 30096 AG 29993 GA 29935 CT 29857 " + ;
      "TC 29832 GT 29722 TG 29721 GC 19617 CC 19611 CG 19559 GG 19463" .AND. ;
      CountOf( s, "GGT" ) == 5871 .AND. CountOf( s, "GGTA" ) == 1769 .AND. CountOf( s, "GGTATT" ) == 185 .AND. ;
      CountOf( s, "GGTATTTTAATT" ) == 4 .AND. CountOf( s, "GGTATTTTAATTTATAGT" ) == 4
   ? "k_nucleotide", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

// Every k-mer of s with its count.
STATIC FUNCTION Frames( s, k )
   LOCAL h := { => }, i, cKey
   FOR i := 1 TO Len( s ) - k + 1
      cKey := SubStr( s, i, k )
      IF cKey $ h
         h[ cKey ]++
      ELSE
         h[ cKey ] := 1
      ENDIF
   NEXT
   RETURN h

STATIC FUNCTION CountOf( s, cSeq )
   LOCAL h := Frames( s, Len( cSeq ) )
   RETURN IIf( cSeq $ h, h[ cSeq ], 0 )

// The k-mers by count, then by name: "A 151655 T 150452 ...".
STATIC FUNCTION Sorted( s, k )
   LOCAL h := Frames( s, k ), aPairs := {}, cOut := "", cKey, aPair
   FOR EACH cKey IN HKeys( h )
      AAdd( aPairs, { cKey, h[ cKey ] } )
   NEXT
   ASort( aPairs,,, {| x, y | x[ 2 ] > y[ 2 ] .OR. ( x[ 2 ] == y[ 2 ] .AND. x[ 1 ] < y[ 1 ] ) } )
   FOR EACH aPair IN aPairs
      cOut += IIf( Empty( cOut ), "", " " ) + aPair[ 1 ] + " " + LTrim( Str( aPair[ 2 ] ) )
   NEXT
   RETURN cOut
