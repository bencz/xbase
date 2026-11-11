// Aliases and macros: in compiled code a macro before -> is the alias (&cAlias->name, &cAlias.->name,
// &( expr )->name, &cAlias->( expr ), assignments); in a macro alias->( list ), ( expr )->name, ( expr )->( list ),
// ( n )->name and assignments to them. An alias expression is evaluated once, after the value of a plain assignment
// and before the field's value otherwise, as in compiled code (the reference evaluates it twice for += and ++); the
// work area comes back after an error inside alias->( expr ) (the reference leaves the inner one current). Type()
// checks the names inside alias->( expr ) against the alias's work area, and an evaluation that finds no variable,
// field or alias gives "U". The tables start with "rdd_macro_" and are deleted at the end.
MEMVAR cA, cB

PROCEDURE Main()
   LOCAL c
   PRIVATE cA := "TQ", cB := "T"
   dbCreate( "rdd_macro_q", { { "NAME", "C", 4, 0 }, { "N", "N", 5, 0 } } )
   dbCreate( "rdd_macro_r", { { "N", "N", 5, 0 } } )
   USE rdd_macro_q ALIAS TQ NEW
   APPEND BLANK
   TQ->NAME := "abc"
   TQ->N := 10
   USE rdd_macro_r ALIAS TR NEW
   APPEND BLANK
   TR->N := 100
   SELECT 3
   ? "compiled:", &cA->NAME, &cA.->NAME, &( cB + "Q" )->NAME, &cB.Q->NAME, &cA->( RecNo() ), Select()
   &cA->NAME := "xyz"
   ? "assigned:", TQ->NAME
   &cA->N += 1
   ? "compound:", TQ->N
   ErrorBlock( {| e | QOut( "ERR", e:subCode, e:description, e:operation ), Break( e ) } )
   FOR EACH c IN { 'TQ->( RecNo(), Select() )', '( "TQ" )->NAME', '( "TQ" )->( Select() )', '( 1 )->NAME', ;
         '( "TQ" )->NAME := "w"', 'TQ->NAME', '( A( "TQ" ) )->N := B( 20 )', '( A( "TQ" ) )->N += B( 5 )', ;
         '( A( "TQ" ) )->N++', '++( A( "TQ" ) )->N', '( "TQ" )->N--', 'TQ->N', ;
         'TQ->( TR->( N + Select() ) + N + Select() )', 'TQ->( TR->( NOPE ) )', 'Select()', ;
         'TQ->( Eval( {|| Select() } ) )', '{| x | TQ->( x + N ) }:Eval( 1 )', 'XX->( 1 )', '( "XX" )->N', ;
         'FIELD->TQ->N', 'FIELD->TQ->( 1 )' }
      BEGIN SEQUENCE
         ? c, "=>", &c
      END SEQUENCE
      ?? " area", Select()
   NEXT
   FOR EACH c IN { 'TQ->( N )', 'TQ->( NOPE )', 'TQ->( Eval( {|| N } ) )', '( "TQ" )->N', '( "TQ" )->NOPE', ;
         '( "XX" )->N', '&( "NO" + "PE" )', 'TQ->( 1 + "a" )', 'N' }
      ? "type", c, Type( c )
   NEXT
   CLOSE ALL
   FErase( "rdd_macro_q.dbf" )
   FErase( "rdd_macro_r.dbf" )
RETURN

FUNCTION A( x )
   ?? " A"
RETURN x

FUNCTION B( x )
   ?? " B"
RETURN x
