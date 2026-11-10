// `...` in a call and in an array literal: an args.rest operand, the last, made right before its list.
FUNCTION Pass( a, ... )
RETURN Other( a, 1, ... )

FUNCTION Elements( ... )
RETURN { 2, ... }
