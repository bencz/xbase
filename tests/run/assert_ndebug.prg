// assert.ch with NDEBUG defined: ASSERT is nothing, not even its expression is evaluated.
#define NDEBUG
#include "assert.ch"

PROCEDURE Main()
   LOCAL n := 0
   ASSERT( .F. )
   ASSERT( ++n > 5, "never" )
   ? "after", n
RETURN
