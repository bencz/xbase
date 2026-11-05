// Statements that expand to nothing are dropped.
#define EMPTY_DEF
#xtranslate GONE =>
EMPTY_DEF
GONE
// #x... rules need full words in restricted markers and stop tokens.
#xcommand SET FOO <x: ALWAYS, NEVER> => SetFoo( <"x"> )
SET FOO ALWA
SET FOO always
#xcommand SORTX <x> DESCENDING => SortIt( <x> )
SORTX nVal - DESC DESCENDING
// A marker matched once supplies its value to every repetition.
#command BAR [A <a>] [B <b>] => [f( <a>, <b> );]
BAR A 1 B 2 B 3
// Directive names may be abbreviated.
#ifde NOT_DEFINED_NAME
   this is skipped
#endi
// Stringify markers convert each comma-separated element; macros are unwrapped.
#command BAZ <*x*> => g( <"x"> )
BAZ a, b
#command QQ <x,...> => h( <"x"> )
QQ &cVar, &(e)
// A call with the wrong number of arguments is not a macro call.
#define F(a,b) a+b
y := F(1)
// != and <> are the same operator.
#command CMP <a> <> <b> => Cmp( <a>, <b> )
CMP x != y
// A trailing comma does not add an empty restricted word.
#xcommand OPT <x: ON,> => Opt( <"x"> )
OPT
OPT ON
