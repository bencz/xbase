// QUIT in an INIT procedure: the start function does not run, the EXIT procedures do.
PROCEDURE Main()
 ? "main"
RETURN
INIT PROCEDURE I()
 ? "init"
 QUIT
RETURN
EXIT PROCEDURE E()
 ? "exit"
RETURN
