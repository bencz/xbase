// .AND. cannot be overloaded (the reference accepts it and never calls it).
CLASS TFoo
   OPERATOR ".AND." ARG x INLINE .T.
ENDCLASS
