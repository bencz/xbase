// `...` in a call and in an array literal: an args.rest operand, made right before its list, once for each `...`.
FUNCTION Pass( a, ... )
RETURN Other( a, ..., 1 )

FUNCTION Elements( ... )
RETURN { ..., 2, ... }
