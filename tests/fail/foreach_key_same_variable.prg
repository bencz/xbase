// FOR EACH k => v: same variable is an error.
PROCEDURE Main()
   LOCAL a, b, c, h := { "p" => 1 }
   FOR EACH a => a IN h
   NEXT
   RETURN
