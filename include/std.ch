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
#command USE <(db)> [VIA <rdd>] [ALIAS <a>] [<new: NEW>] [<ex: EXCLUSIVE>] [<sh: SHARED>] [<ro: READONLY>] ;
            [INDEX <(index1)> [, <(indexN)>]] => ;
         dbUseArea( <.new.>, <rdd>, <(db)>, <(a)>, iif( <.sh.> .or. <.ex.>, !<.ex.>, NIL ), <.ro.> ) ;
         [; dbSetIndex( <(index1)> )] [; dbSetIndex( <(indexN)> )]
#command SELECT <whatever>             => dbSelectArea( <(whatever)> )
#command SELECT <f>( [<list,...>] )    => dbSelectArea( <f>( <list> ) )
#command CLOSE <alias>                 => <alias>->( dbCloseArea() )
#command CLOSE                         => dbCloseArea()
#command CLOSE DATABASES               => dbCloseAll()
#command CLOSE ALL                     => dbCloseAll()
#command COMMIT                        => dbCommitAll()
#command UNLOCK                        => dbUnlock()
#command UNLOCK ALL                    => dbUnlockAll()
#command APPEND BLANK                  => dbAppend()

/* Copies between files (rdd/transfer.prg). DELIMITED WITH takes an expression or a name (BLANK, TAB, PIPE); in the
 * rule tried after it, Clipper's, the rest of the command as text. */
#command COPY [STRUCTURE] [EXTENDED] [TO <(f)>] => __dbCopyXStruct( <(f)> )
#command COPY [STRUCTURE] [TO <(f)>] [FIELDS <fields,...>] => __dbCopyStruct( <(f)>, { <(fields)> } )
#command COPY [TO <(f)>] [FIELDS <fields,...>] [FOR <for>] [WHILE <while>] [NEXT <next>] [RECORD <rec>] ;
            [<rest: REST>] [ALL] [CODEPAGE <cp>] [DELIMITED [WITH <*delim*>]] => ;
         __dbDelim( .T., <(f)>, <(delim)>, { <(fields)> }, <{for}>, <{while}>, <next>, <rec>, <.rest.>, <cp> )
#command COPY [TO <(f)>] [FIELDS <fields,...>] [FOR <for>] [WHILE <while>] [NEXT <next>] [RECORD <rec>] ;
            [<rest: REST>] [ALL] [CODEPAGE <cp>] [DELIMITED [WITH <(delim)>]] => ;
         __dbDelim( .T., <(f)>, <(delim)>, { <(fields)> }, <{for}>, <{while}>, <next>, <rec>, <.rest.>, <cp> )
#command COPY [TO <(f)>] [SDF] [FIELDS <fields,...>] [FOR <for>] [WHILE <while>] [NEXT <next>] [RECORD <rec>] ;
            [<rest: REST>] [ALL] [CODEPAGE <cp>] => ;
         __dbSDF( .T., <(f)>, { <(fields)> }, <{for}>, <{while}>, <next>, <rec>, <.rest.>, <cp> )
#command COPY [TO <(f)>] [FIELDS <fields,...>] [FOR <for>] [WHILE <while>] [NEXT <next>] [RECORD <rec>] ;
            [<rest: REST>] [ALL] [VIA <rdd>] [CODEPAGE <cp>] => ;
         __dbCopy( <(f)>, { <(fields)> }, <{for}>, <{while}>, <next>, <rec>, <.rest.>, <rdd>, , <cp> )
#command APPEND [FROM <(f)>] [FIELDS <fields,...>] [FOR <for>] [WHILE <while>] [NEXT <next>] [RECORD <rec>] ;
            [<rest: REST>] [ALL] [CODEPAGE <cp>] [DELIMITED [WITH <*delim*>]] => ;
         __dbDelim( .F., <(f)>, <(delim)>, { <(fields)> }, <{for}>, <{while}>, <next>, <rec>, <.rest.>, <cp> )
#command APPEND [FROM <(f)>] [FIELDS <fields,...>] [FOR <for>] [WHILE <while>] [NEXT <next>] [RECORD <rec>] ;
            [<rest: REST>] [ALL] [CODEPAGE <cp>] [DELIMITED [WITH <(delim)>]] => ;
         __dbDelim( .F., <(f)>, <(delim)>, { <(fields)> }, <{for}>, <{while}>, <next>, <rec>, <.rest.>, <cp> )
#command APPEND [FROM <(f)>] [SDF] [FIELDS <fields,...>] [FOR <for>] [WHILE <while>] [NEXT <next>] [RECORD <rec>] ;
            [<rest: REST>] [ALL] [CODEPAGE <cp>] => ;
         __dbSDF( .F., <(f)>, { <(fields)> }, <{for}>, <{while}>, <next>, <rec>, <.rest.>, <cp> )
