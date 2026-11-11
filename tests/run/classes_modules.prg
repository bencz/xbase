// Classes across separately compiled modules: a parent class and a class function of another module, found by name
// when the program runs, and a STATIC class in each module under the same name, each the parent of its module's
// classes.
PROCEDURE Main()
   ? TShape():New():Describe()
   ? TSquare():New():Describe(), TSquare():New():Area( 3 )
   ? TMainPart():New():who, TShapesPart():New():who
RETURN

STATIC CLASS TPart
   VAR who INIT "main part"
ENDCLASS

CLASS TMainPart INHERIT TPart
ENDCLASS

CLASS TSquare INHERIT TShape
   METHOD New()
   METHOD Area( n ) INLINE n * n * ::sides / 4
ENDCLASS

METHOD New() CLASS TSquare
   ::name := "square"
   ::sides := 4
RETURN Self
