PROCEDURE Main()
   LOCAL o := ErrorNew(), x
   WITH OBJECT o
      x := :& + 1
   END
   ? x
RETURN
