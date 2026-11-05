// Stringified text writes every string with delimiters chosen from its value.
#command LOG2 <*t*> => QOut( #<t> )
#command NORM <x,...> => QOut( <"x"> )

PROCEDURE Main()
   LOG2 "a" + 'b' + [c]
   LOG2 'x"y' + "it's"
   LOG2 'x"y' + "it's" + "a]"
   NORM 'q', [r] + "s"
   RETURN
