// Threads and channels: creatures on threads of their own (ThreadStart) ask for meetings on one channel; a broker
// pairs them two by two and answers each on its own channel with the other's name and color, after which both change
// color; 30,000 meetings with 3 creatures, then 30,000 with 10. Checked: the color table, and per run 2N meetings
// counted, none of a creature with itself (the colors at the end depend on the threads' order).
#define BLUE 0
#define RED 1
#define YELLOW 2

PROCEDURE Main()
   LOCAL t := ElapsedMs(), cTable := "", a, b, ok
   FOR a := 0 TO 2
      FOR b := 0 TO 2
         cTable += Str( Complement( a, b ), 1 )
      NEXT
   NEXT
   ok := cTable == "021210102"
   ok := Run( 30000, { BLUE, RED, YELLOW } ) .AND. ok
   ok := Run( 30000, { BLUE, RED, YELLOW, RED, YELLOW, BLUE, RED, YELLOW, RED, BLUE } ) .AND. ok
   ? "chameneos_redux", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION Complement( a, b )
   RETURN IIf( a == b, a, 3 - a - b )

// Whether n meetings of creatures of these colors count 2n meetings, none with itself. The caller is the broker: a
// request is { name, color, reply channel }; the answer is the other's { name, color }, or NIL once no meeting is
// left.
STATIC FUNCTION Run( n, aColors )
   LOCAL oMeet := Channel(), aThreads := {}, nLeft := n, nMet := 0, nSame := 0, nDone := 0, aFirst, aReq, aResult, i
   FOR i := 1 TO Len( aColors )
      AAdd( aThreads, ThreadStart( {| nId, nColor | Live( oMeet, nId, nColor ) }, i, aColors[ i ] ) )
   NEXT
   DO WHILE nDone < Len( aColors )
      aReq := oMeet:ReceiveAsync():Result
      IF nLeft == 0
         aReq[ 3 ]:SendAsync( NIL ):Wait()
         nDone++
      ELSEIF aFirst == NIL
         aFirst := aReq
      ELSE
         aFirst[ 3 ]:SendAsync( { aReq[ 1 ], aReq[ 2 ] } ):Wait()
         aReq[ 3 ]:SendAsync( { aFirst[ 1 ], aFirst[ 2 ] } ):Wait()
         aFirst := NIL
         nLeft--
      ENDIF
   ENDDO
   FOR i := 1 TO Len( aThreads )
      aResult := aThreads[ i ]:Join()
      nMet += aResult[ 1 ]
      nSame += aResult[ 2 ]
   NEXT
   RETURN nMet == 2 * n .AND. nSame == 0

// A creature: asks for meetings until none is left; returns { meetings, meetings with itself }.
STATIC FUNCTION Live( oMeet, nId, nColor )
   LOCAL oReply := Channel(), nMet := 0, nSame := 0, aOther
   DO WHILE .T.
      oMeet:SendAsync( { nId, nColor, oReply } ):Wait()
      aOther := oReply:ReceiveAsync():Result
      IF aOther == NIL
         EXIT
      ENDIF
      nColor := Complement( nColor, aOther[ 2 ] )
      nMet++
      IF aOther[ 1 ] == nId
         nSame++
      ENDIF
   ENDDO
   RETURN { nMet, nSame }
