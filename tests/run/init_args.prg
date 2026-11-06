// INIT procedures and functions receive the command-line arguments; EXIT FUNCTION too.
PROCEDURE Main()
 ? "main"
RETURN
INIT PROCEDURE Setup( a, b )
 ? "setup", a, b
RETURN
INIT FUNCTION Fn()
 ? "init function"
RETURN 1
EXIT FUNCTION Ef()
 ? "exit function"
RETURN 2
