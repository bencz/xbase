// A function whose body has a loop with nothing in it that can fail, inlined: the poll on the loop's back edge may run
// a destructor (xBase code), which sees the function on the call stack, at its line, as its call would show it.
PROCEDURE Main()
   Make()
   Spin()
RETURN

STATIC FUNCTION Make()
   LOCAL o := Gone():New()
   o := NIL
RETURN NIL

STATIC FUNCTION Spin()
   LOCAL a := NIL
   DO WHILE .T.
      a := { a }
   ENDDO
RETURN a

CREATE CLASS Gone
   EXPORTED:
   METHOD New
   DESTRUCTOR Bye
ENDCLASS

METHOD New() CLASS Gone
RETURN Self

METHOD PROCEDURE Bye() CLASS Gone
   ? "destroyed in", ProcName( 1 ), ProcName( 2 ), ProcName( 3 ), ProcLine( 1 ), ProcLine( 2 )
   QUIT
RETURN
