// A class that IMPLEMENTS an interface answers each of its messages, those it inherits too.

INTERFACE IBase
   VAR Name
ENDINTERFACE

INTERFACE IShape INHERIT IBase
   METHOD Area()
ENDINTERFACE

CLASS TFoo IMPLEMENTS IShape
   METHOD Area() INLINE 1
ENDCLASS
