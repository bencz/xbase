// The default error handler. The program's startup calls ErrorSys() by name, after the STATIC initializations and
// before the INIT procedures: a program that defines its own ErrorSys() replaces this one.
#include "error.ch"

STATIC s_bUnobserved // the block TaskUnobservedBlock() set, NIL before

// ErrorSys(): makes DefError() the error block.
PROCEDURE ErrorSys()
   ErrorBlock( {| oError | DefError( oError ) } )
RETURN

// A division by zero gives 0; in a task, any other error fails the task (its waiter gets the error, which keeps the
// lines of the calls where it was raised; no BEGIN SEQUENCE of the task takes it); elsewhere it is reported to the
// standard error with the call stack (its innermost XB_CALLS_SHOWN calls), after the calls of the tasks it failed when
// it comes from one (then the first call is where it was waited for), and ends the program with exit code 1. (The
// choice between quitting, retrying and the default comes with the terminal layer's Alert().) This function and the
// block are hidden from the call stack: level 0 is where the error happened.
STATIC FUNCTION DefError( oError )
   LOCAL cBefore, cCalls
   IF oError:genCode == EG_ZERODIV .AND. oError:canSubstitute
      RETURN 0
   ENDIF
   // A table another program (or area) uses in a way that excludes this opening, and an append that cannot lock its
   // new record: NetErr() tells, and the operation does nothing.
   IF ( ( oError:genCode == EG_OPEN .AND. oError:osCode == 32 ) .OR. oError:genCode == EG_APPENDLOCK ) .AND. ;
      oError:canDefault
      NetErr( .T. )
      RETURN .F.
   ENDIF
   cBefore := __ErrorCalls( oError )
   cCalls := __CallLines( oError ) // "Waited for in " first when cBefore has lines
   IF __TaskFail( oError, cCalls ) // in a task: it fails with the error, whatever sequence is around
      RETURN .F.
   ENDIF
   OutErr( Chr( 10 ) + ErrorMessage( oError ) + cBefore + cCalls )
   ErrorLevel( 1 )
   QUIT
RETURN .F.

// "Error SUBSYSTEM/subcode  description: operation", with the file name instead of the operation when there is one
// ("Warning" below ES_ERROR), then the system's error code when there is one.
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
   IF !Empty( oError:osCode )
      cMessage += " (DOS Error " + LTrim( Str( oError:osCode ) ) + ")"
   ENDIF
RETURN cMessage

// TaskUnobservedBlock( [bNew] ): the block that gets the failures of tasks nothing observed (__TaskUnobserved), NIL
// before one is set; a block replaces it, as ErrorBlock()'s argument does, for every thread and task.
FUNCTION TaskUnobservedBlock( bNew )
   LOCAL bOld := s_bUnobserved
   IF ValType( bNew ) == "B"
      s_bUnobserved := bNew
   ENDIF
RETURN bOld

// __TaskUnobserved( xFailure, oTask ): a task failed with xFailure, nothing observed it (nothing waited for it,
// continued from it, read its failure or asked whether it failed), and the collector found it unreachable (the Task
// class's destructor calls this). The block TaskUnobservedBlock() set gets both; without one, it is reported to the
// standard error: an Error object as the default handler reports one, with the lines of the calls where it was
// raised, another value by its type. The program goes on.
PROCEDURE __TaskUnobserved( xFailure, oTask )
   IF ValType( s_bUnobserved ) == "B"
      Eval( s_bUnobserved, xFailure, oTask )
   ELSEIF ValType( xFailure ) == "O" .AND. xFailure:ClassName() == "ERROR"
      OutErr( Chr( 10 ) + "Unobserved task failure: " + ErrorMessage( xFailure ) + __ErrorCalls( xFailure ) )
   ELSE
      OutErr( Chr( 10 ) + "Unobserved task failure: a value of type " + ValType( xFailure ) )
   ENDIF
RETURN
