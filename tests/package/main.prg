// The package test's program: a class and a function of another module, and a constant of a standard header that
// only the package's include/ holds (fileio.ch).
#include "fileio.ch"

PROCEDURE Main()
   LOCAL o := Greeter():New( "xbase" )
   ? o:Hello()
   ? Twice( 21 ), F_ERROR, ValType( o )
RETURN
