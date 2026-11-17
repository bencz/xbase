// A file-wide STATIC's initializer runs in no function and must be constant: AWAIT there is that error (no crash).
STATIC s_x := AWAIT 1

PROCEDURE Main()
   ? s_x
RETURN
