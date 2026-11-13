// Scopes as a matrix: each kind of member (VAR, READONLY VAR, METHOD, ACCESS/ASSIGN, CLASSVAR) in each scope
// (EXPORTED, PROTECTED, HIDDEN), read, assigned or called from outside, from a method of the class, of a
// subclass and of an unrelated class, from a FRIEND CLASS, from a codeblock a method made (evaluated outside),
// on an object of the class and of its subclass, by a compiled send.
// Each cell is the value (x when assigned) or the error's subCode (41 hidden, 42 protected). A READONLY variable
// is assigned as if one scope narrower; a HIDDEN READONLY one by its class's methods and friends (the reference
// lets nothing assign it).
// Columns: vE vE= vR vR= vP vP= vPR vPR= vH vH= vHR vHR= mE() mP() mH() aP aP= aH cE cE= cR cR= cP cPR= cH cH= cHR=

PROCEDURE Main()
   LOCAL aObj, o, lMacro
   ErrorBlock( {| x | Break( x ) } )
   FOR EACH lMacro IN { .F. }
      ? iif( lMacro, "macro sends", "compiled sends" )
      aObj := { TA():New(), TB():New() }
      FOR EACH o IN aObj
         ? "outside", o:ClassName(), Outside( o, lMacro )
         ? "TA method", o:ClassName(), TA():New():Try( o, lMacro )
         ? "TB method", o:ClassName(), TB():New():Try( o, lMacro )
         ? "TC method", o:ClassName(), TC():New():Try( o, lMacro )
         ? "friend", o:ClassName(), TF():New():Try( o, lMacro )
         ? "block", o:ClassName(), RunBlocks( TA():New():Blk( lMacro ), o )
      NEXT
   NEXT
RETURN

STATIC FUNCTION Shown( x )
RETURN iif( ValType( x ) == "C", x, ValType( x ) )

STATIC FUNCTION Outside( o, lMacro )
   LOCAL c := "", n, e
   FOR n := 1 TO 27
      BEGIN SEQUENCE
         DO CASE
         CASE n == 1
            c += " " + Shown( o:vE )
         CASE n == 2
            c += " " + Shown( o:vE := "x" )
         CASE n == 3
            c += " " + Shown( o:vR )
         CASE n == 4
            c += " " + Shown( o:vR := "x" )
         CASE n == 5
            c += " " + Shown( o:vP )
         CASE n == 6
            c += " " + Shown( o:vP := "x" )
         CASE n == 7
            c += " " + Shown( o:vPR )
         CASE n == 8
            c += " " + Shown( o:vPR := "x" )
         CASE n == 9
            c += " " + Shown( o:vH )
         CASE n == 10
            c += " " + Shown( o:vH := "x" )
         CASE n == 11
            c += " " + Shown( o:vHR )
         CASE n == 12
            c += " " + Shown( o:vHR := "x" )
         CASE n == 13
            c += " " + Shown( o:mE() )
         CASE n == 14
            c += " " + Shown( o:mP() )
         CASE n == 15
            c += " " + Shown( o:mH() )
         CASE n == 16
            c += " " + Shown( o:aP )
         CASE n == 17
            c += " " + Shown( o:aP := "x" )
         CASE n == 18
            c += " " + Shown( o:aH )
         CASE n == 19
            c += " " + Shown( o:cE )
         CASE n == 20
            c += " " + Shown( o:cE := "x" )
         CASE n == 21
            c += " " + Shown( o:cR )
         CASE n == 22
            c += " " + Shown( o:cR := "x" )
         CASE n == 23
            c += " " + Shown( o:cP )
         CASE n == 24
            c += " " + Shown( o:cPR := "x" )
         CASE n == 25
            c += " " + Shown( o:cH )
         CASE n == 26
            c += " " + Shown( o:cH := "x" )
         CASE n == 27
            c += " " + Shown( o:cHR := "x" )
         ENDCASE
      RECOVER USING e
         c += " " + LTrim( Str( e:subCode ) )
      END SEQUENCE
   NEXT
RETURN c

STATIC FUNCTION RunBlocks( aBlocks, o )
   LOCAL c := "", b, e
   FOR EACH b IN aBlocks
      BEGIN SEQUENCE
         c += " " + Shown( Eval( b, o ) )
      RECOVER USING e
         c += " " + LTrim( Str( e:subCode ) )
      END SEQUENCE
   NEXT
RETURN c

CLASS TA
   VAR vE INIT "vE"
   VAR vR INIT "vR" READONLY
   METHOD mE() INLINE "mE"
   CLASSVAR cE INIT "cE"
   CLASSVAR cR INIT "cR" READONLY
   METHOD Try( o, lMacro )
   METHOD Blk( lMacro )
   FRIEND CLASS TF
   PROTECTED:
   VAR vP INIT "vP"
   VAR vPR INIT "vPR" READONLY
   METHOD mP() INLINE "mP"
   ACCESS aP INLINE ::vP + "+"
   ASSIGN aP( v ) INLINE ::vP := v
   CLASSVAR cP INIT "cP"
   CLASSVAR cPR INIT "cPR" READONLY
   HIDDEN:
   VAR vH INIT "vH"
   METHOD mH() INLINE "mH"
   ACCESS aH INLINE ::vH + "+"
   CLASSVAR cH INIT "cH"
   CLASSVAR cHR INIT "cHR" READONLY
   VAR vHR INIT "vHR" READONLY
