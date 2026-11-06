// FOR EACH k => v: no value is an error.
PROCEDURE Main()
   LOCAL a, b, c, h := { "p" => 1 }
   FOR EACH a =>, b IN h
   NEXT
   RETURN
