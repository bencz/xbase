// Changes made while the page caches are at their minimum (the budget of __dbPageCache() at 0): pages changed are
// written back when their slot is taken, so nothing is lost however few pages are kept. Indexes built, keys added,
// changed and removed (page splits and merges), records appended and replaced, with eight NTX files open at once and
// with a CDX file of four tags; then the files closed, reopened under the default budget and checked whole: each
// order in key order, every key found at its record, the counts. The budget changed while files are open: lowered
// below what they take (no cache gives back what it has; none grows), raised again (they grow). A shared table and
// its index (other programs may change them) under a budget of 0 too. Every cache's pages leave the total when its
// file closes: the total is back to 0 at each end. The files start with "rdd_page_budget_writes", deleted at the end.
REQUEST DBFCDX
FIELD ID, NAME, AMOUNT

#define RECORDS 20000
#define ORDERS 8

PROCEDURE Main()
   LOCAL nDefault := __dbPageCache()[ 2 ]

   __dbPageCache( 0 )
   Changes( "DBFNTX" )
   ? "NTX closed:", __dbPageCache()[ 1 ]
   __dbPageCache( nDefault )
   ? "NTX check:", Check( "DBFNTX" ), "closed:", __dbPageCache()[ 1 ]
   ? "changed while open:", Changed()
   ? "shared:", Shared(), "closed:", __dbPageCache()[ 1 ]

   __dbPageCache( 0 )
   Changes( "DBFCDX" )
   ? "CDX closed:", __dbPageCache()[ 1 ]
   __dbPageCache( nDefault )
   ? "CDX check:", Check( "DBFCDX" ), "closed:", __dbPageCache()[ 1 ]
   Erase()
RETURN

STATIC PROCEDURE Erase()
   AEval( Directory( "rdd_page_budget_writes*.*" ), {| a | FErase( a[ 1 ] ) } )
RETURN

// The name of record n after the changes: every third renamed, so its keys move across the orders' pages.
STATIC FUNCTION NameOf( n, lChanged )
   IF lChanged .AND. n % 3 == 0
      RETURN "Z" + StrZero( ( n * 4099 ) % 100019, 6 ) + "-renamed"
   ENDIF
RETURN "A" + StrZero( ( n * 7919 ) % 100003, 6 )

// The key expression of order k: each order sorts the records differently.
STATIC FUNCTION KeyOf( k )
   LOCAL aKeys := { "NAME", "Upper( NAME ) + Str( ID, 6 )", "Str( AMOUNT, 10, 2 ) + NAME", "Str( ID, 6 )", ;
      "Right( NAME, 3 ) + Str( ID, 6 )", "Str( 999999 - ID, 6 )", "Str( -AMOUNT, 10, 2 ) + Str( ID, 6 )", ;
      "SubStr( NAME, 3 ) + Str( ID, 6 )" }
RETURN aKeys[ k ]

// Builds the table and its orders under the budget in force, then appends, replaces keys and deletes.
STATIC PROCEDURE Changes( cRdd )
   LOCAL n, k
   Erase()
   dbCreate( "rdd_page_budget_writes", { { "ID", "N", 6, 0 }, { "NAME", "C", 20, 0 }, { "AMOUNT", "N", 10, 2 } }, cRdd )
   USE rdd_page_budget_writes VIA cRdd EXCLUSIVE NEW
   FOR n := 1 TO RECORDS / 2
      APPEND BLANK
      REPLACE ID WITH n, NAME WITH NameOf( n, .F. ), AMOUNT WITH ( n * 37 ) % 1000 + n / 100
   NEXT
   FOR k := 1 TO ORDERS
      IF cRdd == "DBFNTX"
         INDEX ON &( KeyOf( k ) ) TO ( "rdd_page_budget_writes" + LTrim( Str( k ) ) )
      ELSEIF k <= 4
         INDEX ON &( KeyOf( k ) ) TAG ( "t" + LTrim( Str( k ) ) ) TO rdd_page_budget_writes
      ENDIF
   NEXT
   Reopen( cRdd, .T. )
   FOR n := RECORDS / 2 + 1 TO RECORDS // appended with every order open
      APPEND BLANK
      REPLACE ID WITH n, NAME WITH NameOf( n, .F. ), AMOUNT WITH ( n * 37 ) % 1000 + n / 100
   NEXT
   ordSetFocus( 4 )
   FOR n := 1 TO RECORDS
      IF n % 3 == 0 .AND. dbSeek( Str( n, 6 ) )
         REPLACE NAME WITH NameOf( n, .T. ) // its keys move in every order
      ENDIF
      IF n % 5 == 0 .AND. dbSeek( Str( n, 6 ) )
         DELETE
      ENDIF
   NEXT
   ? cRdd, "open:", __dbPageCache()[ 1 ] > 0
   CLOSE
RETURN

