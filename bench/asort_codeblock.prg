// ASort with a comparison block over records: 150,000 employees { name, department, salary, id } made from a fixed
// seed, ordered by department, then salary descending, then name, through a block that compares three keys (the
// sort is stable, so equal records keep their order). The order is checked with a running hash of the ids.
STATIC nSeed := 4242

PROCEDURE Main()
   LOCAL a := Array( 150000 ), t, i, k, c, nChk := 0, ok
   FOR i := 1 TO Len( a )
      c := ""
      FOR k := 1 TO 8
         c += Chr( 65 + Rand( 26 ) )
      NEXT
      a[ i ] := { c, Rand( 40 ) + 1, Rand( 90000 ) + 1000, i }
   NEXT
   t := ElapsedMs()
   ASort( a, , , {| p, q | IIf( p[ 2 ] != q[ 2 ], p[ 2 ] < q[ 2 ], ;
                           IIf( p[ 3 ] != q[ 3 ], p[ 3 ] > q[ 3 ], p[ 1 ] < q[ 1 ] ) ) } )
   FOR i := 1 TO Len( a )
      nChk := ( nChk * 31 + a[ i ][ 4 ] ) % 1000000007
   NEXT
   ok := nChk == 773935306 .AND. a[ 1 ][ 4 ] == 84060 .AND. a[ 1 ][ 1 ] == "SOFHDANH" .AND. ;
         a[ 75001 ][ 4 ] == 10853 .AND. a[ 150000 ][ 4 ] == 11664
   ? "asort_codeblock", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
RETURN

STATIC FUNCTION Rand( n )
   nSeed := nSeed * 16807 % 2147483647
RETURN nSeed % n
