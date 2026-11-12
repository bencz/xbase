// The database commands: USE (NEW, ALIAS), APPEND BLANK, REPLACE, assignments to alias->field and FIELD->field
// (also with +=), alias->( expr ) and (n)->field, GO TOP/BOTTOM, GOTO, SKIP, COUNT, SUM, AVERAGE, LOCATE, DELETE and
// RECALL with a scope, SET DELETED, REPLACE ALL, SET FILTER, SELECT, PACK, CLOSE and ZAP. The table is
// "rdd_commands.dbf", deleted at the end.
FIELD NAME, AGE, BORN, OK

PROCEDURE Main()
   LOCAL a, n
   dbCreate( "rdd_commands", { { "NAME", "C", 10, 0 }, { "AGE", "N", 5, 1 }, { "BORN", "D", 8, 0 }, ;
      { "OK", "L", 1, 0 } } )
   USE rdd_commands ALIAS T1 NEW
   ? "empty:", RecNo(), LastRec(), Bof(), Eof(), Alias(), Select(), Used(), FCount(), Header(), RecSize()
   APPEND BLANK
   ? "append:", RecNo(), LastRec(), Bof(), Eof(), "[" + NAME + "]", AGE, BORN, OK
   REPLACE NAME WITH "Ann", AGE WITH 12.34, BORN WITH SToD( "20240229" ), OK WITH .T.
   APPEND BLANK
   T1->NAME := "Bob"
   FIELD->AGE := 99.9
   T1->AGE += 0.1
   ? "fields:", T1->NAME, AGE, FieldGet( 1 ), FieldPos( "age" ), FieldName( 2 ), ValType( BORN ), ;
      (1)->NAME, ( "T1" )->AGE
   APPEND BLANK
   REPLACE NAME WITH "Cid", AGE WITH 3
   GO TOP
   ? "top:", RecNo(), Bof(), Eof(), NAME, AGE
   SKIP -1
   ? "skip -1:", RecNo(), Bof(), Eof(), NAME
   GO BOTTOM
   SKIP
   ? "skip past:", RecNo(), Bof(), Eof(), "[" + NAME + "]", AGE
   GOTO 2
   ? "goto 2:", RecNo(), NAME
   COUNT TO n
   ? "count:", n, RecNo(), Eof()
   SUM AGE TO n FOR AGE > 5
   ? "sum:", n
   AVERAGE AGE, AGE * 2 TO n, a
   ? "average:", n, a, Eof()
   GOTO 2
   AVERAGE AGE TO n NEXT 2
   ? "average next:", n, RecNo()
   AVERAGE AGE TO n FOR AGE > 1000
   ? "average of none:", n, Type( "M->__Avg" )
   LOCATE FOR NAME = "Cid"
   ? "locate:", Found(), RecNo()
   DELETE ALL FOR AGE < 10
   GO 3
   ? "deleted 3:", Deleted()
   SET DELETED ON
   COUNT TO n
   ? "count deleted on:", n
   SET DELETED OFF
   RECALL ALL
   COUNT TO n FOR Deleted()
   ? "after recall:", n
   REPLACE ALL AGE WITH AGE * 2
   GO TOP
   ? "replace all:", AGE
   SET FILTER TO AGE > 100
   GO TOP
   ? "filter:", RecNo(), NAME, dbFilter()
   SET FILTER TO
   SELECT 2
   ? "area 2:", Select(), Used(), T1->NAME
   ? "areaeval:", T1->( RecNo() ), T1->( LastRec() ), Select()
   SELECT T1
   GO 1
   DELETE
   PACK
   ? "pack:", LastRec(), RecNo(), NAME
   a := dbStruct()
   ? "struct:", Len( a ), a[ 1 ][ 1 ], a[ 4 ][ 2 ]
   CLOSE
   ? "closed:", Used(), Alias(), Select()
   USE rdd_commands ALIAS other NEW
   ? "alias:", Alias(), OTHER->NAME, LastRec()
   ZAP
   ? "zap:", LastRec(), Eof()
   CLOSE ALL
   FErase( "rdd_commands.dbf" )
RETURN
