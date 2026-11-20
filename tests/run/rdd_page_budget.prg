// The pages that tables and indexes keep in memory count in one total for the process, which __dbPageCache() reports
// with its budget: past each file's minimum, a cache grows only while the total stays within the budget, else it
// reuses its own oldest pages; a closed file's pages leave the total. Searched the same way under the default budget
// and under one too small for them (NTX: four files of one order; CDX: one file of four tags), every key is found
// at its record, and the pages kept are a fraction of what the default lets them take. The files start with
// "rdd_page_budget", deleted at the end.
REQUEST DBFCDX
FIELD K1, K2, K3, K4

#define RECORDS 50000
// What the caches keep whatever the budget, with room to spare: four NTX files' first 256 pages of 1 KB, the table's
// first 64 KB (a CDX file's first 512 pages take less than 2 MB)
#define MINIMUMS ( 4 * 256 * 1100 + 65536 + 32768 )

PROCEDURE Main()
   LOCAL aCache := __dbPageCache(), nDefault := aCache[ 2 ], nFull, nSmall

   ? "at start:", aCache[ 1 ], nDefault == 256 * 1024 * 1024
   Build()

   nFull := Search( "DBFNTX" )
   ? "NTX default:", nFull > 8 * 1024 * 1024, "closed:", __dbPageCache()[ 1 ]
   ? "set to 1 MB, before:", __dbPageCache( 1024 * 1024 )[ 2 ] == nDefault
   nSmall := Search( "DBFNTX" )
   ? "NTX 1 MB:", nSmall <= 1024 * 1024 + MINIMUMS, nSmall * 4 < nFull, "closed:", __dbPageCache()[ 1 ]

   __dbPageCache( nDefault )
   nFull := Search( "DBFCDX" )
   ? "CDX default:", nFull > 8 * 1024 * 1024, "closed:", __dbPageCache()[ 1 ]
   __dbPageCache( 0 ) // every cache at its minimum
   nSmall := Search( "DBFCDX" )
   ? "CDX none:", nSmall <= 2 * 1024 * 1024, nSmall * 4 < nFull, "closed:", __dbPageCache()[ 1 ]

   __dbPageCache( 12345 )
   ? "ignored:", __dbPageCache( -1 )[ 2 ], __dbPageCache( -0.5 )[ 2 ], __dbPageCache( "x" )[ 2 ], __dbPageCache()[ 2 ]
   __dbPageCache( nDefault )
   AEval( Directory( "rdd_page_budget*.*" ), {| a | FErase( a[ 1 ] ) } )
RETURN

// The key of record n in field i: distinct per field, in an order that differs from the records'.
STATIC FUNCTION KeyOf( n, i )
RETURN Chr( 64 + i ) + StrZero( ( n * ( 7919 + 2 * i ) ) % 100003, 6 ) + Replicate( Chr( 96 + i ), 13 )

STATIC PROCEDURE Build()
   LOCAL n, cRdd
   dbCreate( "rdd_page_budget", { { "K1", "C", 20, 0 }, { "K2", "C", 20, 0 }, { "K3", "C", 20, 0 }, ;
      { "K4", "C", 20, 0 } } )
   USE rdd_page_budget EXCLUSIVE NEW
   FOR n := 1 TO RECORDS
      APPEND BLANK
      REPLACE K1 WITH KeyOf( n, 1 ), K2 WITH KeyOf( n, 2 ), K3 WITH KeyOf( n, 3 ), K4 WITH KeyOf( n, 4 )
   NEXT
   CLOSE
   FOR EACH cRdd IN { "DBFNTX", "DBFCDX" }
      USE rdd_page_budget VIA cRdd EXCLUSIVE NEW
      IF cRdd == "DBFNTX"
         INDEX ON K1 TO rdd_page_budget1
         INDEX ON K2 TO rdd_page_budget2
         INDEX ON K3 TO rdd_page_budget3
         INDEX ON K4 TO rdd_page_budget4
      ELSE
         INDEX ON K1 TAG k1 TO rdd_page_budget
         INDEX ON K2 TAG k2 TO rdd_page_budget
         INDEX ON K3 TAG k3 TO rdd_page_budget
         INDEX ON K4 TAG k4 TO rdd_page_budget
      ENDIF
      CLOSE
   NEXT
RETURN

// Seeks every record's keys, by each order in turn, the records out of their order (the table's pages kept too), and
// reads its fields: the bytes the caches took at the end, when every key was found at its record; -1 otherwise.
STATIC FUNCTION Search( cRdd )
   LOCAL n, m, i, nWrong := 0, nBytes
   USE rdd_page_budget VIA cRdd EXCLUSIVE NEW
   IF cRdd == "DBFNTX"
      SET INDEX TO rdd_page_budget1, rdd_page_budget2, rdd_page_budget3, rdd_page_budget4
   ELSE
      SET INDEX TO rdd_page_budget
   ENDIF
   FOR i := 1 TO 4
      ordSetFocus( i )
      FOR n := 1 TO RECORDS
         m := ( n * 7 ) % RECORDS + 1
         IF !dbSeek( KeyOf( m, i ) ) .OR. RecNo() != m .OR. !( FieldGet( 5 - i ) == KeyOf( m, 5 - i ) )
            ++nWrong
         ENDIF
      NEXT
   NEXT
   nBytes := __dbPageCache()[ 1 ]
   CLOSE
   IF nWrong != 0
      ? cRdd, "wrong:", nWrong
      RETURN -1
   ENDIF
RETURN nBytes
