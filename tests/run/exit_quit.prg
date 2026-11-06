// QUIT in an EXIT procedure stops the EXIT procedures after it.
PROCEDURE Main()
 ? "main"
RETURN
EXIT PROCEDURE E()
 ? "exit1"
 QUIT
 ? "no"
RETURN
EXIT PROCEDURE E2()
 ? "exit2"
RETURN
