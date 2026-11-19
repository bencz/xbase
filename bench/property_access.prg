// Property access: 3,000,000 rounds on an object made by a macro (its class unknown to the compiler, so each access
// is a send through its cache) of variable reads and assignments (with :=, += and ++), a computed ACCESS and an ASSIGN
// method, and the same through WITH OBJECT.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), o := &( "TAccount():New()" ), i, n := 0
   FOR i := 1 TO 3000000
      o:nBalance += 2
      o:nCount++
      o:cLast := "d"
      n += o:nBalance - o:nCount + o:Doubled
      o:Limit := i % 10
      WITH OBJECT o
         :nBalance -= 1
         n += :nLimit + Len( :cLast )
      END
   NEXT
   // round i reads nBalance i + 1, nCount i, Doubled i, then nLimit 10 * ( i % 10 ) and Len( cLast ) 1: it adds
   // i + 1 + 10 * ( i % 10 ) + 1
   ? "property_access", ElapsedMs() - t, ;
     IIf( n == 4500001500000 + 10 * 13500000 + 2 * 3000000 .AND. o:nBalance == 3000000, "ok", "WRONG" )
RETURN

CLASS TAccount
   VAR nBalance INIT 0
   VAR nCount INIT 0
   VAR nLimit INIT 0
   VAR cLast INIT ""
   ACCESS Doubled INLINE ::nBalance * 2 - ::nCount - 2
   ASSIGN Limit( x ) INLINE ::nLimit := x * 10
ENDCLASS
