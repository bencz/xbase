// Classes and an interface of another module, which the program's classes inherit from or implement: the rules they
// break are the runtime's to find, when such a class first makes an object.
FINAL CLASS TSealed
   VAR n INIT 1
ENDCLASS

ABSTRACT CLASS TOtherBase
   METHOD Need() ABSTRACT
   METHOD Twice() INLINE ::Need() * 2
ENDCLASS

INTERFACE IOther
   METHOD Ping()
   VAR Label
ENDINTERFACE

CLASS TFinalMethod
   METHOD Fixed() FINAL INLINE "fixed"
ENDCLASS

INTERFACE ICycA INHERIT ICycB
   METHOD A()
ENDINTERFACE

CLASS TFinBase
   METHOD Foo() FINAL INLINE "final foo"
ENDCLASS

CLASS TFinA INHERIT TFinBase
ENDCLASS
