// fold-constant-statics: a STATIC holding an array literal of constants that no code changes or lets out becomes a
// table (its elements listed under the module), each element read table.index and Len() of it a constant; a STATIC
// whose array an element store changes, or that a call receives, stays a load and an index.
STATIC s_aTable := { 1, 2.5, .F., NIL }
STATIC s_aWritten := { 1, 2 }
STATIC s_aPassed := { 3, 4 }

FUNCTION Read( i )
   s_aWritten[ i ] := 0
RETURN s_aTable[ i ] + Len( s_aTable ) + s_aWritten[ i ] + Pass( s_aPassed )

FUNCTION Pass( a )
RETURN a
