// A counter shared by 8 threads (ThreadStart), contended: each thread adds 1 to an AtomicInt 250,000 times with Add,
// then 50,000 times with a CompareExchange loop, then 50,000 times to an array element under SYNCHRONIZED (a Mutex).
// No increment may be lost, whatever the scheduling.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), oAtomic := AtomicInt(), oCas := AtomicInt(), oM := Mutex(), aBox := { 0 }, aT := {}, k
   FOR k := 1 TO 8
      AAdd( aT, ThreadStart( {|| Work( oAtomic, oCas, oM, aBox ) } ) )
   NEXT
   AEval( aT, {| x | x:Join() } )
   ? "shared_counter", ElapsedMs() - t, ;
     IIf( oAtomic:Value == 8 * 250000 .AND. oCas:Value == 8 * 50000 .AND. aBox[ 1 ] == 8 * 50000, "ok", "WRONG" )
RETURN

STATIC FUNCTION Work( oAtomic, oCas, oM, aBox )
   LOCAL i, n
   FOR i := 1 TO 250000
      oAtomic:Add( 1 )
   NEXT
   FOR i := 1 TO 50000
      DO WHILE .T.
         n := oCas:Value
         IF oCas:CompareExchange( n, n + 1 )
            EXIT
         ENDIF
      ENDDO
   NEXT
   FOR i := 1 TO 50000
      SYNCHRONIZED oM
         aBox[ 1 ]++
      END
   NEXT
RETURN NIL
