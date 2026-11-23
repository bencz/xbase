// No subclass declares a FINAL method again, as a class variable either.
CLASS TBase
   METHOD Id() FINAL INLINE 1
ENDCLASS

CLASS TChild INHERIT TBase
   CLASSVAR Id
ENDCLASS
