// QUIT in the start function: the EXIT procedures still run; ErrorLevel() sets the exit code,
// which an EXIT procedure may change.
PROCEDURE Main()
 ? "main"
 ErrorLevel( 3 )
RETURN
EXIT PROCEDURE E()
 ? "exit", ErrorLevel( 5 )
RETURN
