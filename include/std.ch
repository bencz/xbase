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
#include "rtl.ch"   /* the runtime's EXTERN FUNCTION declarations */

#command DO WHILE <exp>                => while <exp>
#command END <x>                       => end
#command ENDDO <*x*>                   => enddo
#command ENDIF <*x*>                   => endif
#command ENDCASE <*x*>                 => endcase
#command ENDSEQUENCE                   => end
#command ENDFOR [<*x*>]                => next
#command NEXT <v> [TO <x>] [STEP <s>]  => next

#command STORE <v> TO <v1> [, <vN>]    => <v1> := [ <vN> := ] <v>

#command REQUEST <vars,...>            => EXTERNAL <vars>

/* Memory variables: a name or a skeleton (* any characters, ? one) is matched when the code runs. */
#command RELEASE <vars,...>            => __MemRelease( <"vars"> )
#command RELEASE ALL                   => __MemReleaseAll( "*", .T. )
#command RELEASE ALL LIKE <skel>       => __MemReleaseAll( #<skel>, .T. )
#command RELEASE ALL EXCEPT <skel>     => __MemReleaseAll( #<skel>, .F. )
#command CLEAR MEMORY                  => __MemClear()
#command SAVE [TO <(f)>] [ALL]         => __MemSave( <(f)>, "*", .T. )
#command SAVE TO <(f)> ALL LIKE <skel> => __MemSave( <(f)>, #<skel>, .T. )
#command SAVE TO <(f)> ALL EXCEPT <skel> => __MemSave( <(f)>, #<skel>, .F. )
#command SAVE ALL LIKE <skel> TO <(f)> => __MemSave( <(f)>, #<skel>, .T. )
#command SAVE ALL EXCEPT <skel> TO <(f)> => __MemSave( <(f)>, #<skel>, .F. )
#command RESTORE [FROM <(f)>] [<add: ADDITIVE>] => __MemRestore( <(f)>, <.add.> )

/* Scalar classes: values of the type answer the class's messages, as Self. */
#command ASSOCIATE CLASS <cls> WITH TYPE <type: ARRAY, BLOCK, CHARACTER, DATE, HASH, LOGICAL, NIL, NUMERIC> => ;
         ClassAssociate( <cls>(), <"type"> )

/* Work areas and tables (docs/rdd.md). A later rule wins: the plain forms come after the general ones. */
#command USE                           => dbCloseArea()
#command USE <(db)> [VIA <rdd>] [ALIAS <a>] [<new: NEW>] [<ex: EXCLUSIVE>] [<sh: SHARED>] [<ro: READONLY>] => ;
         dbUseArea( <.new.>, <rdd>, <(db)>, <(a)>, iif( <.sh.> .or. <.ex.>, !<.ex.>, NIL ), <.ro.> )
#command SELECT <whatever>             => dbSelectArea( <(whatever)> )
#command SELECT <f>( [<list,...>] )    => dbSelectArea( <f>( <list> ) )
#command CLOSE <alias>                 => <alias>->( dbCloseArea() )
#command CLOSE                         => dbCloseArea()
#command CLOSE DATABASES               => dbCloseAll()
#command CLOSE ALL                     => dbCloseAll()
#command COMMIT                        => dbCommitAll()
#command APPEND BLANK                  => dbAppend()
#command PACK                          => __dbPack()
#command ZAP                           => __dbZap()
#command GOTO <n>                      => dbGoTo( <n> )
#command GO <n>                        => dbGoTo( <n> )
#command GOTO TOP                      => dbGoTop()
#command GO TOP                        => dbGoTop()
#command GOTO BOTTOM                   => dbGoBottom()
#command GO BOTTOM                     => dbGoBottom()
#command SKIP                          => dbSkip( 1 )
#command SKIP <n>                      => dbSkip( <n> )
#command SKIP ALIAS <a>                => <a>->( dbSkip( 1 ) )
#command SKIP <n> ALIAS <a>            => <a>->( dbSkip( <n> ) )
#command SET DELETED <x:ON,OFF,&>      => Set( _SET_DELETED, <(x)> )
#command SET DELETED (<x>)             => Set( _SET_DELETED, <x> )
#command SET FILTER TO                 => dbClearFilter()
#command SET FILTER TO <xpr>           => dbSetFilter( <{xpr}>, <"xpr"> )
#command SET FILTER TO <x:&>           => if ( Empty( <(x)> ) ) ; dbClearFilter() ; ;
                                          else ; dbSetFilter( <{x}>, <(x)> ) ; end

