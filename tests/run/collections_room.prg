// Collections give a peak's room back: a hash that held a million keys, cleared 100000 times and refilled with one
// key each time (each clear costs what it holds now: it drops the large store), keys and order right after; a hash
// whose keys all but one are deleted (it moves into a small store: the heap's memory comes back); an array of two
// million elements cut down by ASize (its own allocation trimmed), its first elements kept; 30 strings of 640000
// bytes made by += and kept (their room trimmed to their length by the collector); a shared array
// and hash (seen by another thread) shrunk the same way stay right. Also Distinct over logicals and NIL (a flag each),
// mixed with numbers and strings, in order of first appearance.
PROCEDURE Main()
   LOCAL h := { => }, i, t, a, nBase, nPeak, x, c := ""
   __GcCollect()
   nBase := Heap()
   FOR i := 1 TO 1000000
      h[ i ] := i
   NEXT
   nPeak := Heap()
   t := ElapsedMs()
   FOR i := 1 TO 100000
      HClear( h )
      h[ "k" + Str( i % 10, 1 ) ] := i
   NEXT
   ? "clears:", ElapsedMs() - t < 20000, Len( h ), HKeyAt( h, 1 ), h[ "k0" ] // a clear costing the peak: minutes
   h := NIL
   __GcCollect()

   __GcCollect()
   nBase := Heap()
   h := { => }
   FOR i := 1 TO 300000 // numbers: the heap's growth is the hash's store alone
      h[ i ] := i
   NEXT
   nPeak := Heap()
   FOR i := 1 TO 299999
      HDel( h, i )
   NEXT
   __GcCollect()
   __GcCollect()
   ? "deleted all but one:", Len( h ), h[ 300000 ], HKeyAt( h, 1 ), Heap() < nBase + ( nPeak - nBase ) / 2
   h[ "new" ] := 1
   ? "grows again:", Len( h ), HKeyAt( h, 2 )

   a := Array( 2000000 )
   FOR i := 1 TO 10
      a[ i ] := i * 7
   NEXT
   nPeak := Heap()
   ASize( a, 10 )
   __GcCollect()
   __GcCollect()
   x := 0
   AEval( a, {| n | x += n } )
   ? "array cut down:", Len( a ), x, Heap() < nBase + ( nPeak - nBase ) / 2
   AAdd( a, 1 )
   ? "grows again:", Len( a ), a[ 11 ]

   __GcCollect()
   nBase := Heap()
   a := {}
   FOR i := 1 TO 30 // each kept once it stopped growing: the collector trims its room (here about twice its length)
      AAdd( a, Built( 630001 ) )
   NEXT
   __GcCollect()
   __GcCollect()
   ? "strings built by +=:", Len( a[ 30 ] ), Right( a[ 30 ], 3 ), Heap() < nBase + 30 * 640000 * 1.4
   a := NIL

   a := Array( 5000 )
   AFill( a, 3 )
   h := { => }
   FOR i := 1 TO 5000
      h[ i ] := i
   NEXT
   ThreadStart( {|| Len( a ) + Len( h ) } ):Join() // shared now
   ASize( a, 10 )
   FOR i := 1 TO 4990
      HDel( h, i )
   NEXT
   ? "shared:", Len( a ), a[ 10 ], Len( h ), HKeyAt( h, 1 ), h[ 5000 ]

   FOR EACH x IN { 1, .T., NIL, "a", .F., .T., NIL, 1, "a", .F., 2 }:Distinct()
      c += Shown( x ) + " "
   NEXT
   ? "distinct:", c
RETURN

// A string of n bytes, appended to 10000 bytes at a time (its room doubling as it grows, compiled with -O1 or more).
STATIC FUNCTION Built( n )
   LOCAL c := "", cPiece := Replicate( "0123456789", 1000 )
   DO WHILE Len( c ) < n
      c += cPiece
   ENDDO
RETURN c

// The heap's memory, in bytes.
STATIC FUNCTION Heap()
RETURN __GcMemory()[ 2 ]

STATIC FUNCTION Shown( x )
   DO CASE
   CASE x == NIL
      RETURN "NIL"
   CASE ValType( x ) == "L"
      RETURN IIf( x, ".T.", ".F." )
   CASE ValType( x ) == "N"
      RETURN LTrim( Str( x ) )
   ENDCASE
RETURN x
