// New sent to an object whose class the compiler knows, without a New of the class: its Init with the arguments, then
// the object (whatever Init returns): an Init of the class, of a parent, none; an Init that returns something else; an
// Init whose error is substituted (the object still the result); an Init HIDDEN in a parent (the runtime's error); a
// New of the class's own; New in a chain of messages and in a loop.
PROCEDURE Main()
   LOCAL o, i, n := 0, e
   o := TA():New( 4 )
   ? "init of the class:", o:ClassName(), o:v
   o := TC():New( 5 )
   ? "init of a parent:", o:ClassName(), o:v, o:w
   o := TB():New( 6 )
   ? "no init:", o:ClassName(), o:v
   o := TR():New()
   ? "init returns another value:", o:ClassName()
   ? "chain:", TA():New( 7 ):v, TA():New( 8 ):Twice()
   FOR i := 1 TO 1000
      n += TA():New( i ):v
   NEXT
   ? "loop:", n
   o := TD():New( 9 )
   ? "own new:", o:ClassName(), o:v
   ErrorBlock( {| e | 99 } )
   o := TF():New( 1 )
   ? "substituted error:", ValType( o ), o:ClassName(), o:v
   ErrorBlock( {| e | Break( e ) } )
   BEGIN SEQUENCE
      o := TE():New( 2 )
      ? "hidden init: made"
   RECOVER USING e
      ? "hidden init:", e:subCode, e:operation
   END SEQUENCE
   RETURN

CLASS TA
   VAR v
   METHOD Init( n )
   METHOD Twice() INLINE ::v * 2
ENDCLASS

METHOD Init( n ) CLASS TA
   ::v := n * 10
   RETURN Self

CLASS TB
   VAR v INIT "default"
ENDCLASS

CLASS TC INHERIT TA
   VAR w INIT "w"
ENDCLASS

CLASS TR
   METHOD Init()
ENDCLASS

METHOD Init() CLASS TR
   RETURN "not the object"

CLASS TD
   VAR v
   METHOD New( n )
ENDCLASS

METHOD New( n ) CLASS TD
   ::v := -n
   RETURN Self

CLASS TF
   VAR v
   METHOD Init( n )
ENDCLASS

METHOD Init( n ) CLASS TF
   ::v := n + "x" // an error the block substitutes
   RETURN Self

CLASS TP
   VAR v
   HIDDEN:
   METHOD Init( n )
ENDCLASS

METHOD Init( n ) CLASS TP
   ::v := n
   RETURN Self

CLASS TE INHERIT TP
ENDCLASS
