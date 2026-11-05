// regular, list, restricted, wild, extended, optional in any order
#command SHOW <x> [COLOR <c>] [AT <r>, <col>] => ShowIt( <x>, <c>, <r>, <col> )
SHOW "a"
SHOW "b" AT 1, 2 COLOR "R"
SHOW "c" COLOR "G"
#command SET FLAG <x: ON, OFF, &> => SetFlag( <(x)> )
SET FLAG ON
SET FLAG off
SET FLAG &cVar
#command LOG <*text*> => LogIt( #<text> )
LOG anything at all, even "strings" + 1
#command USE <(db)> [ALIAS <a>] [<ro: READONLY>] => dbUse( <(db)>, <(a)>, <.ro.> )
USE customer.dbf ALIAS cust READONLY
USE (cFile)
#command LIST <v,...> => ListIt( <"v"> ) ; ListB( <{v}> )
LIST name, age + 1
// repeating optional clause
#command STOREX <v> TO <v1> [, <vN>] => <v1> := [ <vN> := ] <v>
STOREX 0 TO a, b, c
// abbreviations: #command accepts 4+ letters, #xcommand needs the word
#command DISPLAY <x> => DispIt( <x> )
#xcommand XDISPLAY <x> => XDispIt( <x> )
DISP 1
DISPL 2
XDISPLAY 3
XDISP 4
// translate works inside expressions
#xtranslate Square( <n> ) => ( (<n>) * (<n>) )
y := 1 + Square( x + 1 ) + 2
// escaped brackets and a result with ;
#command TWO <a> <b> => First( <a> ) ; Second( <b> )
TWO 1 2
