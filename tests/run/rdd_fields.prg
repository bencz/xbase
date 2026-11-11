// Fields and memory variables: an undeclared name reads the field of the current area first, then the memory
// variable; an assignment to it writes the memory variable; M-> and FIELD-> choose; macros read and assign alias->name
// and Type() sees fields. FIELD ... IN names the area of declared fields, read from another area. The errors of an
// unknown field or alias (also in alias->( expr ), which keeps the current area), of FIELD-> with no table, and of an
// assignment of NIL (a wrong type; FieldPut() ignores NIL). The tables start with "rdd_fields_" and are deleted at the
// end.
MEMVAR AGE
FIELD CODE, QTY IN T6

PROCEDURE Main()
   PRIVATE NAME := "memvar", cName := "NAME"
   dbCreate( "rdd_fields_a", { { "NAME", "C", 10, 0 }, { "AGE", "N", 3, 0 } } )
   USE rdd_fields_a ALIAS T2 NEW
   APPEND BLANK
   REPLACE NAME WITH "field"
   ? "read:", NAME, M->NAME, FIELD->NAME, &cName, &( "T2->NAME" )
   NAME := "assigned"
   ? "after assign:", NAME, M->NAME, FIELD->NAME
   ? "type:", Type( "NAME" ), Type( "AGE" ), Type( "T2->AGE" ), Type( "NOPE" ), Type( "T2->NOPE" )
   &( "T2->NAME" ) := "by macro"
   ? "macro assign:", T2->NAME
   CLOSE
   ? "after close:", NAME
   Declared()
   CLOSE ALL
   FErase( "rdd_fields_a.dbf" )
   FErase( "rdd_fields_b.dbf" )
RETURN

STATIC PROCEDURE Declared()
   dbCreate( "rdd_fields_b", { { "CODE", "C", 4, 0 }, { "QTY", "N", 3, 0 } } )
   USE rdd_fields_b ALIAS T6 NEW
   APPEND BLANK
   CODE := "abcd"
   QTY := 42
   SELECT 2
   ? "declared:", CODE, QTY, T6->CODE
   ErrorBlock( {| e | Show( e ), Break( e ) } )
   Try( {|| T6->NOPE } )
   Try( {|| XX->CODE } )
   Try( {|| FIELD->CODE } )
   Try( {|| XX->( RecNo() ) } )
   Try( {|| (3)->CODE } )
   Try( {|| T6->NOPE := 1 } )
   ? "areaeval keeps area:", Select()
   SELECT T6
   Try( {|| T6->CODE := NIL } )
   ? "after nil:", T6->CODE
RETURN

STATIC PROCEDURE Try( b )
   BEGIN SEQUENCE
      Eval( b )
      ? "no error"
   END SEQUENCE
RETURN

STATIC FUNCTION Show( e )
   ? "ERR", e:subSystem, e:genCode, e:subCode, "[" + e:description + "]", "[" + e:operation + "]", ;
      e:canDefault, e:canRetry, e:canSubstitute, IIF( ValType( e:args ) == "A", Len( e:args ), -1 )
RETURN .F.
