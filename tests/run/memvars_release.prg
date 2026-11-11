// RELEASE, RELEASE ALL [LIKE | EXCEPT], CLEAR MEMORY and DECLARE, compared with the reference. Kind() tells a
// variable holding NIL ("U") from one that does not exist ("-"). A deliberate difference: a skeleton that starts with
// `*` is matched whole (`*B`: the names ending in B), where the reference takes it for every name.
MEMVAR a, b, p, pub, xa1, xa2, xab, xb1, x, ya, yb2, d1, d2, v1, v2, q, z

PROCEDURE Main()
   LOCAL n := 1
   PRIVATE a := 1, b := 2
   PUBLIC pub := "P"
   RELEASE a
   ? "own private:", Kind( {|| a } )
   a := 5
   ? "assigned again:", a
   ReleaseCallers()
   ? "caller's private:", Kind( {|| b } )
   RELEASE pub
   ? "public:", Kind( {|| pub } )
   PUBLIC p := 3
   HideAndRelease()
   ? "public hidden by a private:", p
   RELEASE nothing, a
   ? "no such variable:", Kind( {|| a } )

   PRIVATE xa1 := 1, xa2 := 2, xab := 3, xb1 := 4, x := 5, ya := 6, yb2 := 7
   ReleaseAll()
   ? "only the caller's:", Kind( {|| xa1 } )
   RELEASE ALL LIKE x?1
   ? "like x?1:", Kind( {|| xa1 } ), Kind( {|| xab } ), Kind( {|| xb1 } )
   RELEASE ALL LIKE x
   ? "like x:", Kind( {|| x } ), Kind( {|| xa2 } )
   RELEASE ALL EXCEPT ?a*
   ? "except ?a*:", Kind( {|| xa2 } ), Kind( {|| xab } ), Kind( {|| ya } ), Kind( {|| yb2 } )
   PRIVATE xab := 8
   RELEASE ALL LIKE *B
   ? "like *b:", Kind( {|| xab } ), Kind( {|| ya } )
   RELEASE ALL
   ? "all:", Kind( {|| ya } )

   PRIVATE a := 1
   ByRef( @a )
   ? "passed @a:", a
   Params( @n )
   ? "PARAMETERS by reference:", n
   ParamsAll( @n )
   ? "after RELEASE ALL:", n
   PRIVATE z := 1
   Params( @z )
   ? "PARAMETERS, a private by reference:", z
   PUBLIC pub := 1
   Params( @pub )
   ? "PARAMETERS, a public by reference:", pub
   z := 1
   ParamsAll( @z )
   ? "after RELEASE ALL:", z

   DECLARE d1 := 7, d2[ 3 ]
   DECL d1
   ? "declare:", d1, Len( d2 )
   Clear()
   ? "after clear memory:", Kind( {|| v1 } ), Kind( {|| v2 } ), Kind( {|| d1 } ), Kind( {|| b } )
   v1 := "new"
   ? "created again:", v1
RETURN

PROCEDURE ReleaseCallers()
   RELEASE b
   ? "in the callee:", Kind( {|| b } )
RETURN

PROCEDURE HideAndRelease()
   PRIVATE p := 4
   RELEASE p
   ? "private hiding a public:", Kind( {|| p } )
RETURN

PROCEDURE ReleaseAll()
   PRIVATE q := 9
   RELEASE ALL
   ? "release all:", Kind( {|| q } ), Kind( {|| xa1 } )
   Eval( {|| q := 1, xa2 := 2 } )
RETURN

PROCEDURE ByRef( r )
   RELEASE a
   ? "the reference sees NIL:", r
RETURN

PROCEDURE Params()
   PARAMETERS q
   RELEASE q
   ? "parameter:", q
   q := 9
RETURN

PROCEDURE ParamsAll()
   PARAMETERS q
   RELEASE ALL
   q := 10
RETURN

PROCEDURE Clear()
   PRIVATE v1 := 1
   PUBLIC v2 := 2
   CLEAR MEMORY
   ? "in clear:", Kind( {|| v1 } ), Kind( {|| v2 } )
   v1 := 3
   PRIVATE v1 := 4
   ? "private again:", v1
RETURN

STATIC FUNCTION Kind( bRead )
   LOCAL oldBlock := ErrorBlock( {| e | Break( e ) } ), cKind := "-"
   BEGIN SEQUENCE
      cKind := ValType( Eval( bRead ) )
   END SEQUENCE
   ErrorBlock( oldBlock )
RETURN cKind
