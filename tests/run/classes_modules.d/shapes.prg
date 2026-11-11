// A class of another module, found by name when the program runs (a parent and a class function), and a STATIC class
// whose name the main module uses for its own.
CLASS TShape
   VAR name INIT "shape"
   PROTECTED:
   VAR sides INIT 0
   EXPORTED:
   METHOD Describe()
ENDCLASS

METHOD Describe() CLASS TShape
RETURN ::name + " with " + LTrim( Str( ::sides ) ) + " sides"

STATIC CLASS TPart
   VAR who INIT "shapes part"
ENDCLASS

CLASS TShapesPart INHERIT TPart
ENDCLASS
