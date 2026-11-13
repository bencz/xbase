// TEXT ... ENDTEXT: each line as written goes through QOut() (blanks, tabs, quotes of every kind, comment and
// statement marks kept; blank lines too), `&name` substituted as in a string literal, then QQOut(): what follows
// goes on after the last line. ENDTEXT as a whole word, in any letter case and indented, ends it when nothing but
// blanks, a comment or `;` and statements (which run) follow; otherwise the line is text. The other statements of the
// TEXT line run before the lines. A TEXT inside a function, inside an extended codeblock and in an included file; a
// #xtranslate applies to the calls the lines become.
MEMVAR cName

PROCEDURE Main()
   PRIVATE cName := "World"
   ?? "before"
   TEXT
line one
   indented "double" 'single' [bracket] all: "'] together
	tab first

  // not a comment /* nor this */ ; semicolon && neither
#define NOT_A_DIRECTIVE
Hello &cName. and &cName! and &nosuch and && and &
not ENDTEXT here
ENDTEXTS is no end either
   endtext
   ?? "after"
   TEXT
a
ENDTEXT; ?? "<after a>"; ?? "<and more>"
   TEXT
b
ENDTEXT//comment
   TEXT
ENDTEXT is a line of text
ENDTEXT /* comment */
   ? 1; TEXT; ?? "<rest of the line first>"
c
   EndText && comment
#include "text_block.ch"
   Shown()
   Translated()
   Eval( {||
      TEXT
inside a codeblock
      EndText
      RETURN NIL
   } )
   ? "end"
RETURN

STATIC PROCEDURE Shown()
   TEXT
in a function
   ENDTEXT // the rest of this line is ignored
RETURN

#xtranslate QOut( <x> ) => OutStd( Chr( 10 ) + "[" + <x> + "]" )

STATIC PROCEDURE Translated()
   TEXT
translated
   ENDTEXT
RETURN
