// Destructors. Compared with the reference: DESTRUCTOR name declares one (implemented with METHOD PROCEDURE or
// DESTRUCTOR name CLASS ...), a subclass inherits it, and an object whose class line has several runs each, its own
// class's first; an object the start function still holds is destroyed when it returns, before the EXIT procedures,
// and an error in a destructor is the error block's like any other. Deliberate differences: objects are destroyed
// by the collector (here __GcCollect()) once unreachable, not the moment the last reference goes; a destructor that
// keeps its object makes it live again, without an error, and it is not destroyed twice. Also many objects, a BREAK
// in a destructor (it ends the destructor only), destructors creating objects, destructors that run while a BREAK
// is on its way (they run whole, and the BREAK goes on with its value), a destructor that allocates much (it may
// collect) and one that collects, whose objects are destroyed after it.
STATIC aKeep := {}, nDone := 0, nRan := 0, nFinished := 0

PROCEDURE Main()
   Make( "TA", "a1" )
   ? "collect:", Collect()
   Make( "TB", "b1" )
   ? "inherited:", Collect()
   Make( "TC", "c1" )
   ? "chained:", Collect()
   Make( "TD", "d1" )
   ? "kept:", Collect(), Len( aKeep ), aKeep[ 1 ]:name
   aKeep := {}
   ? "not twice:", Collect()
   Make( "TE", "e1" )
   ? "error:", Collect()
   Make( "TF", "f1" )
   ? "break:", Collect()
   Many()
   Collect()
   ? "many:", nDone
   Make( "TG", "g1" )
   ? "creating:", Collect()
   Collect()
   ? "while breaking:", Breaking(), nRan == nFinished, nRan > 0
   Make( "THeavy", "h1" )
   ? "allocating:", Collect()
   Make( "TCollects", "c2" )
   ? "collecting:", Collect()
   TA():New( "local at exit" )
   ? "end of main"
RETURN

EXIT PROCEDURE Bye
   ? "exit procedure"
RETURN

STATIC FUNCTION Make( cClass, cName )
   LOCAL o := &cClass():New( cName )
RETURN NIL

STATIC FUNCTION Collect()
   __GcCollect()
RETURN "collected"

STATIC FUNCTION Breaking()
   LOCAL i, e, nCaught := 0
   ErrorBlock( {| oErr | Break( oErr ) } )
   FOR i := 1 TO 20000
      TWork():New()
      BEGIN SEQUENCE
         e := Space( 2000 ) + i
      RECOVER USING e
         nCaught += IIF( ValType( e ) == "O" .AND. e:operation == "+", 1, 0 )
      END SEQUENCE
   NEXT
RETURN nCaught == 20000

STATIC PROCEDURE Many()
   LOCAL i
   FOR i := 1 TO 3000
      TCount():New()
   NEXT
RETURN

CLASS TA
   VAR name
   METHOD New( n ) INLINE ::name := n, Self
   DESTRUCTOR Done
ENDCLASS

METHOD PROCEDURE Done CLASS TA
   ?? " [destroy TA " + ::name + "]"
RETURN

CLASS TB INHERIT TA
ENDCLASS

CLASS TC INHERIT TA
   DESTRUCTOR Done2()
ENDCLASS

DESTRUCTOR Done2 CLASS TC
   ?? " [destroy TC " + ::name + "]"
RETURN

CLASS TD INHERIT TA
   DESTRUCTOR Keep
ENDCLASS

METHOD PROCEDURE Keep CLASS TD
   ?? " [keep " + ::name + "]"
   AAdd( aKeep, Self )
RETURN

CLASS TE INHERIT TA
   DESTRUCTOR Fail
ENDCLASS

METHOD PROCEDURE Fail CLASS TE
   LOCAL n := 1 / 0
   ?? " [divided " + Str( n, 1 ) + "]"
RETURN

CLASS TF INHERIT TA
   DESTRUCTOR Stop
ENDCLASS

METHOD PROCEDURE Stop CLASS TF
   ?? " [break]"
   Break( "out" )
   ?? " [not here]"
RETURN

CLASS TCount
   DESTRUCTOR Count
ENDCLASS

METHOD PROCEDURE Count CLASS TCount
   nDone++
RETURN

CLASS TG INHERIT TA
   DESTRUCTOR Spawn
ENDCLASS

METHOD PROCEDURE Spawn CLASS TG
   LOCAL o := TA():New( "made by " + ::name )
   ?? " [spawned " + o:name + "]"
RETURN

CLASS TWork
   DESTRUCTOR Work
ENDCLASS

METHOD PROCEDURE Work CLASS TWork
   LOCAL a := {}
   nRan++
   AAdd( a, Space( 10 ) )
   nFinished++
RETURN

CLASS THeavy INHERIT TA
   DESTRUCTOR Heavy
ENDCLASS

METHOD PROCEDURE Heavy CLASS THeavy
   LOCAL i, s
   FOR i := 1 TO 2000
      s := Space( 1000000 )
   NEXT
   ?? " [allocated " + LTrim( Str( i - 1 ) ) + " MB]"
RETURN

CLASS TCollects INHERIT TA
   DESTRUCTOR Collects
ENDCLASS

METHOD PROCEDURE Collects CLASS TCollects
   Make( "TA", "made by " + ::name )
   __GcCollect( .T. )
   __GcCollect()
   ?? " [collected in " + ::name + "]"
RETURN

REQUEST TA, TB, TC, TD, TE, TF, TG, THeavy, TCollects
