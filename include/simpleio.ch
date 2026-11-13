/*
 * simpleio.ch - the console commands on the standard output and input only:
 * ? and ?? write with OutStd() (so SET CONSOLE and the redirections of the
 * console do not apply), and ACCEPT reads a line from the standard input.
 */

#ifndef XB_SIMPLEIO_CH
#define XB_SIMPLEIO_CH

#command ?  [<list,...>] => ( OutStd( Chr( 10 ) ) [, OutStd( <list> )] )
#command ?? [<list,...>] => OutStd( <list> )

/* At most 256 bytes read at once; the line ends (LF, CR LF) are dropped. */
#command ACCEPT TO <v> => <v> := StrTran( StrTran( FReadStr( 0, 256 ), Chr( 13 ) ), Chr( 10 ) )
#command ACCEPT <prompt> TO <v> => ? <prompt> ; ACCEPT TO <v>

#endif
