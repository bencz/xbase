// .AND./.OR. as values: the left side decides and the right side's value is the result as it is (Clipper's short
// circuit), the same folded on literals and computed at run time; a non-logical left side is an error (the last line).
PROCEDURE Main()
   LOCAL t := .T., f := .F., x := 5
   ? t .AND. x, .T. .AND. x
   ? f .OR. x, .F. .OR. x
   ? f .AND. x, t .OR. x
   ? ValType( t .AND. "s" ), ValType( f .OR. NIL )
   ? t .AND. f, f .OR. t, t .AND. t .AND. x
   IF t .AND. ( x > 1 )
      ? "if"
   ENDIF
   ? x .AND. t
   RETURN
