// Transform(), compared with the reference: no picture, string templates, number templates (digits, `$`, `*`, the
// point and thousands separators, fields too narrow), the functions @B @C @X @( @) @L @0 @R @! @D @E @Z @S, dates
// under SET DATE and SET CENTURY, logicals, and the argument errors. Results are shown between brackets.

PROCEDURE Main()
   LOCAL e
   ? "plain:", Box( "abc", "" ), Box( 123 ), Box( -1.5 ), Box( 1 / 3 ), Box( .T. ), Box( SToD( "20240229" ), NIL )
   ? "string:", Box( "abcdef", "!!!" ), Box( "abcdef", "@! XX-X" ), Box( "hello", "@R XX-XXX" ), ;
      Box( "hi", "@R XXXX" ), Box( "yes", "YYY" ), Box( "5551234", "@R (999) 999-9999" ), Box( "abcdef", "@!" ), ;
      Box( "abcdef", "@S3" ), Box( "  abc", "@B" ), Box( "abc", "@Z" ), Box( "12/31/99", "@E" )
   ? "digits:", Box( 1234.5, "9999.99" ), Box( 1234.5, "999.9" ), Box( -12.3, "9999.99" ), Box( 0.5, ".99" ), ;
      Box( 1.5, ".99" ), Box( 7, "##" ), Box( 1234567, "9,999,999" ), Box( 12, "9,999,999" ), ;
      Box( -12, "9,999,999" ), Box( 42, "$$$,$$9" ), Box( 42, "***,**9" ), Box( 12.345, "99.9" )
   ? "functions:", Box( -42, "@( 9999" ), Box( -42, "@) 9999" ), Box( 42, "@C 9999" ), Box( -42, "@X 9999" ), ;
      Box( 42, "@B 9999" ), Box( 42, "@L 9999" ), Box( -42, "@0 99999" ), Box( 0, "@Z 9999" ), ;
      Box( 1234.5, "@E 9,999.99" ), Box( 1234.5, "@E" ), Box( -1234, "@(" ), Box( 123456, "@S3" ), ;
      Box( 20240229, "@D" ), Box( -99999, "@( 999" )
   SET DATE BRITISH
   ? "british:", Box( SToD( "20240229" ) ), Box( SToD( "20240229" ), "@E" ), Box( SToD( "20240229" ), "@R" ), ;
      Box( CToD( "" ), "@D" )
   SET CENTURY ON
   ? "century:", Box( SToD( "20240229" ) ), Box( SToD( "20240229" ), "@E" ), Box( "29022024", "@D" )
   SET DATE ANSI
   SET CENTURY OFF
   ? "ansi:", Box( SToD( "20240229" ), "@E" ), Box( SToD( "20240229" ), "@B" ), Box( .T., "@D" )
   ? "logical:", Box( .T., "Y" ), Box( .F., "L" ), Box( .T., "@R Y/N" ), Box( .F., "@R [L]" ), Box( .T., "x" ), ;
      Box( .F., "#" )
   ErrorBlock( {| oErr | Break( oErr ) } )
   FOR EACH e IN { {}, NIL, {|| 1 } }
      BEGIN SEQUENCE
         ? Transform( e, "9" )
      RECOVER USING e
         ? "error:", e:subCode, e:operation
      END SEQUENCE
   NEXT
   BEGIN SEQUENCE
      ? Transform( 5, 5 )
   RECOVER USING e
      ? "error:", e:subCode, e:operation
   END SEQUENCE
RETURN

STATIC FUNCTION Box( xValue, cPicture )
RETURN "[" + Transform( xValue, cPicture ) + "]"
