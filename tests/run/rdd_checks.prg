// What xbase checks where the reference does not (docs/rdd.md): dbCreate of a structure with a type the driver does
// not write or with no fields (DBCMD/1014, nothing created), the same table opened twice under its alias ("Alias
// already in use", as for another table under an alias in use), and __dbLocate with no table ("Workarea not in use",
// 2001 as for every other function). The tables start with "rdd_checks", deleted at the end.
PROCEDURE Main()
   ErrorBlock( {| e | Show( e ), Break( e ) } )
   Try( {|| dbCreate( "rdd_checks", { { "X", "Q", 1, 0 } } ) } )
   Try( {|| dbCreate( "rdd_checks", {} ) } )
   ? "created:", File( "rdd_checks.dbf" )
   Try( {|| __dbLocate( {|| .T. } ) } )
   dbCreate( "rdd_checks", { { "X", "C", 1, 0 } } )
   USE rdd_checks NEW
   Try( {|| dbUseArea( .T., , "rdd_checks" ) } )
   ? "areas:", Select(), Used()
   dbCreate( "rdd_checks_b", { { "X", "C", 1, 0 } } )
   Try( {|| dbUseArea( .T., , "rdd_checks_b", "RDD_CHECKS" ) } )
   ? "areas:", Select(), Used()
   CLOSE ALL
   FErase( "rdd_checks.dbf" )
   FErase( "rdd_checks_b.dbf" )
RETURN

STATIC PROCEDURE Try( b )
   BEGIN SEQUENCE
      Eval( b )
      ? "no error"
   END SEQUENCE
RETURN

STATIC FUNCTION Show( e )
   ? "ERR", e:subSystem, e:genCode, e:subCode, "[" + e:description + "]", "[" + e:operation + "]", ;
      "[" + e:filename + "]", e:canDefault, e:canRetry
RETURN .F.
