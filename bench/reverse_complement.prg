// Strings read and built a character at a time: the output of fasta for n = 2,500,000 (25 MB, made with the
// benchmark's LCG before the clock starts) read section by section, each sequence joined, reverse-complemented
// through the IUB table (an array indexed by Asc()) and written again in lines of 60 after its header.
#define IM 139968
#define IA 3877
#define IC 29573
#define LINE 60

PROCEDURE Main()
   LOCAL cIn := Fasta( 2500000 ), t := ElapsedMs(), aComp := Array( 256 ), cFrom, cTo, cOut := "", cSeq := ""
   LOCAL i, c, nTwo, nThree, ok
   cFrom := "ACBDGHKMNSRUTWVYacbdghkmnsrutwvy"
   cTo := "TGVHCDMKNSYAABWRTGVHCDMKNSYAABWR"
   FOR i := 1 TO 256
      aComp[ i ] := Chr( i - 1 )
   NEXT
   FOR i := 1 TO Len( cFrom )
      aComp[ Asc( SubStr( cFrom, i, 1 ) ) + 1 ] := SubStr( cTo, i, 1 )
   NEXT
   i := 1
   DO WHILE i <= Len( cIn )
      c := SubStr( cIn, i, 1 )
      IF c == ">"
         IF ! Empty( cSeq )
            cOut += RevComp( cSeq, aComp )
            cSeq := ""
         ENDIF
         DO WHILE ( c := SubStr( cIn, i++, 1 ) ) != Chr( 10 )
            cOut += c
         ENDDO
         cOut += c
      ELSE
         IF c != Chr( 10 )
            cSeq += c
         ENDIF
         i++
      ENDIF
   ENDDO
   cOut += RevComp( cSeq, aComp )
   nTwo := At( ">TWO", cOut )
   nThree := At( ">THREE", cOut )
   ok := Len( cOut ) == 25416745 .AND. ;
      SubStr( cOut, 23, 60 ) == "GTAGCTGGGATTACAGGCGCGCGCCACCACGCCCGGCTAATTTTTGTATTTTTAGTAGAG" .AND. ;
      SubStr( cOut, nTwo + 25, 60 ) == "TTTATTYSTTGTATCCCTMWCGAACTAVGYHTATANTAWCARAWTTTVCTSTKDNTATHA" .AND. ;
      SubStr( cOut, nThree + 30, 60 ) == "GTAATTTCTCTCGGCTATTAGATTTGAACATATACACAGGCCAGATGATCTGGAAAAATC" .AND. ;
      Right( cOut, 21 ) == "CTATAACTAATATATTCGTC" + Chr( 10 )
   ? "reverse_complement", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

// The sequence reverse-complemented, in lines of 60.
STATIC FUNCTION RevComp( cSeq, aComp )
   LOCAL cOut := "", nCol := 0, i
   FOR i := Len( cSeq ) TO 1 STEP -1
      cOut += aComp[ Asc( SubStr( cSeq, i, 1 ) ) + 1 ]
      IF ++nCol == LINE
         cOut += Chr( 10 )
         nCol := 0
      ENDIF
   NEXT
   IF nCol > 0
      cOut += Chr( 10 )
   ENDIF
   RETURN cOut

// The three sections of fasta for n: the ALU sequence over 2n characters, 3n IUB codes, 5n Homo sapiens bases.
STATIC FUNCTION Fasta( n )
   LOCAL aSeed := { 42 }
   RETURN ">ONE Homo sapiens alu" + Chr( 10 ) + RepeatSeq( 2 * n ) + ;
      ">TWO IUB ambiguity codes" + Chr( 10 ) + RandomSeq( aSeed, "acgtBDHKMNRSVWY", ;
      { 0.27, 0.12, 0.12, 0.27, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02 }, 3 * n ) + ;
      ">THREE Homo sapiens frequency" + Chr( 10 ) + RandomSeq( aSeed, "acgt", ;
      { 0.3029549426680, 0.1979883004921, 0.1975473066391, 0.3015094502008 }, 5 * n )

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

STATIC FUNCTION RandomSeq( aSeed, cSym, aProb, n )
   LOCAL nSym := Len( aProb ), aCum := Array( nSym ), nAcc := 0, seed := aSeed[ 1 ], cOut := "", nDone, m, i, j, r
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
         cOut += SubStr( cSym, j, 1 )
      NEXT
      cOut += Chr( 10 )
   NEXT
   aSeed[ 1 ] := seed
   RETURN cOut
