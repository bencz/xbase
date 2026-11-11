// TRY with CATCH and FINALLY: seq.with and seq.restore around the body, the handler, FINALLY where the body and
// CATCH join and in its own unwind block (seq.suspend, seq.value, seq.resume), and before a RETURN from CATCH.
FUNCTION Main( x )
   TRY
      x := x + 1
   CATCH
      RETURN 0
   FINALLY
      QOut( x )
   END
RETURN x
