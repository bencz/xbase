FUNCTION RtFail()
RETURN Inner()

STATIC FUNCTION Inner()
RETURN Eval( {|| 1 + "a" } )
