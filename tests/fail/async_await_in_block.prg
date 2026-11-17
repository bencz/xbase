// A codeblock runs as a function of its own: it cannot suspend the ASYNC function that made it.
ASYNC FUNCTION Main()
RETURN Eval( {|| AWAIT TaskRun( {|| 1 } ) } )
