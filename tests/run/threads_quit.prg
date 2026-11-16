// QUIT in a thread ends the program, with the exit code ErrorLevel() set: the start procedure, waiting in Join(),
// goes no further, a thread that loops forever and one waiting for it end too, and the EXIT procedures run: they find
// those threads ended (Join() gives NIL), and may start threads again.
STATIC tLoop, tWait

PROCEDURE Main()
   LOCAL oM := Mutex(), tQuit
   oM:Lock()
   tLoop := ThreadStart( {|| Forever() } )
   tWait := ThreadStart( {|| tLoop:Join() } )
   tQuit := ThreadStart( {|| oM:Lock(), Quits() } )
   ? "joining"
   oM:Unlock()
   tQuit:Join()
   ? "not reached"
RETURN

EXIT PROCEDURE Bye()
   ? "bye", ErrorLevel(), tLoop:IsAlive(), tLoop:Join(), tWait:IsAlive(), tWait:Join()
   ? "again", ThreadStart( {| x | x * 2 }, 21 ):Join()
RETURN

STATIC FUNCTION Forever()
   LOCAL n := 0
   DO WHILE .T.
      n++
   ENDDO
RETURN n

STATIC FUNCTION Quits()
   ErrorLevel( 3 )
   QUIT
RETURN NIL
