// Operators and logical words take one spelling, whatever the source wrote: `!=` is `<>`, `**` is `^`, `**=` is
// `^=`, `.NOT.` is `!`, `.and.` and `.or.` are upper case, `.Y.` is `.T.` and `.N.` is `.F.`. Stringified text (an
// index's FOR condition) and the output show it; `#` and string literals keep theirs.
#xtranslate SHOW <x> => QOut( <"x"> )
#xtranslate DUMB <x> => QOut( #<x> )
#xtranslate SMART <x> => QOut( <(x)> )
#define NE(a,b) a != b
SHOW a != b
SHOW a # b
SHOW a <> b
SHOW .not. a == b
SHOW .not.a
SHOW a ** b
SHOW a **= b
SHOW a .and. b .Or. a
SHOW a.and.b
SHOW .t. .or. .y. .and. .F. .or. .n.
SHOW NE(a,b)
DUMB a!=b.or.!a
SMART a!=b
x := a != b .and. a ** 2 > 1
? "a != b", [a ** b]
