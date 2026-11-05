// Young objects stored into old ones must survive young collections: stores into array elements, grown arrays,
// cells of captured variables (in the function and in a block), STATICs, FOR EACH variables and nested arrays.
// Each check runs after a young collection and after allocations that would reuse wrongly freed slots.
STATIC s_old

PROCEDURE Main()
   LOCAL old := Array( 50 ), cap := "start", bSet, bGet, i, n, e, ok := .T., nested := { { "x" } }
   bSet := { | x | cap := x }
   bGet := { || cap }
   s_old := { "s" }
   __GcCollect()                       // everything above is old now
   FOR n := 1 TO 5
      FOR i := 1 TO 50
         old[ i ] := "elem" + Str( i * n )
      NEXT
      AAdd( old, "added" + Str( n ) )
      Eval( bSet, "block" + Str( n ) )
      s_old[ 1 ] := "static" + Str( n )
      nested[ 1 ][ 1 ] := "nested" + Str( n )
      FOR EACH e IN old
         IF e == "added" + Str( n )
            e := "each" + Str( n )
         ENDIF
      NEXT
      __GcCollect( .T. )
      Churn()
      FOR i := 1 TO 50
         ok := ok .AND. old[ i ] == "elem" + Str( i * n )
      NEXT
      ok := ok .AND. ATail( old ) == "each" + Str( n ) .AND. Eval( bGet ) == "block" + Str( n )
      ok := ok .AND. s_old[ 1 ] == "static" + Str( n ) .AND. nested[ 1 ][ 1 ] == "nested" + Str( n )
      cap := "local" + Str( n )        // the cell, written by the function itself
      __GcCollect( .T. )
      Churn()
      ok := ok .AND. Eval( bGet ) == "local" + Str( n )
      ? n, ok, Len( old )
   NEXT
   FOR i := 1 TO 300000                  // young collections triggered by allocation
      old[ ( i % 50 ) + 1 ] := "loop" + Str( i )
   NEXT
   ? old[ 1 ], old[ 50 ], __GcCollect() < 1000
   LocalInit()
   BigArray()
   RETURN

// A large old array is rescanned only around the elements written (card marking): stores spread over it, in its
// last, partial card, and after it grows, must all survive young collections.
PROCEDURE BigArray()
   LOCAL big := Array( 5000 ), n, i, ok := .T., cl
   __GcCollect()
   FOR n := 1 TO 3
      FOR i := 1 TO 5000 STEP 97
         big[ i ] := "big" + Str( i + n )
      NEXT
      big[ 5000 ] := { "last" + Str( n ) }
      __GcCollect( .T. )
      Churn()
      FOR i := 1 TO 5000 STEP 97
         ok := ok .AND. big[ i ] == "big" + Str( i + n )
      NEXT
      ok := ok .AND. big[ 5000 ][ 1 ] == "last" + Str( n )
   NEXT
   AAdd( big, "grown" + Str( 1 ) )
   __GcCollect( .T. )
   big[ 4999 ] := "after" + Str( 2 )
   __GcCollect( .T. )
   Churn()
   ? ok, ATail( big ), big[ 4999 ], Len( big )
   ASize( big, 100 )                      // shrink: dirty cards past the length are ignored
   __GcCollect( .T. )
   ASize( big, 3000 )                     // grow within the capacity: new elements are NIL
   big[ 2500 ] := "regrown" + Str( 3 )
   cl := AClone( big )                    // a young copy of an old carded array
   __GcCollect()
   big[ 2600 ] := { "fresh" + Str( 4 ) }
   cl[ 2700 ] := "clone" + Str( 5 )
   __GcCollect( .T. )
   Churn()
   ? big[ 2500 ], big[ 2600 ][ 1 ], ValType( big[ 2000 ] ), cl[ 2500 ], cl[ 2700 ], Len( cl ), ValType( cl[ 2600 ] )
   RETURN

// The cells of captured LOCALs exist from the function's start, so they can be old by the time an initializer stores
// a young value into them.
PROCEDURE LocalInit()
   LOCAL x := __GcCollect( .T. ), b := { "keep" + Str( 1 ) }, c := Make(), d := Array( 2000, 3 )
   LOCAL blk := { || { b, c, d } }
   __GcCollect( .T. )
   Churn()
   ? Len( Eval( blk )[ 1 ] ), Eval( blk )[ 1 ][ 1 ], Eval( blk )[ 2 ][ 1 ], Len( Eval( blk )[ 3 ] ), ValType( x )
   RETURN

FUNCTION Make()
   __GcCollect( .T. )
   RETURN { "made" + Str( 3 ) }

PROCEDURE Churn()
   LOCAL i, a := {}
   FOR i := 1 TO 20000
      AAdd( a, "garbage" + Str( i ) )
   NEXT
   RETURN
