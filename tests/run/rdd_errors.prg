// The errors of the work areas: FieldPut of a number too wide (stars written), of a value of the wrong type, of NIL
// (nothing written); dbUseArea of a missing file (an Open error that can be retried, the area left free); every
// function on an area with no table, those that only read giving empty values and the others "Workarea not in use";
// dbSelectArea of an unknown alias; a field past the last. The table is "rdd_errors.dbf", deleted at the end.
PROCEDURE Main()
   dbCreate( "rdd_errors", { { "NAME", "C", 4, 0 }, { "AGE", "N", 3, 0 }, { "D", "D", 8, 0 }, { "L", "L", 1, 0 } } )
   dbUseArea( .T., , "rdd_errors" )
   dbAppend()
   ErrorBlock( {| e | Show( e ), .F. } )
   ? FieldPut( 2, 12345 )
   ? FieldPut( 1, "toolongvalue" )
   ? FieldPut( 2, "x" )
   ? FieldPut( 3, 1 )
   ? FieldPut( 4, "T" )
   ? FieldPut( 1, NIL )
   ? "after:", "[" + FieldGet( 1 ) + "]", FieldGet( 2 ), FieldGet( 3 ), FieldGet( 4 )
   ErrorBlock( {| e | Show( e ), Break( e ) } )
   Try( {|| dbUseArea( .T., , "rdd_errors_none" ) } )
   ? "after use:", Used(), "[" + Alias() + "]", Select(), NetErr()
   dbSelectArea( 5 )
   ? "empty area:", FieldGet( 1 ), FieldPos( "x" ), RecNo(), LastRec(), Eof(), Bof(), "[" + Alias() + "]", FCount(), ;
      Deleted(), Found(), Header(), RecSize(), dbFilter()
   Try( {|| RddName() } )
   Try( {|| dbGoTop() } )
   Try( {|| dbGoBottom() } )
   Try( {|| dbGoTo( 1 ) } )
   Try( {|| dbSkip() } )
   Try( {|| dbAppend() } )
   Try( {|| dbDelete() } )
   Try( {|| dbRecall() } )
   Try( {|| FieldPut( 1, "x" ) } )
   Try( {|| dbCommit() } )
   Try( {|| __dbPack() } )
   Try( {|| __dbZap() } )
   Try( {|| dbEval( {|| NIL } ) } )
   Try( {|| dbSelectArea( "NOPE" ) } )
   dbSelectArea( 1 )
   ? "field 9:", FieldGet( 9 ), FieldPut( 9, "x" ), FieldName( 9 )
   dbCloseAll()
   FErase( "rdd_errors.dbf" )
RETURN

STATIC PROCEDURE Try( b )
   BEGIN SEQUENCE
      Eval( b )
      ? "no error"
   END SEQUENCE
RETURN

STATIC FUNCTION Show( e )
   ? "ERR", e:subSystem, e:genCode, e:subCode, "[" + e:description + "]", "[" + e:operation + "]", ;
      "[" + e:filename + "]", e:osCode, e:canDefault, e:canRetry, e:canSubstitute, ;
      IIF( ValType( e:args ) == "A", Len( e:args ), -1 )
RETURN .F.
