// Optional clauses: a marker that cannot take a second value fails the rule (it stays as written), a clause
// without a keyword (a restricted marker counts as one) is tried after the one with a keyword, and each repetition
// of a result clause is spaced by its own first token or the clause's bracket (a comma keeps its own).
#command TESTA [<a,...>] => QOut( <a> )
#command TESTB [<a>] => QOut( <a> )
#command TESTC [FOO <a>] => QOut( <a> )
#command TESTD [<a>] [BAR <b>] => QOut( <a>, <b> )
#command TESTE [FOO <a>] [<b>] => QOut( <a>, <b> )
#command TESTF [FOO <a>] => QOut( [<a>] )
#xtranslate STR(<x>) => #<x>
#command A1 [FOO <a>] => ? STR( F([ <a>] ) )
#command A2 [FOO <a>] => ? STR( F([ {<a>}] ) )
#command A3 [FOO <a>] => ? STR( F( [, <a>] ) )
#command A4 [FOO <a>] => ? STR( F(x[ ,<a>] ) )
#command A5 <x> [, <y>] => ? STR( F( <x> [, <y>] ) )
#command A6 [FOO <a>] => ? STR( F( [<a>] ) )
#command ZZ [<a>] [<n: NEW>] => F( <a>, <"n"> )
#command T9 [FOO [<a>]] [BAR <b>] => F9( <a>, <b> )
PROCEDURE Main()
TESTA 1 abc
TESTB 1 2
TESTC FOO 1 FOO 2
TESTD 1 BAR 2
TESTD BAR 2 1
TESTE FOO 1 2
TESTE 2 FOO 1
TESTD 1 2
TESTF FOO 1 FOO 2
? 1 abc
? 1, 2 3
? 1e3
A1 FOO 1 FOO 2
A2 FOO 1 FOO 2
A3 FOO 1 FOO 2
A4 FOO 1 FOO 2
A5 1, 2, 3
A6 FOO 1 FOO 2
ZZ 1 NEW
ZZ NEW 1
T9 FOO 1 BAR 2
RETURN
