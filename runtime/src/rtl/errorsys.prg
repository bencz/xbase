// The default error handler. The program's startup calls ErrorSys() by name, after the STATIC initializations and
// before the INIT procedures: a program that defines its own ErrorSys() replaces this one.
#include "error.ch"

// ErrorSys(): makes DefError() the error block.
PROCEDURE ErrorSys()
   ErrorBlock( {| oError | DefError( oError ) } )
RETURN

// A division by zero gives 0; any other error is reported to the standard error with the call stack, and ends the
// program with exit code 1. (The choice between quitting, retrying and the default comes with the terminal layer's
// Alert().) This function and the block are hidden from the call stack: level 0 is where the error happened.
STATIC FUNCTION DefError( oError )
   LOCAL n := 0
   IF oError:genCode == EG_ZERODIV .AND. oError:canSubstitute
      RETURN 0
   ENDIF
   OutErr( Chr( 10 ) + ErrorMessage( oError ) )
   DO WHILE !Empty( ProcName( n ) )
      OutErr( Chr( 10 ) + "Called from " + ProcName( n ) + "(" + LTrim( Str( ProcLine( n ) ) ) + ")  " )
      n++
   ENDDO
   ErrorLevel( 1 )
   QUIT
RETURN .F.

// "Error SUBSYSTEM/subcode  description: operation", with the file name instead of the operation when there is one
// ("Warning" below ES_ERROR).
STATIC FUNCTION ErrorMessage( oError )
   LOCAL cMessage := IIF( oError:severity > ES_WARNING, "Error", "Warning" ) + " "
   cMessage += IIF( ValType( oError:subSystem ) == "C", oError:subSystem, "???" )
   cMessage += "/" + IIF( ValType( oError:subCode ) == "N", LTrim( Str( oError:subCode ) ), "???" )
   IF ValType( oError:description ) == "C"
      cMessage += "  " + oError:description
   ENDIF
   IF !Empty( oError:fileName )
      cMessage += ": " + oError:fileName
   ELSEIF !Empty( oError:operation )
      cMessage += ": " + oError:operation
   ENDIF
RETURN cMessage
