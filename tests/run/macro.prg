// The macro operator, compared with the reference: &name, &name., &name.suffix, prefix&name., &( expr ),
// assignment, macro calls, PRIVATE/PUBLIC &name, codeblocks from text, text substitution in strings, Type(), and the
// errors. Deliberate differences: a macro's operators run when it runs, as a variable's would (`&( "7 % 3" )` is 1.00;
// the reference folds some constants); a comma list in a macro (`&c` with c = "1,2") is a syntax error, as in Clipper
// (the reference's own mode spreads it into arguments).
MEMVAR mv, c, d, cName, cnt, arr, p1, p2, zz, newvar, abcxyz, subvar, made

PROCEDURE Main()
   LOCAL l := 7, b, n := 3, a, i, cKind
   PRIVATE mv := 10, c := "mv", d, cName, cnt := 0, arr := { 1, 2, 3 }, abcxyz := "joined"

   ? "names:", &c, &c., &c + 1, &( "mv * 2" ), &( "1+2" ), &( "Upper('abc')" ), &( "10/4" )
   c := "m"
   ? "suffix:", &c.v
   cName := "xy"
   ? "prefix:", abc&cName.z
   c := "  mv  "
   ? "blanks:", &c

   c := "newvar"
   &c := 5
   ? "assigned:", M->newvar
   &c += 1
   &c++
   ? "compound:", M->newvar, --&c, &c--, M->newvar
   c := "arr"
   &c[ 2 ] := 20
   &( "arr[3]" ) := 30
   &c[ 1 ] += 5
   ? "elements:", arr[ 1 ], arr[ 2 ], arr[ 3 ]
   ? "inside:", &( "zz := 5" ), M->zz, &( "zz++" ), M->zz, &( "++zz" ), &( "zz += 10" )
   Sub()
   ? "a callee's variable ends with it:", Kind( {|| subvar } )
   ReleaseAll()

   c := "p1"
   PRIVATE &c := 11
   c := "p2"
   PUBLIC &c
   ? "declared:", M->p1, ValType( M->p2 )
   c := " pv "
   PRIVATE &c := 3
   ? "blanks around the name:", M->pv

   c := "Str"
   ? "calls:", &c( 42, 5 ), &c.( n ), &( "Str" )( 6 )
   c := " Upper "
   ? "a name with blanks:", &c( "x" )

   b := &( "{|x| x * 2 + mv }" )
   ? "block:", ValType( b ), Eval( b, 3 )
   b := &( "{|x, y| x := x + 1, y := x * 2, { x, y } }" )
   ? "parameters:", Eval( b, 1 )[ 1 ], Eval( b, 1 )[ 2 ]
   b := &( "{|v| v[ 1 ] := 9, v[ 1 ]++ }" )
   ? "elements in a block:", Eval( b, arr ), arr[ 1 ]
   b := &( "{|x| {|y| x + y } }" )
   ? "a block in a block sees its own parameters only:", Kind( {|| Eval( Eval( b, 1 ), 2 ) } )
   cName := "World"
   ? "Hello &cName., &cName!", '&cName' + [ &cName.], "A&Q &&cName"

   d := "1+1"
   c := "&d"
   ? "nested:", &c, &( "&( '2*3' )" )
   ? "iif:", &( "IIF( .T., 1, 2 )" ), &( "IF( .F., 1, 2 )" ), &( ".T. .AND. 5" ), &( ".F. .OR. 'x'" ), ;
      &( "!( 1 > 2 )" )
   ? "send:", &( "ErrorNew():subCode" ), &( "{ 'a' => 1, 'b' => 2 }" )[ "b" ]
   ? "by reference:", &( "AFill( arr, 7 )" )[ 2 ], arr[ 3 ]
   ? "the caller:", &( "ProcName()" )

   a := { "[abc]", "123", "1.50", "-5", ".F.", "NIL", "0x10", "1e3", "'a' $ 'abc'", "1 == 1", "!.T.", ".NOT. .F.", ;
          "2 ** 3", "7 % 3", "'ab ' - 'c'", "{ 'a' => 1 }['a']", "&( '1+1' )", "StaticF()", "1 2", "x :=", ;
          "arr[1]", "arr[10]", "@arr", "a->b", "{|| zz }", "1;2", "(1,2)", "arr[1,]", "Upper(", "'abc", "1.", ;
          ".5", "1.2.3", "10 / 4", "3 ^ 2", "-arr[1]", "+1", "{}", "{||}", "M->arr", "MEMVAR->arr", "&c", ;
          "1 .AND. 2", "IIF(.T.,1)", "Len(arr)", "arr:Len", "1 = 1", "1 <> 2", "1 # 2", "1 != 2", "MyFunc()", ;
          "undefinedvar", "l", "CToD( '' )", "  mv  ", "", "mv + 'a'", "{1}" }
   c := NIL
   FOR i := 1 TO Len( a )
      ? Left( a[ i ] + Space( 20 ), 20 ), Type( a[ i ] )
      b := ErrorBlock( {| e | Break( e ) } )
      BEGIN SEQUENCE
         ?? " =>", &( a[ i ] )
      RECOVER USING b
         ?? " error", b:subCode
      END SEQUENCE
      ErrorBlock( {| e | Break( e ) } )
   NEXT
   ? "Type() calls nothing:", cnt

   ErrorBlock( {| e | QOut( "error:", e:genCode, e:subCode, e:description, e:operation, ValType( e:args ), ;
      e:canSubstitute, e:canRetry ), Break( e ) } )
   FOR EACH cKind IN { "1+", 5, "zzundef", "nofunc()", "l", "", "a b" }
      BEGIN SEQUENCE
         c := cKind
         ? &c
      END SEQUENCE
   NEXT
   FOR EACH cKind IN { "nofunc", 5, "Upper('x')" }
      BEGIN SEQUENCE
         c := cKind
         ? &c()
      END SEQUENCE
   NEXT
   BEGIN SEQUENCE
      c := "a b"
      &c := 1
   END SEQUENCE
   BEGIN SEQUENCE
      c := "1abc"
      PRIVATE &c
   END SEQUENCE
   BEGIN SEQUENCE
      ? Type( 1 )
   END SEQUENCE
   ? "substituted:", Eval( {|| ErrorBlock( {|| "sub" } ), &( "1+" ) } )
RETURN

PROCEDURE Sub()
   LOCAL cc := "subvar"
   &cc := 1
   ? "in the callee:", Kind( {|| subvar } )
RETURN

// A variable a macro creates is the function's: RELEASE ALL there reaches it.
PROCEDURE ReleaseAll()
   LOCAL cc := "made"
   &cc := 1
   RELEASE ALL
   ? "RELEASE ALL reaches a macro's variable:", Kind( {|| made } )
RETURN

FUNCTION MyFunc()
   cnt++
RETURN 1

STATIC FUNCTION StaticF()
RETURN "static"

STATIC FUNCTION Kind( bRead )
   LOCAL oldBlock := ErrorBlock( {| e | Break( e ) } ), cKind := "-"
   BEGIN SEQUENCE
      cKind := ValType( Eval( bRead ) )
   END SEQUENCE
   ErrorBlock( oldBlock )
RETURN cKind
