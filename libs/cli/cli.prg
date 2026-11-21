// The command line of a program (docs/libraries.md#cli): the options it declares (--name value, --name=value,
// -x value), its flags (--name, -x) and its positional arguments, read from the arguments it received, with the help
// text made from the declarations.

CLASS CommandLine
   METHOD New( cProgram, cDescription )
   METHOD AddOption( cName, cShort, cHelp, cDefault )
   METHOD AddFlag( cName, cShort, cHelp )
   METHOD AddArgument( cName, cHelp, lRequired )
   METHOD Parse( aArgs )
   METHOD Value( cName )
   METHOD IsSet( cName )
   METHOD Rest() INLINE AClone( ::aRest )
   METHOD ErrorText() INLINE ::cError
   METHOD HelpRequested() INLINE ::lHelp
   METHOD Usage()
   HIDDEN:
   VAR cProgram
   VAR cDescription
   VAR aOptions      // each { cName, cShort, cHelp, cDefault, lFlag }
   VAR aArguments    // each { cName, cHelp, lRequired }
   VAR hValues       // name -> the value given (.T. for a flag)
   VAR aRest
   VAR cError
   VAR lHelp
   METHOD Declare( cName, cShort, cHelp, cDefault, lFlag )
   METHOD Option( cArg )
   METHOD Fail( cText )
ENDCLASS

METHOD New( cProgram, cDescription ) CLASS CommandLine
   ::cProgram := cProgram
   ::cDescription := IIf( cDescription == NIL, "", cDescription )
   ::aOptions := {}
   ::aArguments := {}
   ::hValues := { => }
   ::aRest := {}
   ::cError := ""
   ::lHelp := .F.
RETURN Self

// An option takes a value; cShort ("" or NIL: none) is its one-letter name; cDefault is its value when not given.
METHOD AddOption( cName, cShort, cHelp, cDefault ) CLASS CommandLine
RETURN ::Declare( cName, cShort, cHelp, cDefault, .F. )

METHOD AddFlag( cName, cShort, cHelp ) CLASS CommandLine
RETURN ::Declare( cName, cShort, cHelp, NIL, .T. )

// Positional arguments are taken in the order declared; the ones after them are Rest().
METHOD AddArgument( cName, cHelp, lRequired ) CLASS CommandLine
   AAdd( ::aArguments, { cName, IIf( cHelp == NIL, "", cHelp ), lRequired == .T. } )
RETURN Self

METHOD Declare( cName, cShort, cHelp, cDefault, lFlag ) CLASS CommandLine
   AAdd( ::aOptions, { cName, IIf( cShort == NIL, "", cShort ), IIf( cHelp == NIL, "", cHelp ), cDefault, lFlag } )
RETURN Self

// Reads the arguments: .F. when they are wrong (ErrorText() says why) or --help (-h) is among them
// (HelpRequested()). An argument after "--" is positional, whatever it starts with.
METHOD Parse( aArgs ) CLASS CommandLine
   LOCAL n := 1, nArgument := 0, lOptions := .T., cArg, aOption, cValue, nAt
   ::hValues := { => }
   ::aRest := {}
   ::cError := ""
   ::lHelp := .F.
   DO WHILE n <= Len( aArgs )
      cArg := aArgs[ n++ ]
      IF lOptions .AND. cArg == "--"
         lOptions := .F.
      ELSEIF lOptions .AND. ( cArg == "--help" .OR. cArg == "-h" )
         ::lHelp := .T.
         RETURN .F.
      ELSEIF lOptions .AND. Left( cArg, 1 ) == "-" .AND. Len( cArg ) > 1
         nAt := At( "=", cArg )
         aOption := ::Option( IIf( nAt > 0, Left( cArg, nAt - 1 ), cArg ) )
         IF aOption == NIL
            RETURN ::Fail( "unknown option " + IIf( nAt > 0, Left( cArg, nAt - 1 ), cArg ) )
         ENDIF
         IF aOption[ 5 ]
            IF nAt > 0
               RETURN ::Fail( "the flag --" + aOption[ 1 ] + " takes no value" )
            ENDIF
            ::hValues[ aOption[ 1 ] ] := .T.
         ELSE
            IF nAt > 0
               cValue := SubStr( cArg, nAt + 1 )
            ELSEIF n <= Len( aArgs )
               cValue := aArgs[ n++ ]
            ELSE
               RETURN ::Fail( "the option --" + aOption[ 1 ] + " needs a value" )
            ENDIF
            ::hValues[ aOption[ 1 ] ] := cValue
         ENDIF
      ELSEIF ++nArgument <= Len( ::aArguments )
         ::hValues[ ::aArguments[ nArgument ][ 1 ] ] := cArg
      ELSE
         AAdd( ::aRest, cArg )
      ENDIF
   ENDDO
   FOR n := nArgument + 1 TO Len( ::aArguments )
      IF ::aArguments[ n ][ 3 ]
         RETURN ::Fail( "missing <" + ::aArguments[ n ][ 1 ] + ">" )
      ENDIF
   NEXT
