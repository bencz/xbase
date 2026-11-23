// No subclass declares a FINAL method again.

CLASS TBase
   METHOD Id() FINAL INLINE 1
ENDCLASS

CLASS TChild INHERIT TBase
   METHOD Id() INLINE 2
ENDCLASS
