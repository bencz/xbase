PROCEDURE Main()
   LOCAL o := ErrorNew()
   ? "start"
   o:foo := 1
   ? "not reached"
   RETURN
