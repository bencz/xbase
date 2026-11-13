// FieldLen(), FieldDec() and FieldType() of each field type, by number and by name, of positions outside the
// table and with no table open. The table starts with "rdd_field_info" and is deleted at the end.
PROCEDURE Main()
   LOCAL n
   ? FieldLen( 1 ), FieldDec( 1 ), "[" + FieldType( 1 ) + "]", ValType( FieldType( 1 ) )
   dbCreate( "rdd_field_info", { { "A", "C", 7, 0 }, { "B", "N", 9, 3 }, { "C", "D", 8, 0 }, { "D", "L", 1, 0 }, ;
      { "E", "M", 10, 0 } } )
   USE rdd_field_info
   FOR EACH n IN { 0, 1, 2, 3, 4, 5, 6, -1, 2.9, "B", NIL }
      ? FieldLen( n ), FieldDec( n ), "[" + FieldType( n ) + "]"
   NEXT
   CLOSE
   FErase( "rdd_field_info.dbf" )
   FErase( "rdd_field_info.dbt" )
RETURN
