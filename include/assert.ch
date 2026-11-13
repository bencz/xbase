/*
 * assert.ch - ASSERT( <exp> [, <msg>] ): when the expression is false, writes
 * where it is (the procedure and line) and the message, or the expression as
 * written, to the standard output, and ends the program (QUIT). With NDEBUG
 * defined before this file, ASSERT is nothing.
 */

#ifndef XB_ASSERT_CH
#define XB_ASSERT_CH

#define _ASSERT_DEFINED

#ifdef NDEBUG
#command ASSERT( <exp> [, <msg>] ) =>
#else
#command ASSERT( <exp> [, <msg>] ) => ;
   IF !( <exp> ) ; ;
      OutStd( Chr( 10 ) + ProcName( 0 ) + "(" + LTrim( Str( ProcLine() ) ) + ")  Assertion failed: " + ;
              iif( <.msg.>, <msg>, <"exp"> ) ) ; ;
      QUIT ; ;
   END
#endif

#endif
