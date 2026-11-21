// In the project build only: the package's cli library.
INIT PROCEDURE UsesCli()
   LOCAL o := CommandLine():New( "app" )
   o:AddFlag( "quiet", "q", "says nothing" )
   ?? "cli:", o:Parse( { "-q" } ), o:Value( "quiet" )
RETURN
