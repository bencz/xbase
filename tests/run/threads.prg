// Threads of the program: ThreadStart() with a block or a function reference and their arguments, Join()'s result, a
// thread that ends by BREAK or by an error its block turned into one; counters changed by N threads M times each
// through an AtomicInt, through an array element, a PUBLIC and a STATIC under a Mutex (a matrix of thread counts and
// rounds, the values computed by the threads checked against the formula); the Mutex taken again by its holder, tried
// by another thread, released by a thread that does not hold it; Join() by the thread itself; AtomicInt's arithmetic
// at the ends of the 64-bit range and its argument errors; the environment a thread starts with (SET values,
// ErrorBlock(), the PRIVATEs visible, which it shares with its creator, and releases) and what does not come back (its
// SETs, its PRIVATEs), a PUBLIC created or released by a thread, one that eight threads create at once, CLEAR MEMORY in
// a thread; arguments passed @ (their values); threads that start threads; collections while threads allocate; and
// lines written at once by several threads (more than eight values, more than 256 bytes), each whole.
STATIC s_nStatic := 0
MEMVAR gPublic, gMade, pShared, gRace, gGone

PROCEDURE Main()
   LOCAL aT, n, oM, oA, e, i, a
   PUBLIC gPublic := 0
   PRIVATE pShared := "creator"

   ? "results:", ThreadStart( {| x, y | x * y }, 6, 7 ):Join(), ThreadStart( @Twice(), "ab" ):Join(), ;
     ThreadStart( {|| NIL } ):Join(), ThreadStart( {|| Break( "x" ) } ):Join()
   n := 1
   a := { 1, 2 }
   ? "by value:", ThreadStart( {| x | x := 5 }, @n ):Join(), n, ThreadStart( {| x | x := 7 }, @a[ 2 ] ):Join(), a[ 2 ]
   ErrorBlock( {| e | Break( e ) } )
   ? "error in block:", ThreadStart( {|| 1 + "a" } ):Join()

   FOR EACH n IN { 1, 2, 4, 8 }
      FOR EACH i IN { 1, 100, 3000 }
         ? "count", n, i, Count( n, i ), n * i
      NEXT
   NEXT

   oM := Mutex()
   ? "mutex:", oM:ClassName(), oM:TryLock(), oM:TryLock(), ThreadStart( {|| oM:TryLock() } ):Join()
   oM:Unlock()
   ? "held once:", ThreadStart( {|| oM:TryLock() } ):Join()
   oM:Unlock()
   ? "free:", ThreadStart( {|| oM:TryLock() .AND. Unlocked( oM ) } ):Join()
   BEGIN SEQUENCE
      oM:Unlock()
   RECOVER USING e
      ? "unlock free:", e:subSystem, e:genCode, e:subCode, e:description, e:operation
   END SEQUENCE
   oM:Lock()
   ? "unlock by another:", ThreadStart( {|| UnlockError( oM ) } ):Join()
   ? "waits:", Waits( oM )
   ? "methods:", ArrayText( ObjMethods( oM ) ), Len( ObjVars( oM ) ), ObjHasMethod( oM, "lock" ), ;
     ObjHasMethod( oM, "join" ), ObjHasVar( oM, "(STATE)" )

   a := { NIL }
   oM:Lock()
   a[ 1 ] := ThreadStart( {|| oM:Lock(), JoinError( a[ 1 ] ) } )
   oM:Unlock()
   ? "join itself:", a[ 1 ]:Join(), a[ 1 ]:IsAlive(), a[ 1 ]:ClassName(), ArrayText( ObjMethods( a[ 1 ] ) )

   oA := AtomicInt()
   ? "atomic:", oA:ClassName(), oA:Value, oA:Add( 5 ), oA:Add( -7 ), oA:Value, ( oA:Value := 10 ), oA:Value
   ? "exchange:", oA:CompareExchange( 9, 1 ), oA:Value, oA:CompareExchange( 10, 1 ), oA:Value
   ? "made:", AtomicInt( 2.7 ):Value, AtomicInt( -2.7 ):Value, AtomicInt( NIL ):Value
   oA := AtomicInt( 0x7FFFFFFFFFFFFFFF )
   ? "wraps:", oA:Add( 1 ) == -0x7FFFFFFFFFFFFFFF - 1, oA:Add( -1 ) == 0x7FFFFFFFFFFFFFFF
   ? "methods:", ArrayText( ObjMethods( oA ) ), Len( ObjVars( oA ) )
   FOR EACH e IN { {|| oA:Add( "x" ) }, {|| oA:Add() }, {|| oA:CompareExchange( 1 ) }, {|| AtomicInt( "1" ) }, ;
                   {|| oA:Value := .T. }, {|| Eval( @ThreadStart() ) }, {|| ThreadStart( 5 ) } }
      ? "error:", Caught( e )
   NEXT

   SET DATE FORMAT TO "yyyy.mm.dd"
   SET DECIMALS TO 4
   SET EXACT ON
   ? "inherited:", ThreadStart( {|| Settings() } ):Join()
   ? "private:", ThreadStart( {|| pShared } ):Join(), ThreadStart( {|| pShared := "thread" } ):Join(), pShared
   ? "own SET:", ThreadStart( {|| Set( _SET_EXACT, .F. ) } ):Join(), Set( _SET_EXACT )
   ? "own private:", ThreadStart( {|| MakePrivate() } ):Join(), Type( "pThread" )
   ? "public:", ThreadStart( {|| MakePublic() } ):Join(), gMade
   ThreadStart( {|| __MemRelease( "gMade" ) } ):Join()
   ? "released:", Type( "gMade" ), ThreadStart( {|| __MemRelease( "pShared" ), pShared } ):Join(), pShared
   pShared := "creator"
   ? "race:", Race(), gRace
   SET DATE FORMAT TO "mm/dd/yy"
   SET DECIMALS TO 2
   SET EXACT OFF

   ? "nested:", ThreadStart( {|| Nested( 3 ) } ):Join()
   ? "collections:", Allocators()
   aT := {}
   FOR n := 1 TO 6
      AAdd( aT, ThreadStart( {|| Lines() } ) )
   NEXT
   AEval( aT, {| t | t:Join() } )

   PUBLIC gGone := 1
   ThreadStart( {|| __MemClear() } ):Join()
   ? "cleared:", Type( "gGone" ), Type( "gPublic" ), pShared
