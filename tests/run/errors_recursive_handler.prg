// An error block that raises errors itself, nested too deep, ends the program.
PROCEDURE Main()
   ErrorBlock( {| e | QOut( "handler", e:subCode ), 1 / 0 } )
   ? "start"
   ? 1 / 0
   ? "not reached"
RETURN
