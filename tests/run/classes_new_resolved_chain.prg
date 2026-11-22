// Sends after a New the compiler resolved (the class known, New its Init or nothing): each reaches its own object's
// class, an object made in an INIT value or in Init included (the New resolved left no trace that another send took).
PROCEDURE Main()
   LOCAL o := TA():New(), p := TC():New( 1 )
   ? o:child:Name(), p:child:Name(), o:Name(), p:Name()
   RETURN

FUNCTION MakeB()
   RETURN TB():New()

CLASS TA
   VAR child INIT MakeB()
   METHOD Name() INLINE "TA"
ENDCLASS

CLASS TB
   METHOD Name() INLINE "TB"
ENDCLASS

CLASS TC
   VAR child
   METHOD Init( n )
   METHOD Name() INLINE "TC"
ENDCLASS

METHOD Init( n ) CLASS TC
   ::child := TB():New()
   RETURN Self
