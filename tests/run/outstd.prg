// OutStd() writes to the standard output and OutErr() to the standard error, not to the console: the values as QOut()
// shows them, separated by spaces, with no new line.
PROCEDURE Main()
   ? "console"
   OutStd( Chr( 10 ) + "std:", 12, .T., NIL, "x" )
   OutStd()
   OutErr( "err:", 3.5, .F. )
   ? "end"
RETURN
