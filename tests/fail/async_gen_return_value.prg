// An asynchronous generator misused: RETURN with a value (its elements are what it yields), as in any generator.
ASYNC FUNCTION Gen()
   AWAIT TaskYield()
   YIELD 1
RETURN 2
