// The English texts are made once for the process: environments that start without texts (a thread's, an ASYNC
// function's, the program's, each before any other has them) take the same array, and their errors read in English
// with no language selected before; a language one thread selects is that thread's alone: another thread, and the
// program, keep English.
PROCEDURE Main()
   LOCAL aThread := {}, oT, aTexts
   oT := ThreadStart( {|| AAdd( aThread, __LanguageCurrent() ), AAdd( aThread, Message() ) } )
   oT:Join()
   aTexts := Texts():Result
   ? "same texts:", aThread[ 1 ] == aTexts, __LanguageCurrent() == aTexts, "messages:", aThread[ 2 ], ;
     Described():Result, Message()
   oT := ThreadStart( {|| Set( _SET_LANGUAGE, "PT" ), AAdd( aThread, Message() ) } )
   oT:Join()
   ? "thread in PT:", aThread[ 3 ], "the program:", Message(), Described():Result
RETURN

ASYNC FUNCTION Described()
RETURN Message()

ASYNC FUNCTION Texts()
   AWAIT TaskYield()
RETURN __LanguageCurrent()

STATIC FUNCTION Message()
   LOCAL e
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      ? 1 + "a"
   RECOVER USING e
      RETURN e:description
   END SEQUENCE
RETURN ""
