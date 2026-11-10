// An error left unhandled inside a hidden function of the runtime's xBase code (a STATIC function and its codeblock):
// the "Called from" lines skip them and show the public function with line 0.
EXTERN FUNCTION RtFail() NATIVE

PROCEDURE Main()
   RtFail()
RETURN
