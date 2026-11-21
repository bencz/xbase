// A SCALAR CLASS inherits no instance variables: its values have no area for them (TB inherits TA's).
SCALAR CLASS CHARACTER INHERIT TB
   METHOD Up() INLINE Upper( Self )
ENDCLASS

CLASS TB INHERIT TA
ENDCLASS

CLASS TA
   VAR a INIT 1
ENDCLASS
