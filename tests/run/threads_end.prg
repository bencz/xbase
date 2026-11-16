// The program ends when its start procedure returns, though threads still run: one that loops forever, one waiting
// for it in Join(), one waiting for a Mutex the start procedure holds, one on a Condition (inside SYNCHRONIZED), one on
// a Semaphore with no place free, one that started threads of its own; each is asked to QUIT and waited for, then the
// EXIT procedures run: a thread one starts runs (and is joined), another that still runs when they end is asked to
// QUIT too.
PROCEDURE Main()
   LOCAL oM := Mutex(), tLoop
   oM:Lock()
   tLoop := ThreadStart( {|| Forever() } )
   ThreadStart( {|| tLoop:Join() } )
   ThreadStart( {|| oM:Lock(), "never" } )
   ThreadStart( {|| WaitForever( Mutex(), Condition() ) } )
   ThreadStart( {|| Semaphore( 0 ):Wait() } )
   ThreadStart( {|| ThreadStart( {|| Forever() } ), Forever() } )
   ? "start procedure ends", tLoop:IsAlive()
RETURN

EXIT PROCEDURE Bye()
   ? "bye", ThreadStart( {| x | x * 2 }, 21 ):Join()
   ThreadStart( {|| Forever() } )
RETURN

STATIC FUNCTION WaitForever( oM, oC )
   SYNCHRONIZED oM
      oC:Wait( oM )
   END
RETURN NIL

STATIC FUNCTION Forever()
   LOCAL n := 0
   DO WHILE .T.
      n++
   ENDDO
RETURN n
