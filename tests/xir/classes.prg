// Classes: the class function is a class.new; a send in a method (and in its codeblocks) names the method's class
// ("from"), Super:message too ("super"); a send elsewhere names none.
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
