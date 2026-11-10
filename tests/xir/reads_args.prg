// A function or codeblock that calls PCount() or PValue() keeps its arguments (`args`); PValue() also keeps the
// parameters in their slots, so Value is not versioned for references; Plain keeps nothing, its codeblock does.
FUNCTION Count( a )
RETURN PCount() + a

FUNCTION Value( a )
   a := 1
RETURN PValue( 1 )

FUNCTION Plain( a )
RETURN {| x | PCount() }
