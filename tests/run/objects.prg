// Objects of a runtime class (Error) and messages: reading, assigning and
// incrementing instance variables, the rules of each Error variable and what
// its assignment returns, and ClassName() for every type.
PROCEDURE Main()
   LOCAL o := ErrorNew(), o2 := ErrorNew(), a, i

   ? ValType( o ), o:ClassName(), o:className
   ? ValType( o:args ), o:canDefault, o:canRetry, o:canSubstitute, ValType( o:cargo )
   ? "[" + o:description + "]", "[" + o:filename + "]", o:genCode, "[" + o:operation + "]", o:osCode
   ? o:severity, o:subCode, "[" + o:subSystem + "]", o:tries

   o:description := "x"
   ? o:description, o:DESCRIPTION, o:Description(), o:description( 1, 2 )
   o:tries += 2
   o:tries++
   ++o:tries
   ? o:tries, o:tries--, o:tries
   ? o:description := 1, o:description
   ? o:_description( "z" ), o:description
   ? o:_description(), ValType( o:description )
   o = o2
   o:cargo = { 1, 2 }
   ? Len( o:cargo ), o:cargo[ 2 ], o2:cargo[ 1 ]
   o:cargo[ 1 ] := "first"
   ? o2:cargo[ 1 ]

   o:args := { 1 }
   o:args := 1
   ? ValType( o:args ), Len( o:args )
   o:canRetry := .T.
   o:canRetry := 1
   ? o:canRetry, o:_canRetry( 1 )
   o:tries := 1.5
   ? o:tries
   o:tries := 3000000000
   ? o:tries
   o:tries := -1.7
   ? o:tries
   o:severity := 70000
   ? o:severity
   o:osCode := 70000
   o:genCode := 5000000000
   o:subCode := -1
   ? o:osCode, o:genCode, o:subCode
   o:osCode := 3000000000.7
   ? o:osCode
   o:osCode := 10 ^ 30
   ? o:osCode
   ? o:_tries( 2, 3 ), o:tries

   ? (1):ClassName(), "a":ClassName(), {}:ClassName(), {|| 1 }:ClassName(), NIL:ClassName(), .T.:ClassName(), ;
     Date():ClassName()
   ? o
   ? Empty( o ), o == o, o == ErrorNew(), Len( { o } )
   a := { 1, o, "s", NIL, {}, .T., Date(), {|| 1 } }
   ASort( a )
   FOR i := 1 TO Len( a )
      QQOut( ValType( a[ i ] ) + " " )
   NEXT
   ? AScan( { 1, o }, o )
   ? Trace( o2, "object" ):cargo := Trace( "value", "value" )

   // The value of an assignment is what the variable's _X message returns.
   o := ErrorNew()
   ? o:canRetry := 5, o:args := 7, o:tries := 1.5
   o:tries := 1
   ? o:tries += 1.5, o:tries
   ? o:_description( 1 ), o:_description( 2.5 ), o:_tries( 70000 ), o:tries, o:_tries( -1.7 ), o:tries
   ? o:_tries( 3000000000 ), o:_osCode( 3000000000 ), o:_osCode( 10 ^ 30 ), o:_tries( 10 ^ 30 ), o:_tries( 2.5 )
   ? o:_cargo(), o:_cargo( 4 ), o:_canRetry( .T. ), o:_args( { 1 } ), o:_subSystem( "x" )
   o:tries := 5
   ? ++o:tries, o:tries++, o:tries
   ? o:_severity( 70000.9 ), o:_genCode( -3.5 )
RETURN

STATIC FUNCTION Trace( x, cWhat )
   QQOut( cWhat + " " )
RETURN x
