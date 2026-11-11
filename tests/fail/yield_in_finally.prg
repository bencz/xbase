// YIELD cannot suspend inside the FINALLY code of a TRY either.
FUNCTION Main()
   TRY
      ? "body"
   CATCH
      ? "catch"
   FINALLY
      YIELD 1
   END
RETURN
