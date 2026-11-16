// A generator's own variables (its state, frame and resume function) answer no message, whatever text names them:
// ObjSend() reads and assigns none of them (the class has no such member: error 1004 to read, 1005 to assign), a
// macro message cannot name them (1449), and ObjHasVar() does not see them; its Current still answers, and stepping
// it afterwards is unchanged.
PROCEDURE Main()
   LOCAL g := Numbers( 3 ), e, c, b
   ? ObjHasVar( g, "(STATE)" ), ObjHasVar( g, "(FRAME)" ), ObjHasVar( g, "(RESUME)" ), ObjHasVar( g, "CURRENT" )
   ErrorBlock( {| o | Break( o ) } )
   FOR EACH b IN { {|| ObjSend( g, "(FRAME)" ) }, {|| ObjSend( g, "(STATE)" ) }, {|| ObjSend( g, "_(STATE)", 7 ) }, ;
                   {|| ObjSend( g, "_(RESUME)", 1 ) }, {|| ObjSend( g, "_(FRAME)", {} ) }, ;
                   {|| c := "(STATE)", g:&c }, {|| ObjSend( g, "CURRENT" ) } }
      BEGIN SEQUENCE
         ? "value:", Eval( b )
      RECOVER USING e
         ? "error", e:subCode, e:description
      END SEQUENCE
   NEXT
   FOR EACH c IN g
      ?? "", c
   NEXT
RETURN

STATIC FUNCTION Numbers( nLast )
   LOCAL n
   FOR n := 1 TO nLast
      YIELD n
   NEXT
RETURN
