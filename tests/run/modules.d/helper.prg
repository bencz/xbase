// The second module of tests/run/modules.prg: compiled with --no-entry.
STATIC s_count := 0

FUNCTION Helper( nValue )
   RETURN nValue * 10 + Len( "abc" )

FUNCTION Counter()
   s_count++
   RETURN s_count
