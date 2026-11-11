// Memo fields (M, DBT files): dbCreate makes the .dbt beside the table; a memo is read whole (up to its first 0x1A),
// written as a new memo after the last (the file grows; an empty memo is none), its value "M" for ValType() and
// Type() while it is the field's own string (a copy keeps it, a string made from it is "C"); PACK keeps the memos of
// the records it keeps and drops the others (the file shrinks), ZAP empties it; a table reopened goes on after its last
// memo. A missing .dbt is DBFNTX/1002 Open error with its name; a value that is not a string is a Data type error;
// READONLY is Write not allowed. Where the reference differs (docs/rdd.md): a copy in a variable stays "M" here, PACK
// ends each memo with two 0x1A (one byte more), and the type error names the field. The files start with "rdd_memo",
// deleted at the end.
FIELD NAME, NOTE

PROCEDURE Main()
   LOCAL a, x
   ErrorBlock( {| e | QOut( "ERR", e:subSystem, e:genCode, e:subCode, e:description, "[" + e:filename + "]", ;
      e:osCode, e:canDefault, e:canRetry ), Break( e ) } )
   dbCreate( "rdd_memo", { { "NAME", "C", 4, 0 }, { "NOTE", "M", 10, 0 } } )
   ? "files:", File( "rdd_memo.dbf" ), File( "rdd_memo.dbt" ), Size()
   USE rdd_memo NEW
   a := dbStruct()
   ? "struct:", a[ 2 ][ 1 ], a[ 2 ][ 2 ], a[ 2 ][ 3 ], a[ 2 ][ 4 ], Header(), RecSize()
   APPEND BLANK
   ? "blank:", "[" + NOTE + "]", ValType( NOTE ), Type( "NOTE" ), Len( NOTE )
   REPLACE NAME WITH "a", NOTE WITH "hello"
   APPEND BLANK
   REPLACE NAME WITH "b", NOTE WITH Replicate( "x", 600 )
   APPEND BLANK
   REPLACE NAME WITH "c", NOTE WITH "a" + Chr( 26 ) + "b"
   APPEND BLANK
   REPLACE NAME WITH "d"
   ? "written:", Size()
   GO TOP
   DO WHILE ! Eof()
      ? RecNo(), NAME, Len( NOTE ), Left( NOTE, 5 )
      SKIP
   ENDDO
   GO 1
   REPLACE NOTE WITH "hi"
   ? "rewritten:", NOTE, Size()
   REPLACE NOTE WITH ""
   ? "emptied:", "[" + NOTE + "]", Size()
   REPLACE NOTE WITH "first"
   x := NOTE
   ? "types:", ValType( x ), ValType( FieldGet( 2 ) ), ValType( x + "" ), ValType( Upper( x ) ), Type( "x" )
   GO 2
   DELETE
   PACK
   ? "packed:", LastRec(), Size()
   GO TOP
   DO WHILE ! Eof()
      ? RecNo(), NAME, Len( NOTE ), Left( NOTE, 5 )
      SKIP
   ENDDO
   CLOSE
   USE rdd_memo NEW
   APPEND BLANK
   REPLACE NAME WITH "e", NOTE WITH "after reopening"
   GO BOTTOM
   ? "reopened:", NOTE, Size()
   BEGIN SEQUENCE
      REPLACE NOTE WITH 12
   END SEQUENCE
   ZAP
   ? "zapped:", LastRec(), Size()
   CLOSE
   USE rdd_memo NEW READONLY
   BEGIN SEQUENCE
      APPEND BLANK
   END SEQUENCE
   CLOSE
   FErase( "rdd_memo.dbt" )
   BEGIN SEQUENCE
      USE rdd_memo NEW
   END SEQUENCE
   ? "no dbt:", Used()
   FErase( "rdd_memo.dbf" )
RETURN

STATIC FUNCTION Size()
   LOCAL a := Directory( "rdd_memo.dbt" )
RETURN IIf( Empty( a ), -1, a[ 1 ][ 2 ] )
