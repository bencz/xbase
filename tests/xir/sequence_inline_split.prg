// A body inlined at a call in BEGIN SEQUENCE whose callee entered and left a body of its own within one block (Outer,
// with Inner inlined): the copied block is split where Inner starts and where it ends, so that each piece unwinds
// through the pads that leave the bodies open at its operations (Inner and Outer, then Outer alone) to the handler.
FUNCTION Main( n )
   LOCAL r := 0
   BEGIN SEQUENCE
      r := Outer( n )
   END
RETURN r

STATIC FUNCTION Outer( x )
RETURN Inner( x ) + 1

STATIC FUNCTION Inner( y )
RETURN y * 2
