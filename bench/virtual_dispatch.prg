// Virtual dispatch: 1,000 shapes of 6 classes that override Area() and Perimeter() of their parent TShape (made from
// a fixed seed, in random order), gone through 1,000 times: each shape's Area() and Perimeter() from one call site
// (6 classes there, more than a send cache keeps), and Measure(), which TShape alone has and which calls the
// overriding Area() through Self.
STATIC nSeed := 6502

PROCEDURE Main()
   LOCAL aShapes := Array( 1000 ), aClasses := { {|| TCircle() }, {|| TSquare() }, {|| TRect() }, ;
                    {|| TTriangle() }, {|| THexagon() }, {|| TEllipse() } }, t, i, o, nArea := 0, nPer := 0, nM := 0
   FOR i := 1 TO Len( aShapes )
      o := Eval( aClasses[ Rand( 6 ) + 1 ] )
      aShapes[ i ] := o:New( Rand( 100 ) + 1, Rand( 100 ) + 1 )
   NEXT
   t := ElapsedMs()
   FOR i := 1 TO 1000
      FOR EACH o IN aShapes
         nArea += o:Area()
         nPer += o:Perimeter()
         nM += o:Measure()
      NEXT
   NEXT
   ? "virtual_dispatch", ElapsedMs() - t, ;
     IIf( nArea == 1000 * 7844067 .AND. nPer == 1000 * 245962 .AND. nM == 1000 * ( 7844067 + 1000 ), "ok", "WRONG" )
RETURN

STATIC FUNCTION Rand( n )
   nSeed := nSeed * 16807 % 2147483647
RETURN nSeed % n

CLASS TShape
   VAR x INIT 0
   VAR y INIT 0
   METHOD New( x, y ) INLINE ::x := x, ::y := y, Self
   METHOD Area() INLINE 0
   METHOD Perimeter() INLINE 0
   METHOD Measure() INLINE ::Area() + 1
ENDCLASS

CLASS TCircle INHERIT TShape
   METHOD Area() INLINE 3 * ::x * ::x
   METHOD Perimeter() INLINE 6 * ::x
ENDCLASS

CLASS TSquare INHERIT TShape
   METHOD Area() INLINE ::x * ::x
   METHOD Perimeter() INLINE 4 * ::x
ENDCLASS

CLASS TRect INHERIT TShape
   METHOD Area() INLINE ::x * ::y
   METHOD Perimeter() INLINE 2 * ( ::x + ::y )
ENDCLASS

CLASS TTriangle INHERIT TShape
   METHOD Area() INLINE ::x * ::y * 2 / 2
   METHOD Perimeter() INLINE 3 * ::x
ENDCLASS

CLASS THexagon INHERIT TShape
   METHOD Area() INLINE 6 * ::x * ::x
   METHOD Perimeter() INLINE 6 * ::x
ENDCLASS

CLASS TEllipse INHERIT TShape
   METHOD Area() INLINE 3 * ::x * ::y
   METHOD Perimeter() INLINE 3 * ( ::x + ::y )
ENDCLASS
