// Sends that compiled code performs in place, from the send's inline cache (an object whose class the compiler does
// not know) or resolved when compiling (one it knows: object.get, object.set, a call): an object that survives
// collections (old) taking new strings and arrays through assignments while collections run, whose values must stay
// (the write barrier; each path alone, with nothing but the object referring to the new values); objects shared with
// other threads, which the runtime reads and writes under their locks; an assignment message with no argument; an
// error inside a method called in place (its call stack); a method that returns its object (chained sends); many
// sends whose classes alternate at one site; a site that gets the object and a cast of it in turn (the cast goes to
// the runtime); reads of a variable another thread keeps writing strings and numbers into (never a torn item); AWAIT
// of a method at a site whose cache holds it; a read with arguments (ignored) and assignments with none or two.
PROCEDURE Main()
   LOCAL o := THolder():New(), i, k, c, t, a, s := 0, nBad := 0
   FOR k := 1 TO 3 // the holder survives collections: it is old
      __GcCollect()
   NEXT
   FOR i := 1 TO 20000
      IF i % 100 == 0
         // Two young collections promote what the holder refers to, so that it leaves the remembered set; the new
         // values then reach a young collection only through the barrier of their assignment, and are read after
         // allocations that would take their place had they been freed.
         __GcCollect( .T. )
         __GcCollect( .T. )
      ENDIF
      o:text := Replicate( Chr( 65 + i % 26 ), 1 + i % 50 ) + LTrim( Str( i ) )
      o:list := { i, Str( i ), { i * 2 } }
      o:Keep( o:text )
      IF i % 100 == 0
         __GcCollect( .T. )
         FOR k := 1 TO 200
            t := { Replicate( "z", 40 ), k }
         NEXT
         IF o:list[ 1 ] != i .OR. o:list[ 3 ][ 1 ] != i * 2 .OR. ! Right( o:text, Len( LTrim( Str( i ) ) ) ) == ;
               LTrim( Str( i ) ) .OR. ! o:list[ 2 ] == Str( i )
            nBad++
         ENDIF
      ENDIF
   NEXT
   __GcCollect()
   ? "barrier", nBad, o:text, o:list[ 1 ], o:list[ 2 ], o:list[ 3 ][ 1 ], Len( o:kept ), o:kept[ 1 ], o:kept[ 20000 ]
   ? "barrier known class", Known()
   ? "barrier unknown class", Unknown( { THolder():New() } )
   c := 0
   FOR EACH t IN o:kept
      c += Len( t )
   NEXT
   ? "kept", c
   o:text := "x"
   ? "no argument", ValType( o:_text() ), o:text
   o := TCounter():New()
   a := {}
   FOR k := 1 TO 4
      AAdd( a, ThreadStart( {| x | Bump( x, 5000 ) }, o ) )
   NEXT
   AEval( a, {| x | x:Join() } )
   ? "threads", o:n, o:Total()
   ? "chain", TCounter():New():Add( 2 ):Add( 3 ):Total()
   a := { TCounter():New(), THolder():New(), TCounter():New() }
   FOR i := 1 TO 3000
      s += a[ i % 3 + 1 ]:Size()
   NEXT
   ? "alternating", s
   o := TChild():New()
   c := ""
   FOR i := 1 TO 6
      t := iif( i % 2 == 0, o:TBase2, o )
      c += LTrim( Str( Get( t ) ) ) + "/" + Name( t ) + " "
      Put( t, i )
   NEXT
   ? "casts", c, o:x, o:TBase2:x
   ? "torn", Torn()
   t := Awaited( TAsync():New() )
   t:Wait()
   ? "await", t:Result
   o := THolder():New()
   o:text := "t"
   ? "arguments", o:text( 1, 2 ), o:_text( "a", "b" ), o:text, ValType( o:_text() ), o:text
   ErrorBlock( {| e | Break( e ) } )
   BEGIN SEQUENCE
      o := TCounter():New()
      o:Fail( 3 )
   RECOVER USING t
      ? "error", t:Description, t:Operation, ProcName( 0 )
   END SEQUENCE
RETURN

STATIC FUNCTION Bump( o, n )
   LOCAL i
   FOR i := 1 TO n
      o:Add( 1 )
   NEXT
RETURN NIL

CLASS THolder
   VAR text INIT ""
   VAR list
   VAR kept INIT {}
   METHOD Keep( c ) INLINE AAdd( ::kept, c ), Self
   METHOD Size() INLINE 2
ENDCLASS

CLASS TCounter
   VAR n INIT 0
   VAR lock INIT Mutex()
   METHOD Add( k )
   METHOD Total() INLINE ::n
   METHOD Size() INLINE 1
   METHOD Fail( k ) INLINE iif( k == 0, ::n + "x", ::Fail( k - 1 ) )
