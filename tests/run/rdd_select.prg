// Selecting work areas: SELECT by number, letter and alias, dbSelectArea with a string of digits or a letter, SELECT 0
// (the lowest free area), Select( cAlias ), and the names dbSelectArea reads as areas: the letters A to K (areas 1 to
// 11), digits after blanks (what follows them ignored). Other names are aliases; the letter M is one no area has, an
// error (the reference selects an area past the last one; docs/rdd.md). The table is "rdd_select.dbf", deleted at the
// end.
FIELD NAME, N

PROCEDURE Main()
   LOCAL c
   dbCreate( "rdd_select", { { "NAME", "C", 6, 0 }, { "N", "N", 3, 0 } } )
   USE rdd_select ALIAS T5 NEW
   APPEND BLANK
   REPLACE T5->NAME WITH "abc", N WITH 7
   ? T5->NAME, N
   SELECT 3
   ? "select 3:", Select()
   SELECT B
   ? "select B:", Select()
   SELECT t5
   ? "select t5:", Select()
   dbSelectArea( "4" )
   ? "dbselectarea '4':", Select()
   dbSelectArea( "C" )
   ? "dbselectarea 'C':", Select()
   SELECT 0
   ? "select 0:", Select()
   ? "select('t5'):", Select( "t5" ), Select( "nope" )
   ErrorBlock( {| e | Break( e ) } )
   FOR EACH c IN { "J", "K", "L", "M", "N", "Z", "AB", " 5", "5x", "05" }
      BEGIN SEQUENCE
         dbSelectArea( c )
         ? c, Select()
      RECOVER
         ? c, "error"
      END SEQUENCE
   NEXT
   CLOSE ALL
   FErase( "rdd_select.dbf" )
RETURN
