// A program that stops allocating after a peak gives its memory back while it waits (the collector's trimmer: a full
// collection once the heap has seen none for a few seconds), with no collection of its own: 200 MB of strings, dropped,
// then waits (TaskDelay) until the heap holds little again. Data still referenced through such waits is kept whole:
// the trimmer's collections find it live (another 200 MB, read back after it waited past the trimmer's delay).
#define MB ( 1024 * 1024 )

PROCEDURE Main()
   LOCAL a, nStart, ok, k

   a := Strings( 200000 )
   ? "peak held:", __GcMemory()[ 2 ] > 150 * MB
   a := NIL
   nStart := ElapsedMs()
   DO WHILE __GcMemory()[ 2 ] > 64 * MB .AND. ElapsedMs() - nStart < 60000
      TaskDelay( 100 ):Wait()
   ENDDO
   ? "given back while it waits:", __GcMemory()[ 2 ] <= 64 * MB, "after the idle delay:", ElapsedMs() - nStart >= 3000

   a := Strings( 200000 )
   nStart := ElapsedMs()
   DO WHILE ElapsedMs() - nStart < 7000
      TaskDelay( 100 ):Wait()
   ENDDO
   ok := __GcMemory()[ 2 ] > 150 * MB
   FOR k := 1 TO Len( a ) STEP 997
      ok := ok .AND. a[ k ] == Replicate( Chr( 65 + k % 26 ), 1000 ) + Str( k )
   NEXT
   ? "live data kept:", ok, a[ Len( a ) ] == Replicate( Chr( 65 + Len( a ) % 26 ), 1000 ) + Str( Len( a ) )
RETURN

STATIC FUNCTION Strings( n )
   LOCAL a := Array( n ), k
   FOR k := 1 TO n
      a[ k ] := Replicate( Chr( 65 + k % 26 ), 1000 ) + Str( k )
   NEXT
RETURN a
