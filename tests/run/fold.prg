// Expressions on literals are evaluated at compile time with Clipper's rules
// for the result's type, width and decimals.
PROCEDURE Main()
   ? 10 / 5, 10 / 4, 7 % 3, 7.5 % 2, 2 ^ 10
   ? 1 + 2 * 3 - 4 / 2, 6 / -1, -7 % 3
   ? 1.5 + 1.25, 1.5 * 1.25, 3 * 1.5, 1.50 - 1
   ? 9223372036854775807 + 1, 4611686018427387904 * 2
   ? -5, - -5, -( 2.5 )
   ? 1 < 2, 2.5 >= 2.5, .T. > .F., 1 == 1.0, 3 != 3
   ? "ab" + "cd", "ab  " - "cd", "ab" == "ab", "" = "", "b" $ "abc"
   ? .T. .AND. .F., .F. .OR. .T., .NOT. .T., IIf( .T., "yes", "no" )
   ? Len( "hello" ), Len( "" )
   ? 999999999 + 1, 2000000000 / 2, 3000000000 * 3, 12345678901 + 0, -( -1000000000 ), -1000000000 + 0
   ? 0000000001, 1000000000, 123456789012
   RETURN
