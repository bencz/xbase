// A codeblock has arguments of its own: `...` inside one is an error.
FUNCTION F( ... )
RETURN Eval( {|| G( ... ) } )
