// Scopes and Found(): dbEval with NEXT (from the current record, ending on the last record it took; none for 0 or
// below), RECORD, WHILE (ending on the first record that fails) and REST; Found() of a LOCATE stays until the record
// pointer moves (SKIP, GO TOP, GOTO, APPEND BLANK), not when a field is read. The table is "rdd_scope.dbf", deleted at
// the end.
FIELD NAME

PROCEDURE Main()
   LOCAL n, b := {|| n++ }
   dbCreate( "rdd_scope", { { "NAME", "C", 4, 0 } } )
   USE rdd_scope NEW
   APPEND BLANK
   APPEND BLANK
   APPEND BLANK
   GO 2
   REPLACE NAME WITH "bb"
   LOCATE FOR NAME = "bb"
   ? "found", Found()
   SKIP
   ? "after skip", Found()
   LOCATE FOR NAME = "bb"
   GO TOP
   ? "after go top", Found()
   LOCATE FOR NAME = "bb"
   GO 1
   ? "after goto", Found()
   LOCATE FOR NAME = "bb"
   APPEND BLANK
   ? "after append", Found(), LastRec()
   LOCATE FOR NAME = "bb"
   ? FieldGet( 1 )
   ? "after fieldget", Found()
   n := 0 ; GO 1 ; dbEval( b, , , 1 ) ; ? "next 1", n, RecNo()
   n := 0 ; GO 1 ; dbEval( b, , , 0 ) ; ? "next 0", n, RecNo()
   n := 0 ; GO 2 ; dbEval( b, , , -1 ) ; ? "next -1", n, RecNo()
   n := 0 ; GO 2 ; dbEval( b, , , 2 ) ; ? "next 2", n, RecNo(), Eof()
   n := 0 ; GO 3 ; dbEval( b, , , 5 ) ; ? "next 5", n, RecNo(), Eof()
   n := 0 ; GO 2 ; dbEval( b, , , , 3 ) ; ? "record 3", n, RecNo()
   n := 0 ; GO 2 ; dbEval( b, {|| .T. }, {|| RecNo() < 3 } ) ; ? "while", n, RecNo()
   n := 0 ; GO 2 ; dbEval( b, , , , , .T. ) ; ? "rest", n, RecNo(), Eof()
   n := 0 ; GO 1 ; REPLACE NEXT 2 NAME WITH "nx" ; ? "replace next 2", RecNo()
   COUNT TO n FOR NAME = "nx" ; ? "count", n
   CLOSE
   FErase( "rdd_scope.dbf" )
RETURN
