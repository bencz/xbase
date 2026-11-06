// FOR EACH k => v: two arrows is an error.
PROCEDURE Main()
   LOCAL a, b, c, h := { "p" => 1 }
   FOR EACH a => b => c IN h
   NEXT
   RETURN
