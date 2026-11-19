// The fasta benchmark for n = 1,500,000: the ALU sequence repeated over 2n characters, then 3n IUB codes and
// 5n Homo sapiens bases picked from cumulative probabilities with the benchmark's LCG (seed 42), in lines of 60,
// appended to one string of 15 MB. Checked: its length, the generator's last state, the sum of the indexes picked,
// the end of the first and of the last section.
#define IM 139968
#define IA 3877
#define IC 29573
#define LINE 60

PROCEDURE Main()
   LOCAL t := ElapsedMs(), n := 1500000, aState := { 42, 0 }, cOut := "", nTwo, ok
   cOut += ">ONE Homo sapiens alu" + Chr( 10 )
   cOut += RepeatSeq( 2 * n )
   cOut += ">TWO IUB ambiguity codes" + Chr( 10 )
   cOut += RandomSeq( aState, "acgtBDHKMNRSVWY", ;
      { 0.27, 0.12, 0.12, 0.27, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02 }, 3 * n )
   cOut += ">THREE Homo sapiens frequency" + Chr( 10 )
   cOut += RandomSeq( aState, "acgt", { 0.3029549426680, 0.1979883004921, 0.1975473066391, 0.3015094502008 }, 5 * n )
   nTwo := At( ">TWO", cOut )
   ok := Len( cOut ) == 15250077 .AND. aState[ 1 ] == 28650 .AND. aState[ 2 ] == 25406718 .AND. ;
      nTwo == 3050023 .AND. ;
      SubStr( cOut, nTwo - 21, 21 ) == "TGGGCGACAGAGCGAGACTC" + Chr( 10 ) .AND. ;
      Right( cOut, 21 ) == "acacatatagttttatacaa" + Chr( 10 )
   ? "fasta", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

// n characters of the ALU sequence, cyclically, in lines of 60.
STATIC FUNCTION RepeatSeq( n )
   LOCAL cAlu := "GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTC" + ;
      "AGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCG" + ;
      "TGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGG" + ;
      "AGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAA"
   LOCAL k := Len( cAlu ), nPos := 1, cOut := "", nDone, m, i
   FOR nDone := 0 TO n - 1 STEP LINE
      m := Min( n - nDone, LINE )
      FOR i := 1 TO m
         cOut += SubStr( cAlu, nPos, 1 )
         IF ++nPos > k
            nPos := 1
         ENDIF
      NEXT
      cOut += Chr( 10 )
   NEXT
   RETURN cOut

// n symbols drawn with the LCG in aState[ 1 ] (the sum of their indexes, from 0, added to aState[ 2 ]), in lines of 60.
STATIC FUNCTION RandomSeq( aState, cSym, aProb, n )
   LOCAL nSym := Len( aProb ), aCum := Array( nSym ), nAcc := 0, seed := aState[ 1 ], nPicks := 0, cOut := ""
   LOCAL nDone, m, i, j, r
   FOR i := 1 TO nSym
      nAcc += aProb[ i ]
      aCum[ i ] := nAcc
   NEXT
   FOR nDone := 0 TO n - 1 STEP LINE
      m := Min( n - nDone, LINE )
      FOR i := 1 TO m
         seed := ( seed * IA + IC ) % IM
         r := seed / IM
         j := 1
         DO WHILE j < nSym .AND. r >= aCum[ j ]
            j++
         ENDDO
         nPicks += j - 1
         cOut += SubStr( cSym, j, 1 )
      NEXT
      cOut += Chr( 10 )
   NEXT
   aState[ 1 ] := seed
   aState[ 2 ] += nPicks
   RETURN cOut
