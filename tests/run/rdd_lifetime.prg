// A work area closed by the code it runs: dbEval, a FOR, a WHILE, a filter, LOCATE and CONTINUE blocks that close
// (or close and open again) their own area stop the command there; an error block that closes the area of a write it
// was raised for. Writes to a table opened READONLY are "Write not allowed" (DBFNTX/1025) and change nothing. A table
// whose field descriptors cannot be read (a date that is not 8 bytes, a field of no bytes) is an Open error. The
// files start with "rdd_lifetime_" and are deleted at the end.
PROCEDURE Main()
   LOCAL n := 0
   dbCreate( "rdd_lifetime", { { "NAME", "C", 4, 0 }, { "N", "N", 3, 0 } } )
   Fill()
   dbEval( {|| n++, dbCloseArea() } )
   ? "dbeval:", n, Used(), Select()
   Fill()
   dbEval( {|| n++ }, {|| dbCloseArea(), .T. } )
   ? "for:", Used()
   Fill()
   dbEval( {|| n++ }, , {|| dbCloseArea(), .T. } )
   ? "while:", Used()
   Fill()
   dbSetFilter( {|| dbCloseArea(), .T. } )
   dbGoTop()
   ? "filter:", Used(), Eof()
   Fill()
   dbSetFilter( {|| Reopen(), .T. } )
   dbSkip()
   ? "filter reopens:", Used(), RecNo()
   dbCloseArea()
   Fill()
   __dbLocate( {|| dbCloseArea(), .T. } )
   ? "locate:", Used(), Found()
   Fill()
   __dbLocate( {|| FieldGet( 2 ) == 1 } )
   ? "located:", Found(), RecNo()
   __dbLocate( {|| dbCloseArea(), .T. } )
   Fill()
   __dbLocate( {|| FieldGet( 2 ) > 0 } )
   ErrorBlock( {| e | QOut( "ERR", e:subSystem, e:subCode, e:description ), dbCloseArea(), .F. } )
   FieldPut( 2, 12345 )
   ? "error block closed:", Used()
   ErrorBlock( {| e | QOut( "ERR", e:subSystem, e:genCode, e:subCode, e:description, e:canDefault ), Break( e ) } )
   Fill()
   dbCloseArea()
   dbUseArea( .T., , "rdd_lifetime", , , .T. )
   Try( {|| FieldPut( 1, "x" ) } )
   Try( {|| dbDelete() } )
   Try( {|| dbRecall() } )
   Try( {|| dbAppend() } )
   Try( {|| __dbPack() } )
   Try( {|| __dbZap() } )
   Try( {|| dbCommit() } )
   ? "readonly:", LastRec(), "[" + FieldGet( 1 ) + "]", Deleted()
   dbCloseArea()
   Corrupt( "D", 3 )
   Try( {|| dbUseArea( .T., , "rdd_lifetime_bad" ) } )
   Corrupt( "L", 0 )
   Try( {|| dbUseArea( .T., , "rdd_lifetime_bad" ) } )
   ? "corrupt:", Used()
   FErase( "rdd_lifetime.dbf" )
   FErase( "rdd_lifetime_bad.dbf" )
RETURN

STATIC PROCEDURE Fill()
   dbUseArea( .T., , "rdd_lifetime" )
   __dbZap()
   dbAppend()
   FieldPut( 2, 1 )
   dbAppend()
   FieldPut( 2, 2 )
   dbGoTop()
RETURN

STATIC PROCEDURE Reopen()
   dbCloseArea()
   dbUseArea( .F., , "rdd_lifetime" )
RETURN

// A table of one field of type cType and nLen bytes, written byte by byte.
STATIC PROCEDURE Corrupt( cType, nLen )
   LOCAL h := FCreate( "rdd_lifetime_bad.dbf" )
   FWrite( h, Chr( 3 ) + Chr( 99 ) + Chr( 12 ) + Chr( 31 ) + Replicate( Chr( 0 ), 4 ) + Chr( 66 ) + Chr( 0 ) + ;
      Chr( 1 + nLen ) + Replicate( Chr( 0 ), 21 ) )
   FWrite( h, "X" + Replicate( Chr( 0 ), 10 ) + cType + Replicate( Chr( 0 ), 4 ) + Chr( nLen ) + ;
      Replicate( Chr( 0 ), 15 ) + Chr( 13 ) + Chr( 0 ) + Chr( 26 ) )
   FClose( h )
RETURN

STATIC PROCEDURE Try( b )
   BEGIN SEQUENCE
      Eval( b )
      ?? " no error"
   END SEQUENCE
RETURN