ENDCLASS

METHOD Add( k ) CLASS TCounter
   ::lock:Lock()
   ::n := ::n + k
   ::lock:Unlock()
RETURN Self

// One send site for an object and its cast: each reads and writes the variable of the class it sees.
STATIC FUNCTION Get( o )
RETURN o:x

STATIC FUNCTION Put( o, v )
   o:x := o:x + v
RETURN NIL

STATIC FUNCTION Name( o )
RETURN o:Who()

// A thread writes a string or a number into a shared object's variable while this one reads it: each read is one
// of the two, whole.
STATIC FUNCTION Torn()
   LOCAL o := THolder():New(), t, i, v, nBad := 0
   o:text := "abc"
   t := ThreadStart( {| x | Flip( x, 200000 ) }, o ) // not captured: the reads below are resolved for its class
   FOR i := 1 TO 200000
      v := o:text
      IF ! ( ( ValType( v ) == "C" .AND. v == "abc" ) .OR. ( ValType( v ) == "N" .AND. v == 12345 ) )
         nBad++
      ENDIF
   NEXT
   t:Join()
RETURN nBad

STATIC FUNCTION Flip( o, n )
   LOCAL i
   FOR i := 1 TO n
      o:text := iif( i % 2 == 0, "abc", 12345 )
   NEXT
RETURN NIL

ASYNC FUNCTION Awaited( o )
   LOCAL k, s := 0
   FOR k := 1 TO 5
      s += AWAIT o:Twice( k )
   NEXT
RETURN s

CLASS TBase2
   VAR x INIT 100
   METHOD Who() INLINE "base"
ENDCLASS

CLASS TChild INHERIT TBase2
   VAR x INIT 1
   METHOD Who() INLINE "child"
ENDCLASS

CLASS TAsync
   ASYNC METHOD Twice( k )
ENDCLASS

ASYNC METHOD Twice( k ) CLASS TAsync
RETURN k * 2

// The barrier test with an object whose class the compiler does not know (an array's element): its sends go through
// their inline caches, not resolved when compiling.
STATIC FUNCTION Unknown( aBox )
   LOCAL i, k, t, nBad := 0
   FOR k := 1 TO 3
      __GcCollect()
   NEXT
   FOR i := 1 TO 5000
      IF i % 100 == 0
         __GcCollect( .T. )
         __GcCollect( .T. )
      ENDIF
      FillBox( aBox, i )
      IF i % 100 == 0
         __GcCollect( .T. )
         FOR k := 1 TO 200
            t := { Replicate( "z", 40 ), k }
         NEXT
         IF aBox[ 1 ]:list[ 1 ] != i .OR. aBox[ 1 ]:list[ 3 ][ 1 ] != i * 5 .OR. ;
               ! aBox[ 1 ]:list[ 2 ] == Str( i ) .OR. ;
               ! Right( aBox[ 1 ]:text, Len( LTrim( Str( i ) ) ) ) == LTrim( Str( i ) )
            nBad++
         ENDIF
      ENDIF
   NEXT
RETURN nBad

// The barrier test with an object whose class the compiler knows (made here, in a function without BEGIN SEQUENCE,
// whose variables are values): its sends are resolved when compiling (object.get, object.set), in Fill() too (a copy
// typed for the class).
STATIC FUNCTION Known()
   LOCAL o := THolder():New(), i, k, t, nBad := 0
   FOR k := 1 TO 3
      __GcCollect()
   NEXT
   FOR i := 1 TO 5000
      IF i % 100 == 0
         __GcCollect( .T. )
         __GcCollect( .T. )
      ENDIF
      Fill( o, i ) // its frame gone, nothing but the holder refers to the new values
      IF i % 100 == 0
         __GcCollect( .T. )
         FOR k := 1 TO 200
            t := { Replicate( "z", 40 ), k }
         NEXT
         IF o:list[ 1 ] != i .OR. o:list[ 3 ][ 1 ] != i * 5 .OR. ! o:list[ 2 ] == Str( i ) .OR. ;
               ! Right( o:text, Len( LTrim( Str( i ) ) ) ) == LTrim( Str( i ) )
            nBad++
         ENDIF
      ENDIF
   NEXT
RETURN nBad

STATIC FUNCTION Fill( o, i )
   o:text := Replicate( "k", 1 + i % 30 ) + LTrim( Str( i ) )
   o:list := { i, Str( i ), { i * 5 } }
RETURN NIL

STATIC FUNCTION FillBox( aBox, i )
   aBox[ 1 ]:text := Replicate( "q", 1 + i % 30 ) + LTrim( Str( i ) )
   aBox[ 1 ]:list := { i, Str( i ), { i * 5 } }
RETURN NIL
