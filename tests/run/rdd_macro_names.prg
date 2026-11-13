// A macro after an alias names the field or memory variable when the code runs: REPLACE &cName WITH (also with more
// fields), FIELD->&cName, _FIELD->&cName, alias->&cName, FIELD->alias->&cName, &cAlias->&cName, FIELD->&( expr ) read
// and assigned, M->&cName and MEMVAR->&cName, and the error of a name no field or variable has. The table starts with
// "rdd_macro_names" and is deleted at the end.
FIELD QTY, NAME
PROCEDURE Main()
   LOCAL cName := "QTY", cAlias := "T1"
   PRIVATE pv := 5, cVar := "PV"
   dbCreate( "rdd_macro_names", { { "QTY", "N", 5, 0 }, { "NAME", "C", 6, 0 } } )
   USE rdd_macro_names ALIAS T1 EXCLUSIVE
   APPEND BLANK
   REPLACE &cName WITH 7
   ? QTY, FIELD->&cName, T1->&cName, _FIELD->&cName
   REPLACE &cName WITH 8, NAME WITH "x"
   ? QTY, NAME
   T1->&cName := 9
   FIELD->&( "NA" + "ME" ) := "yz"
   ? QTY, NAME, FIELD->T1->&cName, &cAlias->&cName
   M->&cVar := 6
   ? pv, M->&cVar, MEMVAR->&cVar
   cName := "NOPE"
   ErrorBlock( {| e | QOut( "error", e:subCode, e:description, e:operation ), Break( e ) } )
   BEGIN SEQUENCE
      ? FIELD->&cName
   END SEQUENCE
   CLOSE
   FErase( "rdd_macro_names.dbf" )
RETURN
