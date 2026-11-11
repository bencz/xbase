// A program's own class named Enumerable is another class than the runtime's: a generator's enumerator, and the
// sequences arrays make, keep answering the runtime's Enumerable messages.
PROCEDURE Main()
   ? Len( { 1, 2, 3 }:Select( {| x | x * 2 } ):ToArray() ), Gen():Sum(), Enumerable():Hello()
RETURN

FUNCTION Gen()
   YIELD 4
   YIELD 5
RETURN

CLASS Enumerable
   METHOD Hello() INLINE "mine"
ENDCLASS
