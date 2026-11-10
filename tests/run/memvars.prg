// Memory variables, compared with the reference: PUBLIC (.F. at first), PRIVATE visible in the functions called and
// ended with its activation (a function or a codeblock), hiding and showing back, an undeclared name assigned (a
// PRIVATE of the function), PARAMETERS, M->, codeblocks, @, a PUBLIC over a visible PRIVATE (it assigns the PRIVATE),
// a repeated PRIVATE (kept), and an initializer reading the variable it hides.
MEMVAR pub1, priv1, undecl, x, p1, p2, arr, cnt
PROCEDURE Main()
   PUBLIC pub1
   ? "pub init", ValType( pub1 ), pub1
   PRIVATE priv1 := 10
   Callee()
   ? "after callee priv1", priv1
   ? "undecl created in Sub:", Sub(), Exists( "undecl" )
   undecl := 5
   ? "undecl here", undecl
   PRIVATE x := "outer"
   Hide()
   ? "x back", x
   Params( 1, 2, 3 )
   PRIVATE arr[ 3 ]
   ? "arr", Len( arr ), ValType( arr[ 1 ] )
   cnt := 0
   Eval( {|| cnt++ } )
   Eval( {|| cnt++ } )
   ? "cnt", cnt
   ByRef( @cnt )
   ? "byref cnt", cnt
   ? "m->", M->cnt, M->pub1
   PUBLIC pub1 := 7
   ? "pub again", pub1
   MakePub()
   ? "pub from callee", M->newpub
   PRIVATE dup := 1
   PRIVATE dup := 2
   ? "dup", M->dup
   PRIVATE pp := "private"
   PUBLIC pp
   ? "public over private", M->pp
   PubThenPriv()
   ? "pub2 after", M->pub2
   PRIVATE y := 1
   PRIVATE y
   ? "private again", ValType( M->y )
   Eval( {|| M->inblock := "b" } )
   ? "after block", Exists( "inblock" )
   PRIVATE z := 1
   Init()
   ? "z outer", M->z
   PRIVATE pv := "priv"
   PUBLIC pv := "pubinit"
   ? "pv", M->pv
RETURN

FUNCTION Exists( c )
   LOCAL lFound := .T., bOld := ErrorBlock( {|| lFound := .F., Break() } )
   BEGIN SEQUENCE
      DO CASE
      CASE c == "undecl"
         Eval( {|| M->undecl } )
      CASE c == "inblock"
         Eval( {|| M->inblock } )
      ENDCASE
   END SEQUENCE
   ErrorBlock( bOld )
RETURN lFound

PROCEDURE Callee()
   ? "callee sees priv1", priv1
   priv1 := 20
RETURN

FUNCTION Sub()
   undecl := 1
RETURN Exists( "undecl" )

PROCEDURE Hide()
   PRIVATE x := "inner"
   ? "hide x", x, Deeper()
RETURN

FUNCTION Deeper()
RETURN x

PROCEDURE Params
   PARAMETERS p1, p2
   ? "params", p1, p2, PCount()
RETURN

PROCEDURE ByRef( v )
   v := 99
RETURN

PROCEDURE MakePub()
   PUBLIC newpub := "made"
RETURN

PROCEDURE PubThenPriv()
   PUBLIC pub2 := "p"
   Inner()
RETURN

PROCEDURE Inner()
   PRIVATE pub2 := "hidden"
   ? "inner pub2", M->pub2
RETURN

PROCEDURE Init()
   PRIVATE z := M->z + 1
   ? "z inner", M->z
RETURN
