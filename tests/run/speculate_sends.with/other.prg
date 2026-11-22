// The other module of speculate_sends: a class answering the messages the main module's classes answer, with members
// of its own.
CLASS TElse
   VAR w INIT 100
   METHOD Only() INLINE "else only"
   METHOD Twice( n ) INLINE n * 3
   METHOD Hello() INLINE "else hello"
ENDCLASS