ENDCLASS

CLASS TB INHERIT TA
   METHOD Try( o, lMacro )
ENDCLASS

CLASS TC
   METHOD Try( o, lMacro )
ENDCLASS

CLASS TF
   METHOD Try( o, lMacro )
ENDCLASS

// Blocks made in a method of TA, one per column, evaluated outside.
METHOD Blk( lMacro ) CLASS TA
RETURN { {| o | o:vE }, {| o | o:vE := "x" }, {| o | o:vR }, ;
         {| o | o:vR := "x" }, {| o | o:vP }, {| o | o:vP := "x" }, ;
         {| o | o:vPR }, {| o | o:vPR := "x" }, {| o | o:vH }, ;
         {| o | o:vH := "x" }, {| o | o:vHR }, {| o | o:vHR := "x" }, ;
         {| o | o:mE() }, {| o | o:mP() }, {| o | o:mH() }, ;
         {| o | o:aP }, {| o | o:aP := "x" }, {| o | o:aH }, ;
         {| o | o:cE }, {| o | o:cE := "x" }, {| o | o:cR }, ;
         {| o | o:cR := "x" }, {| o | o:cP }, {| o | o:cPR := "x" }, ;
         {| o | o:cH }, {| o | o:cH := "x" }, {| o | o:cHR := "x" } }

METHOD Try( o, lMacro ) CLASS TA
   LOCAL c := "", n, e
   FOR n := 1 TO 27
      BEGIN SEQUENCE
         DO CASE
         CASE n == 1
            c += " " + Shown( o:vE )
         CASE n == 2
            c += " " + Shown( o:vE := "x" )
         CASE n == 3
            c += " " + Shown( o:vR )
         CASE n == 4
            c += " " + Shown( o:vR := "x" )
         CASE n == 5
            c += " " + Shown( o:vP )
         CASE n == 6
            c += " " + Shown( o:vP := "x" )
         CASE n == 7
            c += " " + Shown( o:vPR )
         CASE n == 8
            c += " " + Shown( o:vPR := "x" )
         CASE n == 9
            c += " " + Shown( o:vH )
         CASE n == 10
            c += " " + Shown( o:vH := "x" )
         CASE n == 11
            c += " " + Shown( o:vHR )
         CASE n == 12
            c += " " + Shown( o:vHR := "x" )
         CASE n == 13
            c += " " + Shown( o:mE() )
         CASE n == 14
            c += " " + Shown( o:mP() )
         CASE n == 15
            c += " " + Shown( o:mH() )
         CASE n == 16
            c += " " + Shown( o:aP )
         CASE n == 17
            c += " " + Shown( o:aP := "x" )
         CASE n == 18
            c += " " + Shown( o:aH )
         CASE n == 19
            c += " " + Shown( o:cE )
         CASE n == 20
            c += " " + Shown( o:cE := "x" )
         CASE n == 21
            c += " " + Shown( o:cR )
         CASE n == 22
            c += " " + Shown( o:cR := "x" )
         CASE n == 23
            c += " " + Shown( o:cP )
         CASE n == 24
            c += " " + Shown( o:cPR := "x" )
         CASE n == 25
            c += " " + Shown( o:cH )
         CASE n == 26
            c += " " + Shown( o:cH := "x" )
         CASE n == 27
            c += " " + Shown( o:cHR := "x" )
         ENDCASE
      RECOVER USING e
         c += " " + LTrim( Str( e:subCode ) )
      END SEQUENCE
   NEXT
RETURN c

METHOD Try( o, lMacro ) CLASS TB
   LOCAL c := "", n, e
   FOR n := 1 TO 27
      BEGIN SEQUENCE
         DO CASE
         CASE n == 1
            c += " " + Shown( o:vE )
         CASE n == 2
            c += " " + Shown( o:vE := "x" )
         CASE n == 3
            c += " " + Shown( o:vR )
         CASE n == 4
            c += " " + Shown( o:vR := "x" )
         CASE n == 5
            c += " " + Shown( o:vP )
         CASE n == 6
            c += " " + Shown( o:vP := "x" )
         CASE n == 7
            c += " " + Shown( o:vPR )
         CASE n == 8
            c += " " + Shown( o:vPR := "x" )
         CASE n == 9
            c += " " + Shown( o:vH )
         CASE n == 10
            c += " " + Shown( o:vH := "x" )
         CASE n == 11
            c += " " + Shown( o:vHR )
         CASE n == 12
            c += " " + Shown( o:vHR := "x" )
         CASE n == 13
            c += " " + Shown( o:mE() )
         CASE n == 14
            c += " " + Shown( o:mP() )
         CASE n == 15
            c += " " + Shown( o:mH() )
         CASE n == 16
            c += " " + Shown( o:aP )
         CASE n == 17
            c += " " + Shown( o:aP := "x" )
         CASE n == 18
            c += " " + Shown( o:aH )
         CASE n == 19
            c += " " + Shown( o:cE )
         CASE n == 20
            c += " " + Shown( o:cE := "x" )
         CASE n == 21
            c += " " + Shown( o:cR )
         CASE n == 22
            c += " " + Shown( o:cR := "x" )
         CASE n == 23
            c += " " + Shown( o:cP )
         CASE n == 24
            c += " " + Shown( o:cPR := "x" )
         CASE n == 25
            c += " " + Shown( o:cH )
         CASE n == 26
            c += " " + Shown( o:cH := "x" )
         CASE n == 27
            c += " " + Shown( o:cHR := "x" )
         ENDCASE
      RECOVER USING e
         c += " " + LTrim( Str( e:subCode ) )
      END SEQUENCE
   NEXT
