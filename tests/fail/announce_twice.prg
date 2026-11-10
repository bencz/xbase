// A module announces one name: a second ANNOUNCE is ignored with a warning (the wrong argument count of ValType()
// ends the compilation, so that it shows in a compile-failure test).
ANNOUNCE First
ANNOUNCE Second
PROCEDURE Main()
   ? ValType()
RETURN