/* Commands over a scope of records: FOR, WHILE, NEXT, RECORD, REST, ALL (dbEval). */
#command DELETE [FOR <for>] [WHILE <while>] [NEXT <next>] [RECORD <rec>] [<rest: REST>] [ALL] => ;
         dbEval( {|| dbDelete() }, <{for}>, <{while}>, <next>, <rec>, <.rest.> )
#command DELETE                        => dbDelete()
#command RECALL [FOR <for>] [WHILE <while>] [NEXT <next>] [RECORD <rec>] [<rest: REST>] [ALL] => ;
         dbEval( {|| dbRecall() }, <{for}>, <{while}>, <next>, <rec>, <.rest.> )
#command RECALL                        => dbRecall()
#command REPLACE [<f1> WITH <x1> [, <fN> WITH <xN>]] [FOR <for>] [WHILE <while>] [NEXT <next>] [RECORD <rec>] ;
                 [<rest: REST>] [ALL] => ;
         dbEval( {|| _FIELD-><f1> := <x1> [, _FIELD-><fN> := <xN>] }, <{for}>, <{while}>, <next>, <rec>, <.rest.> )
#command REPLACE <f1> WITH <x1> [, <fN> WITH <xN>] => _FIELD-><f1> := <x1> [; _FIELD-><fN> := <xN>]
#command LOCATE [FOR <for>] [WHILE <while>] [NEXT <next>] [RECORD <rec>] [<rest: REST>] [ALL] => ;
         __dbLocate( <{for}>, <{while}>, <next>, <rec>, <.rest.> )
#command CONTINUE                      => __dbContinue()
#command COUNT [TO <var>] [FOR <for>] [WHILE <while>] [NEXT <next>] [RECORD <rec>] [<rest: REST>] [ALL] => ;
         <var> := 0 ; dbEval( {|| <var> := <var> + 1 }, <{for}>, <{while}>, <next>, <rec>, <.rest.> )
#command SUM <x1> [, <xN>] TO <v1> [, <vN>] [FOR <for>] [WHILE <while>] [NEXT <next>] [RECORD <rec>] ;
             [<rest: REST>] [ALL] => ;
         <v1> := [ <vN> := ] 0 ; ;
         dbEval( {|| <v1> := <v1> + <x1> [, <vN> := <vN> + <xN>] }, <{for}>, <{while}>, <next>, <rec>, <.rest.> )

#command ?  [<explist,...>]            => QOut( <explist> )
#command ?? [<explist,...>]            => QQOut( <explist> )

/* Settings (see set.ch); the runtime's Set() holds the values. */

#command SET DECIMALS TO <x>           => Set( _SET_DECIMALS, <x> )
#command SET DECIMALS TO               => Set( _SET_DECIMALS, 0 )
#command SET EXACT <x:ON,OFF,&>        => Set( _SET_EXACT, <(x)> )
#command SET EXACT (<x>)               => Set( _SET_EXACT, <x> )

#define _DFSET( x, y ) Set( _SET_DATEFORMAT, iif( __SetCentury(), x, y ) )
#command SET DATE [TO] AMERICAN        => _DFSET( "mm/dd/yyyy", "mm/dd/yy" )
#command SET DATE [TO] ANSI            => _DFSET( "yyyy.mm.dd", "yy.mm.dd" )
#command SET DATE [TO] BRITISH         => _DFSET( "dd/mm/yyyy", "dd/mm/yy" )
#command SET DATE [TO] FRENCH          => _DFSET( "dd/mm/yyyy", "dd/mm/yy" )
#command SET DATE [TO] GERMAN          => _DFSET( "dd.mm.yyyy", "dd.mm.yy" )
#command SET DATE [TO] ITALIAN         => _DFSET( "dd-mm-yyyy", "dd-mm-yy" )
#command SET DATE [TO] JAPANESE        => _DFSET( "yyyy/mm/dd", "yy/mm/dd" )
#command SET DATE [TO] USA             => _DFSET( "mm-dd-yyyy", "mm-dd-yy" )
#command SET DATE FORMAT [TO] <f>      => Set( _SET_DATEFORMAT, <f> )
#command SET CENTURY <x:ON,OFF,&>      => __SetCentury( <(x)> )
#command SET LANGUAGE TO <x>           => Set( _SET_LANGUAGE, <(x)> )
#command SET CODEPAGE TO <x>           => Set( _SET_CODEPAGE, <(x)> )
#command SET CENTURY (<x>)             => __SetCentury( <x> )
#command SET EPOCH TO <y>              => Set( _SET_EPOCH, <y> )

#command QUIT                          => __Quit()
#command CANCEL                        => __Quit()

#command BREAK [<x>]                   => __Break( <x> )

#endif /* _STD_CH */
