// YIELD cannot suspend inside a USING: the value would not be disposed if the generator were left there.
FUNCTION Main()
   USING Mutex():Hold()
      YIELD 1
   END
RETURN
