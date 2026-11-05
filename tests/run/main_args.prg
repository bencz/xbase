// The command-line arguments (tests/run/main_args.args) are the start procedure's parameters, as strings; a
// parameter without an argument is NIL, and an argument may hold blanks.
PROCEDURE Main( cFirst, cSecond, cThird, cFourth )
   ? ValType( cFirst ), ValType( cSecond ), ValType( cThird ), ValType( cFourth )
   ? cFirst, "[" + cSecond + "]", "[" + cThird + "]"
   ? Val( cFirst ) * 2
   RETURN
