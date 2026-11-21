// Shows the most frequent words of text files: a word is a run of letters (IsAlpha), counted with or without letter
// case.
PROCEDURE Main( ... )
   LOCAL oCmd := CommandLine():New( "wordfreq", "Shows the most frequent words of text files." )
   LOCAL hCount := { => }, aFiles, cFile, aWords, nTop, aWord, lFold
   oCmd:AddOption( "top", "n", "how many words to show", "10" )
   oCmd:AddFlag( "ignore-case", "i", "counts words without letter case" )
   oCmd:AddArgument( "file", "a text file", .T. )
   IF ! oCmd:Parse( { ... } )
      IF ! oCmd:HelpRequested()
         OutErr( "wordfreq: " + oCmd:ErrorText() + Chr( 10 ) )
         ErrorLevel( 2 )
      ENDIF
      ? oCmd:Usage()
      RETURN
   ENDIF
   nTop := Val( oCmd:Value( "top" ) )
   lFold := oCmd:Value( "ignore-case" )
   aFiles := { oCmd:Value( "file" ) }
   AEval( oCmd:Rest(), {| c | AAdd( aFiles, c ) } )
   FOR EACH cFile IN aFiles
      IF ! File( cFile )
         OutErr( "wordfreq: cannot read " + cFile + Chr( 10 ) )
         ErrorLevel( 1 )
         RETURN
      ENDIF
      Count( MemoRead( cFile ), lFold, hCount )
   NEXT
   aWords := {}
   HEval( hCount, {| k, v | AAdd( aWords, { k, v } ) } )
   ASort( aWords,,, {| a, b | a[ 2 ] > b[ 2 ] .OR. ( a[ 2 ] == b[ 2 ] .AND. a[ 1 ] < b[ 1 ] ) } )
   ? LTrim( Str( Len( aWords ) ) ), "different words"
   FOR EACH aWord IN aWords
      IF --nTop < 0
         EXIT
      ENDIF
      ? PadL( aWord[ 2 ], 6 ), aWord[ 1 ]
   NEXT
RETURN

STATIC PROCEDURE Count( cText, lFold, hCount )
   LOCAL c, cWord := ""
   FOR EACH c IN cText + " "
      IF IsAlpha( c )
         cWord += c
      ELSEIF ! cWord == ""
         IF lFold
            cWord := Lower( cWord )
         ENDIF
         hCount[ cWord ] := HGetDef( hCount, cWord, 0 ) + 1
         cWord := ""
      ENDIF
   NEXT
RETURN