// The table opened again with every order, exclusive (lExclusive) or shared.
STATIC PROCEDURE Reopen( cRdd, lExclusive )
   LOCAL k
   CLOSE
   IF lExclusive
      USE rdd_page_budget_writes VIA cRdd EXCLUSIVE NEW
   ELSE
      USE rdd_page_budget_writes VIA cRdd SHARED NEW
   ENDIF
   IF cRdd == "DBFNTX"
      FOR k := 1 TO ORDERS
         ordListAdd( "rdd_page_budget_writes" + LTrim( Str( k ) ) )
      NEXT
   ELSE
      ordListAdd( "rdd_page_budget_writes" )
   ENDIF
RETURN

// Each order read whole in its order (each key not below the one before it, each record once), the count, and every
// record's key found at that record: "ok" or what was wrong.
STATIC FUNCTION Check( cRdd )
   LOCAL k, n, cPrev, xKey, aSeen, nCount, cWrong := "", bKey
   Reopen( cRdd, .T. )
   FOR k := 1 TO IIf( cRdd == "DBFNTX", ORDERS, 4 )
      ordSetFocus( k )
      bKey := &( "{|| " + KeyOf( k ) + " }" )
      aSeen := Array( RECORDS )
      AFill( aSeen, .F. )
      nCount := 0
      cPrev := NIL
      GO TOP
      DO WHILE !Eof()
         xKey := Eval( bKey )
         IF ( cPrev != NIL .AND. xKey < cPrev ) .OR. aSeen[ RecNo() ]
            cWrong += " order" + Str( k, 2 ) + " at" + Str( RecNo(), 6 )
            EXIT
         ENDIF
         aSeen[ RecNo() ] := .T.
         cPrev := xKey
         ++nCount
         SKIP
      ENDDO
      IF nCount != RECORDS .OR. ordKeyCount() != RECORDS
         cWrong += " order" + Str( k, 2 ) + " count" + Str( nCount, 6 ) + Str( ordKeyCount(), 6 )
      ENDIF
      FOR n := 1 TO RECORDS STEP 7
         GO n
         xKey := Eval( bKey )
         IF !dbSeek( xKey ) .OR. Eval( bKey ) != xKey
            cWrong += " seek" + Str( k, 2 ) + Str( n, 6 )
            EXIT
         ENDIF
      NEXT
   NEXT
   FOR n := 1 TO RECORDS
      GO n
      IF ID != n .OR. !( RTrim( NAME ) == NameOf( n, .T. ) ) .OR. Deleted() != ( n % 5 == 0 )
         cWrong += " record" + Str( n, 6 )
         EXIT
      ENDIF
   NEXT
   CLOSE
RETURN IIf( Empty( cWrong ), "ok", cWrong )

// With the NTX files open: the budget lowered below what the caches take, then raised again.
STATIC FUNCTION Changed()
   LOCAL nBefore, nLow, nHigh, n, cWrong := ""
   __dbPageCache( 256 * 1024 * 1024 )
   Reopen( "DBFNTX", .T. )
   ordSetFocus( 1 )
   GO TOP
   DO WHILE !Eof() // every page of order 1
      SKIP
   ENDDO
   nBefore := __dbPageCache()[ 1 ]
   __dbPageCache( 1 ) // below what is kept: nothing given back, nothing more made
   FOR n := 2 TO ORDERS
      ordSetFocus( n )
      GO TOP
      DO WHILE !Eof()
         SKIP
      ENDDO
   NEXT
   nLow := __dbPageCache()[ 1 ]
   __dbPageCache( 256 * 1024 * 1024 )
   FOR n := 2 TO ORDERS
      ordSetFocus( n )
      GO BOTTOM
      DO WHILE !Bof()
         SKIP -1
      ENDDO
   NEXT
   nHigh := __dbPageCache()[ 1 ]
   IF nLow < nBefore
      cWrong += " gave back"
   ENDIF
   IF nLow > nBefore + ( ORDERS - 1 ) * 256 * 1100 // the minimums of the orders read after
      cWrong += " grew past the minimums"
   ENDIF
   IF nHigh <= nLow
      cWrong += " did not grow again"
   ENDIF
   CLOSE
   IF __dbPageCache()[ 1 ] != 0
      cWrong += " left"
   ENDIF
RETURN IIf( Empty( cWrong ), "ok", cWrong )

// A shared table and its NTX orders changed under a budget of 0, then checked.
STATIC FUNCTION Shared()
   LOCAL n
   __dbPageCache( 0 )
   Reopen( "DBFNTX", .F. )
   ordSetFocus( 4 )
   FOR n := 2 TO RECORDS STEP 3
      IF n % 3 != 0 .AND. dbSeek( Str( n, 6 ) ) .AND. RLock()
         REPLACE NAME WITH NameOf( n, .F. ) // the same name again: keys rewritten in place
         dbUnlock()
      ENDIF
   NEXT
   CLOSE
   __dbPageCache( 256 * 1024 * 1024 )
RETURN Check( "DBFNTX" )