RETURN

// Eight threads create the same PUBLIC at once: one variable, which each adds 1 to.
STATIC FUNCTION Race()
   LOCAL aT := {}, oM := Mutex(), oGo := Mutex(), k
   oGo:Lock()
   FOR k := 1 TO 8
      AAdd( aT, ThreadStart( {|| oGo:Lock(), oGo:Unlock(), MakeRace( oM ) } ) )
   NEXT
   oGo:Unlock()
   AEval( aT, {| t | t:Join() } )
RETURN Type( "gRace" )

STATIC FUNCTION MakeRace( oM )
   PUBLIC gRace
   oM:Lock()
   IF ValType( gRace ) == "L"
      gRace := 0
   ENDIF
   gRace++
   oM:Unlock()
RETURN NIL

STATIC FUNCTION Twice( c )
RETURN c + c

// N threads, each adding 1 M times to each counter; what they compute along the way is checked too.
STATIC FUNCTION Count( n, m )
   LOCAL aT := {}, oA := AtomicInt(), oM := Mutex(), aSum := { 0 }, k, nBad := 0
   gPublic := 0
   s_nStatic := 0
   FOR k := 1 TO n
      AAdd( aT, ThreadStart( {| nMe | Adder( nMe, m, oA, oM, aSum ) }, k ) )
   NEXT
   FOR k := 1 TO n
      IF aT[ k ]:Join() != k * m
         nBad++
      ENDIF
   NEXT
RETURN LTrim( Str( oA:Value ) ) + " " + LTrim( Str( aSum[ 1 ] ) ) + " " + LTrim( Str( gPublic ) ) + " " + ;
       LTrim( Str( s_nStatic ) ) + " " + LTrim( Str( nBad ) ) + " " + iif( aT[ 1 ]:IsAlive(), "alive", "ended" )

