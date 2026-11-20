// RUN and ! run a command line through the system's shell, its output in order with the program's; RUN ( expr ) takes
// the line from an expression; ErrorLevel() stays as it was; RUN = and RUN := assign a variable named run; __Run()
// of no line runs nothing.
PROCEDURE Main()
   LOCAL c := "echo from macro", run := 1
   ? "before"
   RUN echo hello world
   RUN ( c )
   ! echo bang
   RUN exit 3
   ? "after", ErrorLevel()
   __Run( "echo direct" )
   RUN := 5
   ? "variable", run
   RUN = 7
   ? "again", run
   __Run()
   __Run( 12 )
   ? "end"
RETURN