RETURN c

METHOD Try( o, lMacro ) CLASS TC
   LOCAL c := "", n, e
   FOR n := 1 TO 27
      BEGIN SEQUENCE
         DO CASE
         CASE n == 1
            c += " " + Shown( o:vE )
         CASE n == 2
            c += " " + Shown( o:vE := "x" )
         CASE n == 3
            c += " " + Shown( o:vR )
         CASE n == 4
            c += " " + Shown( o:vR := "x" )
         CASE n == 5
            c += " " + Shown( o:vP )
         CASE n == 6
            c += " " + Shown( o:vP := "x" )
         CASE n == 7
            c += " " + Shown( o:vPR )
         CASE n == 8
            c += " " + Shown( o:vPR := "x" )
         CASE n == 9
            c += " " + Shown( o:vH )
         CASE n == 10
            c += " " + Shown( o:vH := "x" )
         CASE n == 11
            c += " " + Shown( o:vHR )
         CASE n == 12
            c += " " + Shown( o:vHR := "x" )
         CASE n == 13
            c += " " + Shown( o:mE() )
         CASE n == 14
            c += " " + Shown( o:mP() )
         CASE n == 15
            c += " " + Shown( o:mH() )
         CASE n == 16
            c += " " + Shown( o:aP )
         CASE n == 17
            c += " " + Shown( o:aP := "x" )
         CASE n == 18
            c += " " + Shown( o:aH )
         CASE n == 19
            c += " " + Shown( o:cE )
         CASE n == 20
            c += " " + Shown( o:cE := "x" )
         CASE n == 21
            c += " " + Shown( o:cR )
         CASE n == 22
            c += " " + Shown( o:cR := "x" )
         CASE n == 23
            c += " " + Shown( o:cP )
         CASE n == 24
            c += " " + Shown( o:cPR := "x" )
         CASE n == 25
            c += " " + Shown( o:cH )
         CASE n == 26
            c += " " + Shown( o:cH := "x" )
         CASE n == 27
            c += " " + Shown( o:cHR := "x" )
         ENDCASE
      RECOVER USING e
         c += " " + LTrim( Str( e:subCode ) )
      END SEQUENCE
   NEXT
RETURN c

METHOD Try( o, lMacro ) CLASS TF
   LOCAL c := "", n, e
   FOR n := 1 TO 27
      BEGIN SEQUENCE
         DO CASE
         CASE n == 1
            c += " " + Shown( o:vE )
         CASE n == 2
            c += " " + Shown( o:vE := "x" )
         CASE n == 3
            c += " " + Shown( o:vR )
         CASE n == 4
            c += " " + Shown( o:vR := "x" )
         CASE n == 5
            c += " " + Shown( o:vP )
         CASE n == 6
            c += " " + Shown( o:vP := "x" )
         CASE n == 7
            c += " " + Shown( o:vPR )
         CASE n == 8
            c += " " + Shown( o:vPR := "x" )
         CASE n == 9
            c += " " + Shown( o:vH )
         CASE n == 10
            c += " " + Shown( o:vH := "x" )
         CASE n == 11
            c += " " + Shown( o:vHR )
         CASE n == 12
            c += " " + Shown( o:vHR := "x" )
         CASE n == 13
            c += " " + Shown( o:mE() )
         CASE n == 14
            c += " " + Shown( o:mP() )
         CASE n == 15
            c += " " + Shown( o:mH() )
         CASE n == 16
            c += " " + Shown( o:aP )
         CASE n == 17
            c += " " + Shown( o:aP := "x" )
         CASE n == 18
            c += " " + Shown( o:aH )
         CASE n == 19
            c += " " + Shown( o:cE )
         CASE n == 20
            c += " " + Shown( o:cE := "x" )
         CASE n == 21
            c += " " + Shown( o:cR )
         CASE n == 22
            c += " " + Shown( o:cR := "x" )
         CASE n == 23
            c += " " + Shown( o:cP )
         CASE n == 24
            c += " " + Shown( o:cPR := "x" )
         CASE n == 25
            c += " " + Shown( o:cH )
         CASE n == 26
            c += " " + Shown( o:cH := "x" )
         CASE n == 27
            c += " " + Shown( o:cHR := "x" )
         ENDCASE
      RECOVER USING e
         c += " " + LTrim( Str( e:subCode ) )
      END SEQUENCE
   NEXT
RETURN c