STATIC FUNCTION Adder( nMe, m, oA, oM, aSum )
   LOCAL i, nMine := 0
   FOR i := 1 TO m
      oA:Add( 1 )
      oM:Lock()
      aSum[ 1 ]++
      gPublic++
      oM:Lock() // its holder takes it again
      s_nStatic++
      oM:Unlock()
      oM:Unlock()
      nMine += nMe
   NEXT
RETURN nMine

STATIC FUNCTION Unlocked( oM )
   oM:Unlock()
RETURN .T.

STATIC FUNCTION UnlockError( oM )
   LOCAL e
   BEGIN SEQUENCE
      oM:Unlock()
   RECOVER USING e
      RETURN e:subCode
   END SEQUENCE
RETURN "no error"

// A thread waits on the mutex while this one holds it, then gets it once it is released.
STATIC FUNCTION Waits( oM )
   LOCAL t := ThreadStart( {|| oM:Lock(), Unlocked( oM ), "got it" } )
   ? "waiting:", t:IsAlive()
   oM:Unlock()
RETURN t:Join()

STATIC FUNCTION JoinError( t )
   LOCAL e
   BEGIN SEQUENCE
      t:Join()
   RECOVER USING e
      RETURN LTrim( Str( e:subCode ) ) + " " + e:description + " " + e:operation
   END SEQUENCE
RETURN "no error"

STATIC FUNCTION Caught( b )
   LOCAL e
   BEGIN SEQUENCE
      Eval( b )
   RECOVER USING e
      RETURN e:subSystem + "/" + LTrim( Str( e:subCode ) ) + " " + e:operation
   END SEQUENCE
RETURN "none"

STATIC FUNCTION Settings()
RETURN DToC( SToD( "20261116" ) ) + " " + Str( 1 / 3 ) + " " + iif( "ab" = "a", "inexact", "exact" )

STATIC FUNCTION MakePrivate()
   PRIVATE pThread := 1
RETURN Type( "pThread" )

STATIC FUNCTION MakePublic()
   PUBLIC gMade := "made by a thread"
RETURN Type( "gMade" )

STATIC FUNCTION Nested( n )
   LOCAL t1, t2
   IF n == 0
      RETURN 1
   ENDIF
   t1 := ThreadStart( {|| Nested( n - 1 ) } )
   t2 := ThreadStart( {|| Nested( n - 1 ) } )
RETURN t1:Join() + t2:Join()

// Threads that make garbage and collect, each building a list it checks.
STATIC FUNCTION Allocators()
   LOCAL aT := {}, k, nGood := 0
   FOR k := 1 TO 4
      AAdd( aT, ThreadStart( {| nMe | Garbage( nMe ) }, k ) )
   NEXT
   FOR k := 1 TO 4
      nGood += aT[ k ]:Join()
   NEXT
RETURN nGood

STATIC FUNCTION Garbage( nMe )
   LOCAL a := {}, i, h := { => }, nGood := 0
   FOR i := 1 TO 20000
      AAdd( a, { nMe, i, Str( i ) } )
      h[ i ] := a[ i ]
      IF i % 5000 == 0
         __GcCollect( i % 10000 != 0 )
      ENDIF
   NEXT
   FOR i := 1 TO Len( a )
      IF a[ i, 1 ] == nMe .AND. a[ i, 2 ] == i .AND. h[ i ] == a[ i ] .AND. Val( a[ i, 3 ] ) == i
         nGood++
      ENDIF
   NEXT
RETURN iif( nGood == 20000, 1, 0 )

// The same line, many times, from each thread: written whole, the output is the same whatever their order.
STATIC PROCEDURE Lines()
   LOCAL i, cLong := Replicate( "-", 300 )
   FOR i := 1 TO 50
      ? "line", 1, 2.5, .T., "of", "several", "values", 8, 9, 10, 11, cLong
      OutStd( Chr( 10 ) + "line", 1, 2.5, .T., "of", "several", "values", 8, 9, 10, 11, cLong )
   NEXT
RETURN

STATIC FUNCTION ArrayText( a )
   LOCAL c := "", x
   FOR EACH x IN a
      c += iif( Empty( c ), "", "," ) + x
   NEXT
RETURN c
