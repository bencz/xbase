// value-captures: Mk's parameter and Base's LOCAL, which codeblocks capture and nothing assigns, are captured by value
// (by.value); Count's LOCAL, which its block assigns, and Ref's parameter, passed by reference, stay shared.
FUNCTION Mk( k )
   RETURN {| x | x + k }

FUNCTION Base( n )
   LOCAL m := n * 2
   RETURN {|| m }

FUNCTION Count()
   LOCAL c := 0
   RETURN {|| ++c }

FUNCTION Ref( v )
   Twice( @v )
   RETURN {|| v }

STATIC PROCEDURE Twice( v )
   v *= 2
   RETURN
