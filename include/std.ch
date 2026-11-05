/*
 * std.ch - xbase standard commands.
 *
 * Loaded automatically by xbc before every source file (disable with -u,
 * replace with -u<file>). Commands are ordinary preprocessor rules: the
 * compiler itself knows nothing about them.
 *
 * Only commands whose runtime support exists are listed; this file grows
 * together with the runtime.
 */

#ifndef _STD_CH
#define _STD_CH

#include "set.ch"

#command DO WHILE <exp>                => while <exp>
#command END <x>                       => end
#command ENDDO <*x*>                   => enddo
#command ENDIF <*x*>                   => endif
#command ENDCASE <*x*>                 => endcase
#command ENDFOR [<*x*>]                => next
#command NEXT <v> [TO <x>] [STEP <s>]  => next

#command STORE <v> TO <v1> [, <vN>]    => <v1> := [ <vN> := ] <v>

#command ?  [<explist,...>]            => QOut( <explist> )
#command ?? [<explist,...>]            => QQOut( <explist> )

/* Settings (see set.ch); the runtime's Set() holds the values. */

#command SET DECIMALS TO <x>           => Set( _SET_DECIMALS, <x> )
#command SET DECIMALS TO               => Set( _SET_DECIMALS, 0 )
#command SET EXACT <x:ON,OFF,&>        => Set( _SET_EXACT, <(x)> )
#command SET EXACT (<x>)               => Set( _SET_EXACT, <x> )

#command QUIT                          => __Quit()
#command CANCEL                        => __Quit()

#endif /* _STD_CH */
