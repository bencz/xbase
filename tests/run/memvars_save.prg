// SAVE and RESTORE (.mem files), compared with the reference. Kind() tells a variable holding NIL ("U") from one
// that does not exist ("-"). Deliberate differences: a RESTORE whose file cannot be opened leaves the variables as
// they were (the reference releases them first); a SAVE into a missing directory is "path not found" (3) on every
// system.
MEMVAR cText, cBig, cNul, cEmpty, nInt, nDec, nNeg, nBig, dDay, dEmpty, lTrue, aList, xNil, abcdefghijklm, pub, hid, z
MEMVAR p, arr, zzzzzzzzzzzz

PROCEDURE Main()
   PUBLIC pub := "public"
   PRIVATE cText := "hello", cBig := Replicate( "a", 70000 ), cNul := "a" + Chr( 0 ) + "b", cEmpty := ""
   PRIVATE nInt := 42, nDec := 3.14159, nNeg := -7.5, nBig := 10 ^ 20, dDay := SToD( "20240229" )
   PRIVATE dEmpty := SToD( "" ), lTrue := .T., aList := { 1 }, xNil := NIL, abcdefghijklm := 1
   PRIVATE hid := "visible"
   Hidden()
   SAVE TO memvars_save_all
   SAVE ALL LIKE c* TO memvars_save_c
   SAVE TO memvars_save_n.dat ALL EXCEPT c*

   CLEAR MEMORY
   RESTORE FROM memvars_save_all
   ? "text:", cText, Len( cBig ), Len( cNul ), Asc( SubStr( cNul, 2, 1 ) ), Len( cEmpty )
   ? "numbers:", nInt, nDec, nNeg, Str( nInt ), Str( nDec ), Str( nNeg )
   ? "dates:", DToS( dDay ), Empty( dEmpty ), "logical:", lTrue
   ? "not saved:", Kind( {|| aList } ), Kind( {|| xNil } )
   ? "long name:", Kind( {|| abcdefghijklm } ), M->abcdefghij
   ? "public saved as visible:", pub, "the visible one:", hid

   CLEAR MEMORY
   RESTORE FROM memvars_save_c
   ? "like c*:", Kind( {|| cText } ), Kind( {|| nInt } )
   CLEAR MEMORY
   RESTORE FROM memvars_save_n.dat
   ? "except c*:", Kind( {|| cText } ), Kind( {|| nInt } ), Kind( {|| lTrue } )

   CLEAR MEMORY
   PUBLIC pub := "kept"
   PRIVATE nInt := 0
   Additive()
   ? "after ADDITIVE in a callee:", nInt, pub, Kind( {|| cText } )
   Replace()
   ? "after RESTORE in a callee:", Kind( {|| nInt } ), Kind( {|| pub } ), Kind( {|| cText } )
   Eval( {|| __MemRestore( "memvars_save_c", .T. ) } )
   ? "after RESTORE in a codeblock:", Kind( {|| cText } )

   PRIVATE z := 1
   ErrorBlock( {| e | Report( e ) } )
   RESTORE FROM memvars_save_none
   ? "after a failed RESTORE:", z
   SAVE TO memvars_save_no/dir/file
   ? "after a failed SAVE"
   ErrorBlock( {| e | QOut( "try", e:tries ), e:tries < 3 } )
   RESTORE FROM memvars_save_none ADDITIVE
   ? "retried"

   ErrorBlock( {| e | Break( e ) } )
   ? "no file:", Caught( {|| __MemSave() } ), Caught( {|| __MemRestore( NIL, .T. ) } )
   CLEAR MEMORY
   ViaDo()
   ? "after RESTORE through Do():", Kind( {|| cText } )
   NewNames()
RETURN

PROCEDURE ViaDo()
   Do( "__MemRestore", "memvars_save_c", .T. )
   ? "through Do():", Kind( {|| cText } )
RETURN

// An error block that restores names no module uses while an assignment reaches a missing element.
PROCEDURE NewNames()
   LOCAL a := { 1, 2, 3 }
   CLEAR MEMORY
   PRIVATE zzzzzzzzzzzz := "x" // saved as ZZZZZZZZZZ, a name no module uses
   SAVE TO memvars_save_new
   CLEAR MEMORY
   Element( @a[ 3 ], a )
RETURN

PROCEDURE Element()
   LOCAL oErr
   PARAMETERS p, arr
   ASize( arr, 1 )
   ErrorBlock( {| e | __MemRestore( "memvars_save_new", .T. ), Break( e ) } )
   BEGIN SEQUENCE
      p := 5
   RECOVER USING oErr
      ? "element gone:", oErr:subCode, Len( arr )
   END SEQUENCE
RETURN

STATIC FUNCTION Caught( bCode )
   LOCAL oErr
   BEGIN SEQUENCE
      Eval( bCode )
   RECOVER USING oErr
      RETURN LTrim( Str( oErr:subCode ) ) + " " + oErr:description
   END SEQUENCE
RETURN "no error"

PROCEDURE Hidden()
   PRIVATE hid := "hidden"
RETURN

PROCEDURE Additive()
   RESTORE FROM memvars_save_n.dat ADDITIVE
   ? "in the callee:", nInt, Kind( {|| pub } ), Kind( {|| lTrue } )
RETURN

PROCEDURE Replace()
   RESTORE FROM memvars_save_c
   ? "in the callee:", Kind( {|| nInt } ), Kind( {|| pub } ), cText
RETURN

STATIC FUNCTION Report( e )
   ? "error:", e:genCode, e:subCode, e:description, e:filename, e:osCode, e:canRetry, e:canDefault, ;
      e:canSubstitute, e:operation, ValType( e:args )
RETURN .F.

STATIC FUNCTION Kind( bRead )
   LOCAL oldBlock := ErrorBlock( {| e | Break( e ) } ), cKind := "-"
   BEGIN SEQUENCE
      cKind := ValType( Eval( bRead ) )
   END SEQUENCE
   ErrorBlock( oldBlock )
RETURN cKind