#command APPEND [FROM <(f)>] [FIELDS <fields,...>] [FOR <for>] [WHILE <while>] [NEXT <next>] [RECORD <rec>] ;
            [<rest: REST>] [ALL] [VIA <rdd>] [CODEPAGE <cp>] => ;
         __dbApp( <(f)>, { <(fields)> }, <{for}>, <{while}>, <next>, <rec>, <.rest.>, <rdd>, , <cp> )
#command CREATE <(db)> [FROM <(src)>] [VIA <rdd>] [ALIAS <a>] [<new: NEW>] [CODEPAGE <cp>] => ;
         __dbCreate( <(db)>, <(src)>, <rdd>, <.new.>, <(a)> )
#command SORT [TO <(f)>] [ON <fields,...>] [FOR <for>] [WHILE <while>] [NEXT <next>] [RECORD <rec>] ;
            [<rest: REST>] [ALL] [VIA <rdd>] [CODEPAGE <cp>] => ;
         __dbSort( <(f)>, { <(fields)> }, <{for}>, <{while}>, <next>, <rec>, <.rest.>, <rdd> )
#command TOTAL [TO <(f)>] [ON <key>] [FIELDS <fields,...>] [FOR <for>] [WHILE <while>] [NEXT <next>] ;
            [RECORD <rec>] [<rest: REST>] [ALL] [VIA <rdd>] [CODEPAGE <cp>] => ;
         __dbTotal( <(f)>, <{key}>, { <(fields)> }, <{for}>, <{while}>, <next>, <rec>, <.rest.>, <rdd> )
#command UPDATE [FROM <(alias)>] [ON <key>] [<rand: RANDOM>] [REPLACE <f1> WITH <x1> [, <fN> WITH <xN>]] => ;
         __dbUpdate( <(alias)>, <{key}>, <.rand.>, {|| _FIELD-><f1> := <x1> [, _FIELD-><fN> := <xN>] } )
