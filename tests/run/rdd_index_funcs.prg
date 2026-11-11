// The order functions: names (ordName() upper case, ordBagName() as given, ordNumber() in any case), keys, conditions,
// UNIQUE and DESCENDING flags, ordSetFocus() (the name before), dbSetOrder(), ordListClear() (the record stays),
// ordListAdd() (the first one added becomes controlling and the first record current; a file open already is not
// added again); INDEX ON over an empty table; every record goes into an order, deleted or filtered; a SEEK of a value
// of another type than the key's finds nothing; a number that shows as zero is zero; a key past 256 bytes is cut; a key
// of a type no order can have, an index file missing, a SEEK with no order are errors. The files start with
// "rdd_index_funcs", deleted at the end.
FIELD NAME, N

PROCEDURE Main()
   LOCAL i
   ErrorBlock( {| e | QOut( "ERR", e:subSystem, e:genCode, e:subCode, e:description, "[" + e:operation + "]", ;
      "[" + e:filename + "]", e:canDefault, e:canRetry ), Break( e ) } )
   dbCreate( "rdd_index_funcs", { { "NAME", "C", 8, 0 }, { "N", "N", 8, 4 } } )
   USE rdd_index_funcs NEW
   BEGIN SEQUENCE
      SEEK "x"
   END SEQUENCE
   INDEX ON NAME TO rdd_index_funcs_e
   ? "empty:", IndexOrd(), RecNo(), Eof(), Bof(), LastRec()
   FOR i := 1 TO 5
      APPEND BLANK
      REPLACE NAME WITH Chr( 100 - i ), N WITH IIf( i == 3, -0.00001, i - 3 )
   NEXT
   GO 2
   DELETE
   SET DELETED ON
   SET FILTER TO NAME != "a"
   INDEX ON NAME TO rdd_index_funcs_all
   SET DELETED OFF
   SET FILTER TO
   Walk( "all:" )
   SEEK 12
   ? "seek number on C:", Found(), Eof(), RecNo()
   SEEK SToD( "20200101" )
   ? "seek date on C:", Found(), Eof(), RecNo()
   INDEX ON N TO rdd_index_funcs_n
   Walk( "n:" )
   SEEK "x"
   ? "seek string on N:", Found(), Eof()
   INDEX ON Replicate( NAME, 40 ) TO rdd_index_funcs_long
   ? "long key:", Len( Replicate( NAME, 40 ) ), IndexOrd()
   BEGIN SEQUENCE
      INDEX ON {} TO rdd_index_funcs_bad
   END SEQUENCE
   INDEX ON NAME TO rdd_index_funcs_name
   INDEX ON N TO Rdd_Index_Funcs_Num FOR N > 0 DESCENDING ADDITIVE
   ? IndexOrd(), ordName( 1 ), ordName( 2 ), ordName( 3 ), ordNumber( "RDD_INDEX_FUNCS_NUM" ), ;
      ordNumber( "rdd_index_funcs_num" ), ordNumber( "x" )
   ? ordBagName( 1 ), ordBagName( 2 ), ordBagName( "rdd_index_funcs_name" ), ordBagExt(), IndexExt()
   ? ordKey(), ordKey( 1 ), ordKey( "rdd_index_funcs_name" ), ordFor( 2 ), ordFor( 1 ), ordIsUnique( 1 ), ;
      ordDescend( 2 ), ordDescend( 1 )
   ? ordSetFocus(), ordSetFocus( 1 ), ordSetFocus(), ordSetFocus( "RDD_INDEX_FUNCS_NUM" ), IndexOrd(), RecNo()
   GO 1
   ? dbSetOrder( 0 ), IndexOrd(), RecNo()
   ordListClear()
   ? IndexOrd(), ordName( 1 ), IndexKey(), RecNo()
   GO 2
   ordListAdd( "Rdd_Index_Funcs_Num" )
   ? "listadd:", IndexOrd(), RecNo()
   ordListAdd( "rdd_index_funcs_name" )
   ? "listadd 2:", IndexOrd(), RecNo()
   dbSetIndex( "rdd_index_funcs_name" )
   ? "setindex again:", IndexOrd(), ordName( 3 ), RecNo()
   BEGIN SEQUENCE
      ordListAdd( "rdd_index_funcs_none" )
   END SEQUENCE
   CLOSE
   FErase( "rdd_index_funcs.dbf" )
   AEval( { "e", "all", "n", "long", "name", "none" }, {| c | FErase( "rdd_index_funcs_" + c + ".ntx" ) } )
   FErase( "Rdd_Index_Funcs_Num.ntx" )
RETURN

// The records in the controlling order: number and N.
STATIC PROCEDURE Walk( cTitle )
   ? cTitle
   GO TOP
   DO WHILE ! Eof()
      ?? " " + LTrim( Str( RecNo() ) ) + "=" + LTrim( Str( N, 10, 5 ) )
      SKIP
   ENDDO
RETURN
