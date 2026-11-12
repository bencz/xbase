// Index files of more pages than their drivers keep in memory (512 a file): 30,000 records with random keys, an NTX
// order and a CDX tag on them, then 6,000 key changes and appends at random, so pages are dropped from memory (the
// changed ones written first) and read again. Each order then walks in its order, holds every record, and a SEEK finds
// every key. The files start with "rdd_index_pages", deleted at the end.
REQUEST DBFCDX
FIELD K
STATIC nSeed := 4242

PROCEDURE Main()
   Exercise( "DBFNTX", "rdd_index_pages_ntx", ".ntx" )
   Exercise( "DBFCDX", "rdd_index_pages_cdx", ".cdx" )
RETURN

STATIC PROCEDURE Exercise( cDriver, cName, cExt )
   LOCAL i
   FErase( cName + ".dbf" ) // left by a run that failed
   FErase( cName + cExt )
   dbCreate( cName, { { "K", "C", 10, 0 } }, cDriver )
   USE ( cName ) VIA ( cDriver ) NEW EXCLUSIVE
   FOR i := 1 TO 30000
      APPEND BLANK
      REPLACE K WITH RandKey()
   NEXT
   INDEX ON K TO ( cName )
   FOR i := 1 TO 6000
      GO Rand( LastRec() ) + 1
      IF i % 5 == 0
         APPEND BLANK
      ENDIF
      REPLACE K WITH RandKey()
   NEXT
   Check( cDriver )
   CLOSE
   FErase( cName + ".dbf" )
   FErase( cName + cExt )
RETURN

STATIC PROCEDURE Check( cDriver )
   LOCAL nCount := 0, cPrev := "", lOrdered := .T., nMisses := 0
   GO TOP
   DO WHILE ! Eof()
      IF K < cPrev
         lOrdered := .F.
      ENDIF
      cPrev := K
      nCount++
      SKIP
   ENDDO
   SET ORDER TO 0
   GO TOP
   DO WHILE ! Eof()
      IF ! SeekFinds( K )
         nMisses++
      ENDIF
      SKIP
   ENDDO
   ? cDriver, "records", LastRec(), "in order", nCount, "ordered", lOrdered, "seek misses", nMisses
RETURN

// Whether a SEEK in the order finds the key (the table's position is kept).
STATIC FUNCTION SeekFinds( cKey )
   LOCAL nRec := RecNo(), lFound
   SET ORDER TO 1
   lFound := dbSeek( cKey )
   SET ORDER TO 0
   GO nRec
RETURN lFound

STATIC FUNCTION Rand( n )
   nSeed := ( nSeed * 1103 + 12345 ) % 1000003
RETURN nSeed % n

STATIC FUNCTION RandKey()
   LOCAL c := "", i
   FOR i := 1 TO 10
      c += Chr( 48 + Rand( 10 ) )
   NEXT
RETURN c
