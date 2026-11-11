// Aliases: a name (a letter or _, then letters, digits and _; trailing blanks dropped), else DBCMD/1010 "Illegal
// characters in alias" after the table opened (closed again; an Open error comes first); the letters A to K and M, or
// an alias another area has, are DBCMD/1011 "Alias already in use", also for dbCreate( ..., lNewArea, cAlias ), which
// then leaves the area current before current. Select() reads names as SELECT does (letters A to K, trailing blanks
// ignored, leading ones not). The tables start with "rdd_alias", deleted at the end.
PROCEDURE Main()
   ErrorBlock( {| e | QOut( "ERR", e:subSystem, e:genCode, e:subCode, e:description, "[" + e:operation + "]", ;
      e:canDefault, e:canRetry ), Break( e ) } )
   dbCreate( "rdd_alias", { { "NAME", "C", 4, 0 } } )
   Try( {|| dbUseArea( .T., , "rdd_alias", "B" ) }, "B in 1" ) ; ? Select(), Alias() ; CLOSE ALL
   SELECT 2
   Try( {|| dbUseArea( .F., , "rdd_alias", "B" ) }, "B in 2" ) ; ? Select(), Alias(), Select( "B" ) ; CLOSE ALL
   SELECT 1
   Try( {|| dbUseArea( .F., , "rdd_alias", "M" ) }, "M" ) ; ? Select(), Alias() ; CLOSE ALL
   Try( {|| dbUseArea( .T., , "rdd_alias", "L" ) }, "L" ) ; ? Select(), Alias(), Select( "L" ) ; CLOSE ALL
   Try( {|| dbUseArea( .T., , "rdd_alias", "2024" ) }, "2024" ) ; ? Alias(), Select() ; CLOSE ALL
   Try( {|| dbUseArea( .T., , "rdd_alias", "1X" ) }, "1X" ) ; ? Alias(), Select() ; CLOSE ALL
   Try( {|| dbUseArea( .T., , "rdd_alias", "A B" ) }, "A B" ) ; ? Alias(), Select() ; CLOSE ALL
   Try( {|| dbUseArea( .T., , "rdd_alias", "_X9" ) }, "_X9" ) ; ? Alias(), Select() ; CLOSE ALL
   Try( {|| dbUseArea( .T., , "rdd_alias", "X-Y" ) }, "X-Y" ) ; ? Alias(), Select() ; CLOSE ALL
   Try( {|| dbUseArea( .T., , "rdd_alias", "ra  " ) }, "blanks" ) ; ? "[" + Alias() + "]", Select(), Select( "RA" )
   ? "select", Select( "A" ), Select( "B" ), Select( "K" ), Select( "L" ), Select( "ra " ), Select( " ra" )
   Try( {|| dbSelectArea( "RA  " ) }, "selectarea blanks" ) ; ? Select()
   CLOSE ALL
   Try( {|| dbUseArea( .T., , "rdd_alias_none", "1X" ) }, "missing file" ) ; ? Alias(), Select(), Used()
   CLOSE ALL
   USE rdd_alias ALIAS R1A NEW
   Try( {|| dbCreate( "rdd_alias_c", { { "X", "C", 1, 0 } }, , .T., "R1A" ) }, "dbcreate dup" )
   ? Select(), Alias(), File( "rdd_alias_c.dbf" )
   Try( {|| dbUseArea( .T., , "rdd_alias_c", "R1A" ) }, "use dup" ) ; ? Select(), Alias(), Used()
   CLOSE ALL
   FErase( "rdd_alias.dbf" )
   FErase( "rdd_alias_c.dbf" )
RETURN

STATIC PROCEDURE Try( b, c )
   ?? c, ""
   BEGIN SEQUENCE
      Eval( b )
      ?? "no error"
   END SEQUENCE
RETURN
