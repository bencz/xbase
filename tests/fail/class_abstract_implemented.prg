// An ABSTRACT method has no body: an implementation of it is an error.

ABSTRACT CLASS TFoo
   METHOD Area() ABSTRACT
ENDCLASS

METHOD Area() CLASS TFoo
RETURN 1
