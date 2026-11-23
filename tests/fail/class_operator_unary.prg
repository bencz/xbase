// UNARY names unary minus only ("-" UNARY): unary plus leaves its operand as it is, and the other unary operators
// (! ++ -- ~) are unary already.
CLASS TFoo
   OPERATOR "+" UNARY INLINE .T.
ENDCLASS