RETURN .T.

// The declared option (by --name or -x) an argument names, else NIL.
METHOD Option( cArg ) CLASS CommandLine
   LOCAL aOption
   FOR EACH aOption IN ::aOptions
      IF ( Left( cArg, 2 ) == "--" .AND. SubStr( cArg, 3 ) == aOption[ 1 ] ) .OR. ;
         ( ! Empty( aOption[ 2 ] ) .AND. cArg == "-" + aOption[ 2 ] )
         RETURN aOption
      ENDIF
   NEXT
RETURN NIL

METHOD Fail( cText ) CLASS CommandLine
   ::cError := cText
RETURN .F.

// An option's value (its default when not given), a flag's .T. or .F., a positional argument's value (NIL when not
// given); NIL for a name not declared.
METHOD Value( cName ) CLASS CommandLine
   LOCAL aOption
   IF cName $ ::hValues
      RETURN ::hValues[ cName ]
   ENDIF
   FOR EACH aOption IN ::aOptions
      IF aOption[ 1 ] == cName
         RETURN IIf( aOption[ 5 ], .F., aOption[ 4 ] )
      ENDIF
   NEXT
RETURN NIL

// Whether an option, a flag or a positional argument was given.
METHOD IsSet( cName ) CLASS CommandLine
RETURN cName $ ::hValues

// The help text: the usage line, the description, the arguments and the options, one a line.
METHOD Usage() CLASS CommandLine
   LOCAL cText := "usage: " + ::cProgram, aLines := {}, nWidth := 0, aLine, aOption, aArgument
   IF ! Empty( ::aOptions )
      cText += " [options]"
   ENDIF
   FOR EACH aArgument IN ::aArguments
      cText += " " + IIf( aArgument[ 3 ], "<" + aArgument[ 1 ] + ">", "[<" + aArgument[ 1 ] + ">]" )
      AAdd( aLines, { "<" + aArgument[ 1 ] + ">", aArgument[ 2 ] } )
   NEXT
   FOR EACH aOption IN ::aOptions
      AAdd( aLines, { IIf( Empty( aOption[ 2 ] ), "    ", "-" + aOption[ 2 ] + ", " ) + "--" + aOption[ 1 ] + ;
                      IIf( aOption[ 5 ], "", " <value>" ), ;
                      aOption[ 3 ] + IIf( aOption[ 4 ] == NIL, "", " (default: " + aOption[ 4 ] + ")" ) } )
   NEXT
   AAdd( aLines, { "-h, --help", "shows this help" } )
   FOR EACH aLine IN aLines
      nWidth := Max( nWidth, Len( aLine[ 1 ] ) )
   NEXT
   IF ! Empty( ::cDescription )
      cText += Chr( 10 ) + ::cDescription
   ENDIF
   FOR EACH aLine IN aLines
      cText += Chr( 10 ) + "  " + PadR( aLine[ 1 ], nWidth ) + "  " + aLine[ 2 ]
   NEXT
RETURN cText
