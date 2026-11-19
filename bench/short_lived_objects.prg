// Objects that die at once: 2,000,000 small vectors of a compiled class made with :New(), each added to a running
// sum by a method that returns a new vector, so three objects are made and as many dropped per step.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), i, v, s := TVec():New( 0, 0 ), n := 0, ok
   FOR i := 1 TO 2000000
      v := TVec():New( i % 10, i % 7 - 3 )
      s := s:Add( v )
      n += v:Dot( s:Scaled( 0 ) ) + v:x
   NEXT
   ok := s:x == 9000000 .AND. s:y == -3 .AND. n == 9000000
   ? "short_lived_objects", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

CLASS TVec
   VAR x INIT 0
   VAR y INIT 0
   METHOD New( nX, nY )
   METHOD Add( o ) INLINE TVec():New( ::x + o:x, ::y + o:y )
   METHOD Scaled( k ) INLINE TVec():New( ::x * k, ::y * k )
   METHOD Dot( o ) INLINE ::x * o:x + ::y * o:y
ENDCLASS

METHOD New( nX, nY ) CLASS TVec
   ::x := nX
   ::y := nY
   RETURN Self
