// A line of 8 classes, each inheriting the one before: 4,000,000 rounds, on an object of the last made by a macro (its
// class unknown to the compiler), of a method each class overrides and that calls Super's down to the first (8
// calls), a method only the first class has, and variables declared by the first, a middle and the last class (read,
// and the first one assigned).
PROCEDURE Main()
   LOCAL t := ElapsedMs(), o := &( "TL7():New()" ), i, n := 0
   FOR i := 1 TO 4000000
      n += o:Depth() + o:Base() + o:v0 + o:v4 + o:v7
      o:v0 := i % 3
   NEXT
   // o:Depth() is 8 + 7 + ... + 1 = 36, o:Base() is o:v0 + 100, o:v4 + o:v7 is 11; o:v0 is ( i - 1 ) % 3, 0 at first
   ? "inheritance_depth", ElapsedMs() - t, IIf( n == 4000000 * ( 36 + 100 + 11 ) + 2 * 3999999, "ok", "WRONG" )
RETURN

CLASS TL0
   VAR v0 INIT 0
   METHOD Depth() INLINE 1
   METHOD Base() INLINE ::v0 + 100
ENDCLASS

CLASS TL1 INHERIT TL0
   VAR v1 INIT 1
   METHOD Depth() INLINE ::Super:Depth() + 2
ENDCLASS

CLASS TL2 INHERIT TL1
   VAR v2 INIT 2
   METHOD Depth() INLINE ::Super:Depth() + 3
ENDCLASS

CLASS TL3 INHERIT TL2
   VAR v3 INIT 3
   METHOD Depth() INLINE ::Super:Depth() + 4
ENDCLASS

CLASS TL4 INHERIT TL3
   VAR v4 INIT 4
   METHOD Depth() INLINE ::Super:Depth() + 5
ENDCLASS

CLASS TL5 INHERIT TL4
   VAR v5 INIT 5
   METHOD Depth() INLINE ::Super:Depth() + 6
ENDCLASS

CLASS TL6 INHERIT TL5
   VAR v6 INIT 6
   METHOD Depth() INLINE ::Super:Depth() + 7
ENDCLASS

CLASS TL7 INHERIT TL6
   VAR v7 INIT 7
   METHOD Depth() INLINE ::Super:Depth() + 8
ENDCLASS
