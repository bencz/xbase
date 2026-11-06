// An error that ends the program runs the EXIT procedures, with exit code 1 already set.
PROCEDURE Main()
 LOCAL x
 ? "main"
 x := 1 + "a"
RETURN
EXIT PROCEDURE E()
 ? "exit", ErrorLevel()
RETURN
