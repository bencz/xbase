// `...` in a call and in an array literal: the operand list shows where the arguments past the parameters go.
FUNCTION Pass( a, ... )
RETURN Other( a, ..., 1 )

FUNCTION Elements( ... )
RETURN { ... }
