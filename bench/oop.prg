// Messages to objects of compiled classes: method calls (one inherited through Super), variable reads and
// assignments, in a loop of 3,000,000.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), o := TCounter():New(), i, r
   FOR i := 1 TO 3000000
      o:Add( i % 7 )
      o:n := o:n + 1
   NEXT
   r := o:Result()
   ? "oop", ElapsedMs() - t, IIf( r == 11999997, "ok", "WRONG" )
   RETURN

CLASS TBase
   VAR total INIT 0
   METHOD Add( x ) INLINE ::total += x
ENDCLASS

CLASS TCounter INHERIT TBase
   VAR n INIT 0
   METHOD Add( x ) INLINE Super:Add( x )
   METHOD Result() INLINE ::total + ::n
ENDCLASS
