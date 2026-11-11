// The macro operator: macro.* operations take the text as an item, evaluated once for an assignment (macro.get, then
// macro.set of the same text); a string with `&name` is substituted when it runs (macro.text), one without stays a
// constant.
MEMVAR c
PROCEDURE Main()
   LOCAL n := 1
   c := "n"
   n := &c + 1
   &c := n
   &c++
   ? &c.( n ), "&c.x"
   PRIVATE &c := 2
   PUBLIC &( c + "2" )
RETURN

FUNCTION NoMacro( x )
RETURN x + "a & b"
