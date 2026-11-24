// Part of tests/run/block_direct_entry.prg: blocks made in another module, and a block evaluated there.
FUNCTION Tripler()
RETURN {| p | p * 3 }

FUNCTION EvalThere( b, x )
RETURN Eval( b, x, "past its parameters" )
