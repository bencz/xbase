/*
 * common.ch - Clipper's common definitions: logical constants, type tests and
 * the commands that give a parameter a default or update a variable.
 *
 * The type tests compare ValType(), so a memo field's value is ISMEMO() and
 * not ISCHARACTER(), and a number of either kind is ISNUMBER().
 */

#ifndef XB_COMMON_CH
#define XB_COMMON_CH

#define TRUE   .T.
#define FALSE  .F.
#define YES    .T.
#define NO     .F.

#translate ISNIL( <v> )       => ( <v> == NIL )
#translate ISARRAY( <v> )     => ( ValType( <v> ) == "A" )
#translate ISBLOCK( <v> )     => ( ValType( <v> ) == "B" )
#translate ISCHARACTER( <v> ) => ( ValType( <v> ) == "C" )
#translate ISDATE( <v> )      => ( ValType( <v> ) == "D" )
#translate ISLOGICAL( <v> )   => ( ValType( <v> ) == "L" )
#translate ISMEMO( <v> )      => ( ValType( <v> ) == "M" )
#translate ISNUMBER( <v> )    => ( ValType( <v> ) == "N" )
#translate ISOBJECT( <v> )    => ( ValType( <v> ) == "O" )

/* DEFAULT x TO v [, y TO w ...]: each variable that is NIL takes its value. */
#xcommand DEFAULT <v1> TO <x1> [, <vn> TO <xn>] => ;
   IF <v1> == NIL ; <v1> := <x1> ; END [; IF <vn> == NIL ; <vn> := <xn> ; END]

/* UPDATE x IF cond TO v: the variable takes the value when the condition holds. */
#command UPDATE <v1> IF <exp> TO <v2> => IF <exp> ; <v1> := <v2> ; END

#endif
