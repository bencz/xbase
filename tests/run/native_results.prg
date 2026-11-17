// Runtime functions declared with the types they return (RESULT in rtl.ch): in loops the compiler computes with their
// results as those types, and a value of another type an error block substitutes for a failed call goes on correctly.
// Int(), Len(), Asc(), At(), Val(), Str(), Upper(), SubStr(), Max(), Day() and Empty() in loops whose arguments turn
// bad on some pass; Int() of doubles at and past the 64-bit limit, of negative halves and of -0.5; Round() of
// halves and Mod() of doubles, whose truncation is made without a call.
PROCEDURE Main()
   LOCAL aArgs := { 7.9, -7.9, "x", 1000000000000.5, NIL, -0.5, 9223372036854775807.0, 4 }
   ErrorBlock( {| e | iif( e:canSubstitute, Sub( e ), Break( e ) ) } )
   ? "int", Sum( {| x | Int( x ) }, aArgs )
   ? "len", Sum( {| x | Len( x ) }, { "ab", { 1, 2, 3 }, 5, "", { "a" => 1 }, NIL } )
   ? "asc", Sum( {| x | Asc( x ) }, { "A", "", 66, "z" } )
   ? "at", Sum( {| x | At( "b", x ) }, { "abc", "bbb", 3, "" } )
   ? "val", Sum( {| x | Val( x ) }, { "12", " 3.5", 4, "-1" } )
   ? "str", Join( {| x | Str( x, 5 ) }, { 1, 2.5, "x", -3 } )
   ? "upper", Join( {| x | Upper( x ) }, { "ab", 1, "Cd" } )
   ? "substr", Join( {| x | SubStr( "hello", x, 2 ) }, { 1, 4, "y", 9 } )
   ? "max", Sum( {| x | Max( x, 3 ) }, { 1, 5, "q", 2.5 } )
   ? "day", Sum( {| x | Day( x ) }, { SToD( "20261116" ), 0, SToD( "20260229" ), CToD( "" ) } )
   ? "empty", Join( {| x | iif( Empty( x ), "e", "f" ) }, { "", " a", 0, 1, NIL, {} } )
   ? "ints", Ints()
   ? "rounds", Round( 2.5, 0 ), Round( -2.5, 0 ), Round( 0.125, 2 ), Round( -0.4, 0 ), Round( 1234.5678, -2 )
   ? "mods", Mod( 7.5, 2 ), Mod( -7.5, 2 ), 7.5 % 2, -7.5 % 2, 9007199254740993.0 % 2, 1000000000000000.0 % 7
RETURN

// The block's results added up, in a loop: a substituted value shows as itself.
STATIC FUNCTION Sum( b, aArgs )
   LOCAL x, n := 0, c := ""
   FOR EACH x IN aArgs
      x := Eval( b, x )
      IF ValType( x ) == "N"
         n += x
      ELSE
         c += "[" + ValType( x ) + ":" + Shown( x ) + "]"
      ENDIF
   NEXT
RETURN Str( n, 25, 2 ) + " " + c

STATIC FUNCTION Join( b, aArgs )
   LOCAL x, c := ""
   FOR EACH x IN aArgs
      x := Eval( b, x )
      c += "<" + iif( ValType( x ) == "C", x, ValType( x ) + ":" + Shown( x ) ) + ">"
   NEXT
RETURN c

// A value of another type for each failed call: the function's name.
STATIC FUNCTION Sub( e )
RETURN { e:Operation }

STATIC FUNCTION Shown( x )
RETURN iif( ValType( x ) == "A", x[ 1 ], ValType( x ) )

// Int() at the edges, typed in a loop: each result's value and display width.
STATIC FUNCTION Ints()
   LOCAL x, c := ""
   FOR EACH x IN { 2.5, -2.5, -0.5, 0.5, 9007199254740993.0, -9223372036854775808.0, 9223372036854775807.0, ;
         100000000000000000000.0, -100000000000000000000.0, 123456789012.75 }
      c += "|" + LTrim( Str( Int( x ) ) ) + ":" + LTrim( Str( Len( Str( Int( x ) ) ) ) ) + ":" + ValType( Int( x ) )
   NEXT
RETURN c
