// Fields named with an alias in a codeblock that Eval runs where it is made: read and written in the area the alias
// names, not the current one. The tables start with "eval_inl", deleted at the end.
PROCEDURE Main()
   dbCreate( "eval_inla", { { "NAME", "C", 10, 0 } } )
   dbCreate( "eval_inlb", { { "NAME", "C", 10, 0 } } )
   USE eval_inla ALIAS CA NEW
   dbAppend()
   CA->NAME := "from A"
   USE eval_inlb ALIAS CB NEW
   dbAppend()
   CB->NAME := "from B"
   SELECT CB
   ? Eval( {|| CA->NAME } ), Eval( {| c | ( c )->NAME }, "CA" )
   Eval( {|| CA->NAME := "set" } )
   ? CA->NAME, CB->NAME, Alias()
   CLOSE ALL
   FErase( "eval_inla.dbf" )
   FErase( "eval_inlb.dbf" )
RETURN