#command JOIN [WITH <(alias)>] [TO <(f)>] [FIELDS <fields,...>] [FOR <for>] [VIA <rdd>] [CODEPAGE <cp>] => ;
         __dbJoin( <(alias)>, <(f)>, { <(fields)> }, iif( Empty( #<for> ), {|| .T. }, <{for}> ), <rdd> )
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
#command SET EXCLUSIVE <x:ON,OFF,&>    => Set( _SET_EXCLUSIVE, <(x)> )
#command SET EXCLUSIVE (<x>)           => Set( _SET_EXCLUSIVE, <x> )
#command SET SOFTSEEK <x:ON,OFF,&>     => Set( _SET_SOFTSEEK, <(x)> )
#command SET SOFTSEEK (<x>)            => Set( _SET_SOFTSEEK, <x> )
#command SET UNIQUE <x:ON,OFF,&>       => Set( _SET_UNIQUE, <(x)> )
#command SET UNIQUE (<x>)              => Set( _SET_UNIQUE, <x> )
#command SET RELATION TO               => dbClearRelation()
#command SET RELATION [<add:ADDITIVE>] [TO <exp1> INTO <(alias1)> [<scp1:SCOPED>] ;
            [, [TO] <expN> INTO <(aliasN)> [<scpN:SCOPED>]]] => ;
         if ( ! <.add.> ) ; dbClearRelation() ; end ;
         ; dbSetRelation( <(alias1)>, <{exp1}>, <"exp1">, <.scp1.> ) ;
         [ ; dbSetRelation( <(aliasN)>, <{expN}>, <"expN">, <.scpN.> ) ]
#command SET FILTER TO                 => dbClearFilter()
#command SET FILTER TO <xpr>           => dbSetFilter( <{xpr}>, <"xpr"> )
#command SET FILTER TO <x:&>           => if ( Empty( <(x)> ) ) ; dbClearFilter() ; ;
                                          else ; dbSetFilter( <{x}>, <(x)> ) ; end

/* Orders (indexes). INDEX ON without TO makes tag <tag> of the production file (CDX). */
#command INDEX ON <key> TAG <(tag)> [FOR <for>] [<all: ALL>] [WHILE <while>] [NEXT <next>] ;
            [RECORD <rec>] [<rest: REST>] [EVAL <eval>] [EVERY <every>] [<u: UNIQUE>] [<asc: ASCENDING>] ;
            [<d: DESCENDING>] [<a: ADDITIVE>] [<cur: USECURRENT>] => ;
         ordCondSet( <"for">, <{for}>, <.all.>, <{while}>, <{eval}>, <every>, RecNo(), <next>, <rec>, <.rest.>, ;
                     <.d.>, , <.a.>, <.cur.> ) ;
         ; ordCreate( , <(tag)>, <"key">, <{key}>, iif( <.u.>, .T., NIL ) )
#command INDEX ON <key> [TAG <(tag)>] TO <(file)> [FOR <for>] [<all: ALL>] [WHILE <while>] [NEXT <next>] ;
            [RECORD <rec>] [<rest: REST>] [EVAL <eval>] [EVERY <every>] [<u: UNIQUE>] [<asc: ASCENDING>] ;
            [<d: DESCENDING>] [<a: ADDITIVE>] [<cur: USECURRENT>] => ;
         ordCondSet( <"for">, <{for}>, <.all.>, <{while}>, <{eval}>, <every>, RecNo(), <next>, <rec>, <.rest.>, ;
                     <.d.>, , <.a.>, <.cur.> ) ;
         ; ordCreate( <(file)>, <(tag)>, <"key">, <{key}>, iif( <.u.>, .T., NIL ) )
#command SET INDEX TO                  => ordListClear()
#command SET INDEX TO <(index1)> [, <(indexN)>] [<a: ADDITIVE>] => ;
         if ! <.a.> ; ordListClear() ; end ; ordListAdd( <(index1)> ) [; ordListAdd( <(indexN)> )]
#command SET ORDER TO                  => ordSetFocus( 0 )
#command SET ORDER TO <n>              => ordSetFocus( <n> )
#command SET ORDER TO TAG <(tag)>      => ordSetFocus( <(tag)> )
#command SEEK <key> [<soft: SOFTSEEK>] [<last: LAST>] => ;
         dbSeek( <key>, iif( <.soft.>, .T., NIL ), iif( <.last.>, .T., NIL ) )
#command FIND <*text*>                 => dbSeek( <(text)> )
#command DELETE TAG <(tag)> [IN <(bag)>] [, <(tagN)> [IN <(bagN)>]] => ;
         ordDestroy( <(tag)>, <(bag)> ) [; ordDestroy( <(tagN)>, <(bagN)> )]
#command REINDEX [EVAL <eval>] [EVERY <every>] => ordCondSet( , , , , <{eval}>, <every> ) ; ordListRebuild()
#command REINDEX                       => ordListRebuild()
#command SET SCOPETOP TO               => ordScope( 0, NIL )
#command SET SCOPETOP TO <x>           => ordScope( 0, <x> )
#command SET SCOPEBOTTOM TO            => ordScope( 1, NIL )
#command SET SCOPEBOTTOM TO <x>        => ordScope( 1, <x> )
#command SET SCOPE TO                  => ordScope( 0, NIL ) ; ordScope( 1, NIL )
#command SET SCOPE TO <x>, <y>         => ordScope( 0, <x> ) ; ordScope( 1, <y> )
#command SET SCOPE TO <x>              => ordScope( 0, <x> ) ; ordScope( 1, <x> )
#command SET SCOPE TO , <y>            => ordScope( 1, <y> )
#command CLOSE INDEXES                 => ordListClear()

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
/* AVERAGE counts the records in the memory variable __Avg, as Clipper's does (a PRIVATE of the function). */
#command AVERAGE <x1> [, <xN>] TO <v1> [, <vN>] [FOR <for>] [WHILE <while>] [NEXT <next>] [RECORD <rec>] ;
             [<rest: REST>] [ALL] => ;
         M->__Avg := <v1> := [ <vN> := ] 0 ; ;
         dbEval( {|| M->__Avg := M->__Avg + 1, <v1> := <v1> + <x1> [, <vN> := <vN> + <xN>] }, ;
                 <{for}>, <{while}>, <next>, <rec>, <.rest.> ) ; ;
         <v1> := <v1> / M->__Avg [ ; <vN> := <vN> / M->__Avg ]

#command ?  [<explist,...>]            => QOut( <explist> )
#command ?? [<explist,...>]            => QQOut( <explist> )

/* TEXT ... ENDTEXT: each line between them, as written, through QOut(), then QQOut(); `text` is the preprocessor's
   form that takes the lines (see preprocessor.md). */
#command TEXT                          => text QOut, QQOut

/* Settings (see set.ch); the runtime's Set() holds the values. */

#command SET DECIMALS TO <x>           => Set( _SET_DECIMALS, <x> )
#command SET DECIMALS TO               => Set( _SET_DECIMALS, 0 )
#command SET EXACT <x:ON,OFF,&>        => Set( _SET_EXACT, <(x)> )
#command SET EXACT (<x>)               => Set( _SET_EXACT, <x> )
#command SET FIXED <x:ON,OFF,&>        => Set( _SET_FIXED, <(x)> )
#command SET FIXED (<x>)               => Set( _SET_FIXED, <x> )

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
