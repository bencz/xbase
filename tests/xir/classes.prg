// Classes: the class function is a class.new; a send in a method (and in its codeblocks) names the method's class
// ("from"), Super:message too ("super"); a send elsewhere names none. Get() to the object New() returns, whose class is
// known, is a call of TFoo's method, which is inlined; its Super:Get() stays a send, and the codeblock's ::a, to Self
// captured (a variable, not the object's known class), is versioned on TFoo (speculate-sends).
PROCEDURE Main()
   ? TFoo():New():Get()
RETURN

CLASS TFoo INHERIT TBar
   VAR a INIT 1
   METHOD Get()
ENDCLASS

METHOD Get() CLASS TFoo
RETURN {|| ::a } + Super:Get()

CLASS TBar
   METHOD Get() INLINE 2
ENDCLASS
