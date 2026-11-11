// PRIVATE belongs to functions, not to extended codeblocks.
PROCEDURE Main()
   LOCAL b := {||
      PRIVATE x := 1
      RETURN x
   }
RETURN
